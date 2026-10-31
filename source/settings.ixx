module;

#include <common.hxx>
#include <shlobj.h>
#include <d3dx9.h>
#include <psapi.h>
#include <span>
#include <deque>
#include <atomic>
#include <stdexcept>
#include <asi_settings/asi_settings.hpp>
#include <upscaler/graphics_api.h>

export module settings;

import common;
import comvars;
import d3dx9_43;
import gxtloader;
import natives;
import timecycext;

namespace SettingsTables
{
    // These are the XML parser's native layouts, not C++ containers. The game
    // allocates and frees their elements through its own allocator.
    template<class T> struct Array
    {
        T* data = nullptr;
        uint16_t count = 0;
        uint16_t capacity = 0;
    };

    struct DisplayValue
    {
        char text[16]{};
        int32_t action = 0;
        int32_t value = 0;
    };

    struct Display
    {
        int32_t id = 0;
        Array<DisplayValue> values;
    };

    struct Option
    {
        uint8_t action = 0;
        char label[16]{};
        uint8_t padding = 0;
        int16_t preference = 0;
        uint8_t scaler = 0;
        uint8_t display = 100;
    };

    struct Screen
    {
        char header[16]{};
        Array<Option> options;
    };

    static_assert(sizeof(DisplayValue) == 24);
    static_assert(sizeof(Display) == 12);
    static_assert(sizeof(Option) == 22);
    static_assert(offsetof(Option, preference) == 18);
    static_assert(sizeof(Screen) == 24);

    // Preference IDs are signed 16-bit fields in Option. Display IDs 100 and
    // above are reserved for NONE, SLIDER, numbers, etc. in the renderer.
    constexpr size_t PreferenceCapacity = 32768;
    constexpr size_t DisplayCapacity = 100;
    inline std::array<int32_t, PreferenceCapacity> scalers{};
    inline std::array<Display, DisplayCapacity> displays{};

    // The 1.1.2.0 array helpers take their array in ESI, with no stack arguments.
    static __declspec(naked) void* __fastcall AppendLegacy(void* array, void* function)
    {
        __asm
        {
            push esi
            mov esi, ecx
            call edx
            pop esi
            ret
        }
    }
}

namespace CText
{
    using CText = void;
    CText* g_text = nullptr;

    const wchar_t* (__fastcall* Get)(CText* text, void* edx, const char* key);

    // Texts of code-defined menu entries: enum values and submenu rows, longer than
    // the 15-byte label fields. Kept apart from gxtEntries, which reloads clear.
    std::unordered_map<uint32_t, std::wstring> menuTexts;

    const wchar_t* FindMenuText(uint32_t hash)
    {
        if (auto it = menuTexts.find(hash); it != menuTexts.end())
            return it->second.c_str();
        if (auto it = gxtEntries.find(hash); it != gxtEntries.end())
            return it->second.c_str();
        return nullptr;
    }

    SafetyHookInline shGetText{};
    const wchar_t* __fastcall getText(CText* text, void* edx, const char* key)
    {
        if (auto found = FindMenuText(GetHash(key)))
            return found;

        return shGetText.fastcall<const wchar_t*>(text, edx, key);
    }

    SafetyHookInline shGetTextByKey{};
    const wchar_t* __fastcall getTextByKey(CText* text, void* edx, uint32_t hash, int a3)
    {
        if (auto found = FindMenuText(hash))
            return found;

        return shGetTextByKey.fastcall<const wchar_t*>(text, edx, hash, a3);
    }

    SafetyHookInline shDoesTextLabelExist{};
    char __fastcall doesTextLabelExist(CText* text, void* edx, const char* key)
    {
        if (FindMenuText(GetHash(key)))
            return 1;

        return shDoesTextLabelExist.fastcall<char>(text, edx, key);
    }

    export const wchar_t* getText(const char* key)
    {
        return Get(g_text, nullptr, key);
    }

    export bool hasViceCityStrings()
    {
        auto COL4_17 = doesTextLabelExist(g_text, 0, "COL4_17");
        auto ROK3_1 = doesTextLabelExist(g_text, 0, "ROK3_1");

        if (COL4_17 && ROK3_1)
            return true;

        return false;
    }

    void Hook()
    {
        auto pattern = find_pattern("B9 ? ? ? ? E8 ? ? ? ? 50 8D 84 24 ? ? ? ? 50 E8 ? ? ? ? 83 C4 0C", "B9 ? ? ? ? E8 ? ? ? ? 50 8D 84 24 ? ? ? ? 68 ? ? ? ? 50 BA ? ? ? ? E8 ? ? ? ? 83 C4 14");
        g_text = *pattern.get_first<CText*>(1);

        pattern = find_pattern("E8 ? ? ? ? 50 68 ? ? ? ? 8D 84 24 ? ? ? ? 68 ? ? ? ? 50 E8 ? ? ? ? 83 C4 18", "E8 ? ? ? ? 50 8D 84 24 ? ? ? ? 68 ? ? ? ? 50 BA ? ? ? ? E8 ? ? ? ? 83 C4 14");
        Get = (const wchar_t* (__fastcall*)(void*, void*, const char*))injector::GetBranchDestination(pattern.get_first(0)).as_int();

        pattern = find_pattern("E8 ? ? ? ? 50 8D 86 ? ? ? ? 50 E8 ? ? ? ? 83 C4 0C EB 27", "E8 ? ? ? ? 50 8D 8F ? ? ? ? 51");
        shGetTextByKey = safetyhook::create_inline(injector::GetBranchDestination(pattern.get_first()).as_int(), getTextByKey);

        pattern = find_pattern("51 8B 44 24 08 53 8B D9 C6 44 24", "51 8B 44 24 08 85 C0 53 8B D9");
        shDoesTextLabelExist = safetyhook::create_inline(pattern.get_first(), doesTextLabelExist);
    }
}

// The game's fonts keep some characters at positions of their own. GXT texts are compiled through the fonts' character
// tables (source/resources/GameFont.rc); text of the descriptions goes through the same tables on its way to the menu.
namespace GameFont
{
    struct Table
    {
        std::unordered_map<wchar_t, wchar_t> toGame;
        std::unordered_map<wchar_t, wchar_t> toUnicode;
    };

    // "XXXX=YYYY" lines: a Unicode character and its position in the font; the first line of a character counts
    static Table Load(const wchar_t* name)
    {
        Table table;
        HMODULE module = nullptr;
        GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCWSTR)&Load, &module);
        auto resource = FindResourceW(module, name, L"GAMEFONT");
        auto data = resource ? LoadResource(module, resource) : nullptr;
        auto text = data ? static_cast<const char*>(LockResource(data)) : nullptr;
        if (!text)
            return table;
        std::string_view rest(text, SizeofResource(module, resource));
        while (!rest.empty())
        {
            auto end = rest.find('\n');
            auto line = rest.substr(0, end);
            rest = end == rest.npos ? std::string_view() : rest.substr(end + 1);
            uint32_t unicode = 0, game = 0;
            auto equals = line.find('=');
            if (equals == line.npos ||
                std::from_chars(line.data(), line.data() + equals, unicode, 16).ec != std::errc() ||
                std::from_chars(line.data() + equals + 1, line.data() + line.size(), game, 16).ec != std::errc())
                continue;
            table.toGame.emplace(static_cast<wchar_t>(unicode), static_cast<wchar_t>(game));
            table.toUnicode.emplace(static_cast<wchar_t>(game), static_cast<wchar_t>(unicode));
        }
        return table;
    }

    // The table of the font of a language (BCP 47 tag): Russian and Japanese have fonts of their own
    static const Table& Find(std::string_view language)
    {
        static const Table latin = Load(L"LATIN");
        static const Table russian = Load(L"RU");
        static const Table japanese = Load(L"JA");
        return language == "ru" ? russian : language == "ja" ? japanese : latin;
    }

    // A similar character for one the font lacks: full-width forms as ASCII, a long dash as a shorter one
    static wchar_t StandIn(const std::unordered_map<wchar_t, wchar_t>& table, wchar_t c)
    {
        if (c >= 0xFF01 && c <= 0xFF5E)
            return static_cast<wchar_t>(c - 0xFEE0);
        if (c == 0x2014 || c == 0x2015)
        {
            for (wchar_t dash : { 0x2014, 0x2015, 0x2013 })
                if (table.contains(dash))
                    return dash;
            return L'-';
        }
        return c;
    }

    // Unicode text to the font's positions. Tokens (~r~, ~INPUT_JUMP~) stay as they are.
    static std::wstring ToGame(std::wstring text, std::string_view language)
    {
        auto& table = Find(language).toGame;
        bool token = false;
        for (auto& c : text)
        {
            if (c == L'~')
                token = !token;
            else if (!token)
            {
                if (c >= 0x100 && !table.contains(c))
                    c = StandIn(table, c);
                if (auto it = table.find(c); it != table.end())
                    c = it->second;
            }
        }
        return text;
    }

    // Text of the game's to Unicode. GXT texts mark the characters of tokens with the high bit, which is dropped.
    static std::wstring ToUnicode(std::wstring_view text, std::string_view language)
    {
        auto& table = Find(language).toUnicode;
        std::wstring out(text);
        bool token = false;
        for (auto& c : out)
        {
            if ((c & 0x7FFF) == L'~')
            {
                c = L'~';
                token = !token;
            }
            else if (token)
                c &= 0x7FFF;
            else if (auto it = table.find(c); it != table.end())
                c = it->second;
        }
        return out;
    }
}

// The settings of Fusion Fix are described in settings/GTAIV.EFLC.FusionFix.json, which the build embeds together
// with its translations (source/resources/Settings.rc). asi_settings::Plugin takes the defaults from it, reads and
// writes GTAIV.EFLC.FusionFix.cfg next to the plugin, and answers menu hosts and editors through the
// AsiSettings_GetApi export below (ASI Settings, see external/FusionFixUtils/asi_settings).
export asi_settings::Plugin& SettingsPlugin()
{
    static asi_settings::Plugin plugin;
    static bool loaded = []()
    {
        if (!plugin.Load())
            OutputDebugStringA(("GTAIV.EFLC.FusionFix: settings unavailable, " + plugin.Error() + "\n").c_str());
        // FusionFixHub changes the values while the game runs (the live link, served for all plugins of the game)
        else
            asi_settings::LiveLink::Start();
        return plugin.Loaded();
    }();
    return plugin;
}

ASI_SETTINGS_EXPORT(SettingsPlugin())

// The modules' reader of the plugin's settings: the value in effect of SECTION.Key from the description and the cfg
// (ASI Settings), with Read* calls. The default given here is only used for keys the description lacks.
export class SettingsReader
{
    static const asi_settings::Setting* Find(std::string_view section, std::string_view key)
    {
        return SettingsPlugin().GetDescription().Find(std::string(section) + "." + std::string(key));
    }

    static std::string Id(std::string_view section, std::string_view key)
    {
        return std::string(section) + "." + std::string(key);
    }

public:
    int ReadInteger(std::string_view section, std::string_view key, int defaultValue)
    {
        auto setting = Find(section, key);
        if (!setting)
            return defaultValue;
        if (setting->FloatValued())
            return static_cast<int>(SettingsPlugin().Float(setting->id));
        if (setting->StringValued())
        {
            // Keys are virtual-key codes like 0xDB
            int value = 0;
            return asi_settings::detail::ParseInt(SettingsPlugin().Get(setting->id), value) ? value : defaultValue;
        }
        return SettingsPlugin().Int(setting->id);
    }

    float ReadFloat(std::string_view section, std::string_view key, float defaultValue)
    {
        auto setting = Find(section, key);
        if (!setting)
            return defaultValue;
        if (setting->FloatValued())
            return SettingsPlugin().Float(setting->id);
        return static_cast<float>(ReadInteger(section, key, static_cast<int>(defaultValue)));
    }

    std::string ReadString(std::string_view section, std::string_view key, std::string_view defaultValue)
    {
        return Find(section, key) ? SettingsPlugin().Get(Id(section, key)) : std::string(defaultValue);
    }
};

export class CSettings
{
public:
    static inline int32_t* ms_ShadowQuality;
    static inline int32_t* ms_NightShadows;

private:
    struct CSetting
    {
        int32_t value = 0;              // the value in effect, a copy of the plugin's for settings of the description
        std::string prefName;
        std::string id;                 // SECTION.Key of the description, empty for in-memory preferences
        std::function<void(int32_t value)> callback;
        int32_t idStart = 0;
        int32_t idEnd = 1;
        // Code-defined options keep their value outside the preference (AddToggle/AddEnum/AddSlider with callbacks)
        std::function<int32_t()> getter;
        std::function<void(int32_t)> setter;
        std::function<bool(int32_t value)> available;

        auto GetValue() { return getter ? getter() : value; }
        void SetValue(int32_t v)
        {
            v = std::clamp(v, idStart, idEnd);
            // The menu wraps around, so a step from the last value to the first one counts as forward
            auto previous = GetValue();
            v = FindAvailable(v, v == previous + 1 || (previous == idEnd && v == idStart));
            if (!id.empty())
            {
                // The plugin checks, stores and applies it: OnPluginChange copies the value in effect and runs the
                // callback, here on the game's thread
                if (v != previous)
                    SettingsPlugin().Set(id, std::to_string(v));
                return;
            }
            value = v;
            if (setter)
                setter(value);
            if (callback) callback(value);
        }

        // Values that are not available on this system are skipped, in the direction the value was changed
        int32_t FindAvailable(int32_t v, bool forward)
        {
            if (!available)
                return v;
            for (auto i = idStart; i <= idEnd; ++i)
            {
                if (available(v))
                    return v;
                if (forward)
                    v = v == idEnd ? idStart : v + 1;
                else
                    v = v == idStart ? idEnd : v - 1;
            }
            return idStart;
        }
    };

    struct MenuPrefs
    {
        uint32_t prefID;
        char* name;
    };

    static inline std::vector<MenuPrefs> aMenuPrefs;
    static inline auto firstCustomID = 0;
    static inline bool legacyExecutable = false;
    static inline std::unordered_map<std::string, int32_t> displayIDs;
    static inline int32_t nextDisplayID = 60;
private:
    static inline int32_t* mPrefs = nullptr;
    static inline std::unordered_map<uint32_t, CSetting> mFusionPrefs;

    static inline SafetyHookInline nameLookup;
    static inline SafetyHookInline displayLookup;

    struct DynamicOption
    {
        int32_t screen;
        SettingsTables::Option option;
        std::function<void()> selected;
        int32_t submenu = -1;           // screen of the page the row opens
        std::string before;             // label of the row it's inserted before, else it's added at the end
        int32_t beforePreference = -1;  // or the preference of that row
        uint8_t episodes = 0xFF;        // shown in these episodes: 1 IV, 2 TLAD, 4 TBoGT
        bool gap = false;               // an empty line
        std::function<bool()> visible;  // the row is left out while false (settings of other plugins)
    };
    static inline std::vector<DynamicOption> dynamicOptions;

    // A plugin with ASI Settings whose settings the menu shows: Fusion Fix itself, whose values go through
    // SettingsPlugin(), or another plugin of the process, through the table of its AsiSettings_GetApi export
    struct MenuPlugin
    {
        HMODULE module = nullptr;
        const AsiSettingsApi* api = nullptr;
        asi_settings::DescribedFile file;

        // Functions past the plugin's table don't exist (SPEC.md, Compatibility)
        bool Has(size_t offset) const { return api && api->size >= offset + sizeof(void*); }

        // The value in effect, as text
        std::string Get(const std::string& id) const
        {
            if (!api)
                return SettingsPlugin().Get(id);
            char buffer[256];
            auto length = api->Get(id.c_str(), buffer, sizeof(buffer));
            if (length < 0)
                return {};
            if (length < static_cast<int32_t>(sizeof(buffer)))
                return std::string(buffer, length);
            std::string value(static_cast<size_t>(length) + 1, '\0');
            length = api->Get(id.c_str(), value.data(), static_cast<uint32_t>(value.size()));
            value.resize(length >= 0 ? std::min<size_t>(length, value.size() - 1) : 0);
            return value;
        }

        int32_t Set(const std::string& id, const std::string& value) const
        {
            return api ? api->Set(id.c_str(), value.c_str()) : SettingsPlugin().Set(id, value);
        }

        // The values usable now (SPEC.md, Availability), nothing when every value is
        std::optional<std::vector<std::string>> Usable(const asi_settings::Setting& setting) const
        {
            if (!api)
            {
                if (setting.type != asi_settings::Type::Bool && setting.type != asi_settings::Type::Choice)
                    return std::nullopt;
                return SettingsPlugin().UsableValues(setting.id);
            }
            if (!Has(offsetof(AsiSettingsApi, GetChoices)) || !api->GetChoices)
                return std::nullopt;
            char buffer[1024];
            auto length = api->GetChoices(setting.id.c_str(), buffer, sizeof(buffer));
            if (length < 0)
                return std::nullopt;
            std::string json(buffer, std::min<size_t>(length, sizeof(buffer) - 1));
            if (length >= static_cast<int32_t>(sizeof(buffer)))
            {
                json.assign(static_cast<size_t>(length) + 1, '\0');
                length = api->GetChoices(setting.id.c_str(), json.data(), static_cast<uint32_t>(json.size()));
                json.resize(length >= 0 ? std::min<size_t>(length, json.size() - 1) : 0);
            }
            asi_settings::json::Value root;
            if (!asi_settings::json::Parse(json, root) || !root.IsArray())
                return std::nullopt;
            std::vector<std::string> values;
            for (auto& item : root.items)
                if (auto value = item.Find("value"))
                    values.push_back(asi_settings::detail::JsonToText(*value));
            return values;
        }

        static bool IsUsable(const std::optional<std::vector<std::string>>& usable, const std::string& value)
        {
            return !usable || std::any_of(usable->begin(), usable->end(), [&](const std::string& u)
            {
                float a, b;
                return asi_settings::text::IEquals(u, value) ||
                    (asi_settings::detail::ParseFloat(u, a) && asi_settings::detail::ParseFloat(value, b) && a == b);
            });
        }

        // No usable value now: the setting is left out (ASI_SETTINGS_STATE_HIDDEN)
        bool Hidden(const std::string& id) const
        {
            if (!api)
                return SettingsPlugin().State(id) == ASI_SETTINGS_STATE_HIDDEN;
            return Has(offsetof(AsiSettingsApi, GetState)) && api->GetState && api->GetState(id.c_str(), nullptr, 0) == ASI_SETTINGS_STATE_HIDDEN;
        }

        // The plugin's values as the menu sees them, to evaluate conditions and presets
        std::vector<asi_settings::Slot> Slots() const
        {
            std::vector<asi_settings::Slot> slots;
            for (auto& setting : file.description.settings)
            {
                if (!setting.HasValue())
                    continue;
                asi_settings::Slot slot;
                slot.setting = &setting;
                asi_settings::SetDefault(slot);
                asi_settings::ParseValue(slot, Get(setting.id), false);
                slots.push_back(std::move(slot));
            }
            return slots;
        }

        bool Evaluate(const asi_settings::Condition& condition) const
        {
            auto slots = Slots();
            return asi_settings::Evaluate(condition, [&](std::string_view id) -> const asi_settings::Slot*
            {
                for (auto& slot : slots)
                    if (asi_settings::text::IEquals(slot.setting->id, id))
                        return &slot;
                return nullptr;
            });
        }

        bool HasPresets() const
        {
            auto& settings = file.description.settings;
            // Presets change only menu settings, never compatibility ones (SPEC.md, Presets)
            return std::any_of(settings.begin(), settings.end(), [](auto& s) { return s.InPresets() && s.HasValue(); });
        }

        // The preset its preset settings match, -1 for Custom
        int32_t CurrentPreset() const
        {
            if (!api)
                return static_cast<int32_t>(SettingsPlugin().CurrentPreset());
            return static_cast<int32_t>(asi_settings::CurrentPreset(Slots()));
        }

        void ApplyPreset(int32_t preset) const
        {
            if (!api)
            {
                SettingsPlugin().ApplyPreset(static_cast<asi_settings::Preset>(preset));
                return;
            }
            for (auto& slot : Slots())
            {
                auto& presets = slot.setting->presets;
                if (!slot.setting->InPresets())
                    continue;
                auto value = asi_settings::detail::JsonToText((*presets)[preset]);
                if (!asi_settings::detail::JsonEquals((*presets)[preset], asi_settings::FormatValue(slot), !slot.setting->StringValued()))
                    api->Set(slot.setting->id.c_str(), value.c_str());
            }
        }

        std::string Name() const { return file.texts.Resolve(file.description.name); }
    };
    static inline MenuPlugin ownPlugin;
    static inline std::deque<MenuPlugin> otherPlugins;  // never moved: rows keep pointers to them

    // Text of the number next to code-defined sliders (MENU_DISPLAY_VALUE_SLIDERBAR), by preference
    static inline std::unordered_map<int32_t, std::function<std::wstring()>> valueTexts;
    static inline SafetyHookMid valueTextHook;

    // Changes of rows loaded from the frontend XML: removed, or another scaler
    struct RowEdit
    {
        int32_t screen;
        std::string label;
        int32_t preference = -1;        // any preference
        bool remove = false;
        int32_t scaler = -1;
    };
    static inline std::vector<RowEdit> rowEdits;

    // Custom screens (AddScreen: tabs, AddSubmenu: pages opened from a row, AddCategory) are shown in place
    // of a host screen, whose option array points at the page's rows while one is active: the Display
    // screen in the pause menu, the parent screen itself in the title menu (no tabs there) and for categories.
    struct Page
    {
        char label[240]{};              // a key of the texts, else the text itself
        std::wstring text;
        std::array<SettingsTables::Option, 50> rows{};
        int32_t parent = -1;
        int32_t parentRow = 0;
        bool displayGame = false;
        bool category = false;          // selected in the column of categories of its parent screen
        std::function<std::wstring()> value;    // shown in the row that opens the page
    };

    // A column of categories left of the options of a screen or a page: the screen's own options first (a screen of
    // the game), then pages. A page's column has only pages: the page shows the selected one (Plugins).
    struct CategoryScreen
    {
        int32_t screen = -1;
        bool ownRows = true;
        std::vector<int32_t> pages;
        int32_t current = 0;
        std::vector<int32_t> selectedRows;  // the row selected when the category was left

        int32_t Count() const { return static_cast<int32_t>(pages.size()) + (ownRows ? 1 : 0); }
        // The page of an entry, -1 for the screen's own options
        int32_t PageOf(int32_t entry) const { return ownRows ? (entry > 0 ? pages[entry - 1] : -1) : pages[entry]; }
    };
    static inline std::vector<CategoryScreen> categoryScreens;

    struct Bounds
    {
        float left = 0.0f;
        float top = 0.0f;
        float right = 0.0f;
        float bottom = 0.0f;
    };
    // The column of categories is drawn by the menu API of CE; elsewhere categories are submenus
    static inline bool categoriesEnabled = false;
    static inline bool categoryFocus = true;            // up and down change the category, else the option
    static inline bool backConsumed = false;
    static inline bool categorySwitched = false;
    static inline bool blockAccept = false;             // Enter was taken (Preset rows)
    static inline int32_t swallowFrames = 0;            // frames Enter and Back are ignored after the confirmation
    static inline int32_t pendingSelection = -2;        // the row to select once the screen is filled, -1 for the first
    static inline std::array<Bounds, 64> categoryBounds{};
    static inline uint8_t* layoutMenu = nullptr;
    static inline std::array<float, 2> originalPosition{};
    static inline std::array<float, 2> originalWidths{};
    static inline std::array<float, 3> appliedLayout{};     // position and widths written by ApplyLayout
    static inline std::array<uint8_t, 2> originalAutoScale{};
    static inline float* bodyPosition = nullptr;
    static inline uint8_t* fontStates = nullptr;
    static inline float valueColumnWidth = 0.0f;
    static inline float sidebarWidth = 0.0f;
    static inline int32_t(__cdecl* selectRow)(int32_t, int32_t) = nullptr;
    static inline void(__cdecl* setFontStyle)(int32_t) = nullptr;
    static inline void(__cdecl* setFontOrientation)(int32_t) = nullptr;
    static inline void(__cdecl* setFontWrap)(float, float) = nullptr;
    static inline void(__cdecl* setFontEdge)(float) = nullptr;
    static inline void(__cdecl* setFontEdgeColor)(uint32_t) = nullptr;
    static inline int32_t(__cdecl* getFontContext)() = nullptr;
    static inline float* (__cdecl* getColumnWidth)(float*) = nullptr;
    static inline uint8_t(__cdecl* adjustForWidescreen)(int32_t, float*, float*, float*) = nullptr;
    static inline SafetyHookInline menuDraw;

    struct Tab
    {
        int32_t id = -1;
        float left = 0.0f;
        float top = 0.0f;
        float right = 0.0f;
        float bottom = 0.0f;
        bool visible = false;
    };

    static constexpr int32_t FirstCustomScreen = 256;
    static constexpr int32_t NoTab = 73;
    static constexpr int32_t DisplayScreen = 8;
    // Pages are never moved: the host screen's option array points at the rows of the page shown. Each sub-page
    // takes one for the pause menu and one for the title menu.
    static constexpr size_t MaxPages = 512;
    static inline std::deque<Page> pages;
    static inline size_t pageCount = 0;
    static inline size_t customTabCount = 0;
    // Stock tabs in visual order; Stats is skipped online and Game uses NetworkGame
    static inline std::vector<Tab> tabs = { { 3 }, { 2 }, { 4 }, { 5 }, { 7 }, { 8 }, { 49 }, { 0 } };
    static inline bool tabsInitialized = false;
    static inline SettingsTables::Array<SettingsTables::Option> displayOptions{};
    static inline int32_t activePage = -1;
    static inline int32_t activeHost = DisplayScreen;   // screen showing the active page
    static inline bool pendingTitleBack = false;
    static inline int32_t pendingPage = -1;
    static inline int32_t requestedTab = -1;
    static inline int32_t openSubmenu = -1;
    static inline bool mouseTabRequest = false;
    static inline bool tabTransitionComplete = false;
    static inline bool tabTextEnabled = false;
    static inline bool checkingBack = false;
    static inline bool submenuBack = false;
    static inline uint8_t* tabInputState = nullptr;
    static inline uint8_t* tabDrawState = nullptr;
    static inline uint8_t* tabState = nullptr;
    static inline void* tabTextObject = nullptr;
    static inline int32_t* tabHover = nullptr;
    static inline int32_t* mousePosition = nullptr;
    static inline float* mouseScale = nullptr;
    static inline uintptr_t pageBackground = 0;
    static inline uintptr_t drawPageBackground = 0;
    static inline uintptr_t afterPageBackground = 0;
    static inline uint8_t* renderGame = nullptr;
    static inline uint8_t* frontendActive = nullptr;
    static inline float tabSpacing = 0.0f;
    static inline std::array<std::array<wchar_t, 60>, 73> tabText{};
    static inline std::array<std::array<wchar_t, 60>, 73> tabLabels{};
    static inline float* (__cdecl* getTabWidget)(float*, int32_t) = nullptr;
    static inline void(__cdecl* setTabScale)(float, float) = nullptr;
    static inline float(__cdecl* getTabHeight)() = nullptr;
    static inline void(__cdecl* setTabColor)(uint32_t) = nullptr;
    static inline uint32_t* (__cdecl* getHudColour)(uint32_t*, int32_t) = nullptr;
    static inline uint8_t(__cdecl* isNetworkGame)() = nullptr;
    static inline uint8_t(__cdecl* hasTabModal)() = nullptr;
    static inline int32_t(__cdecl* hideRow)(int32_t, int32_t, uint8_t) = nullptr;
    static inline const char* (__cdecl* copyRowLabel)(const char*, wchar_t*) = nullptr;
    static inline SafetyHookInline processTabs;
    static inline SafetyHookInline drawTabs;
    static inline SafetyHookInline checkForBackInput;
    static inline SafetyHookInline queryInput;
    static inline SafetyHookInline switchToNewScreen;
    static inline SafetyHookInline getTabText;
    static inline SafetyHookInline printTab;
    static inline SafetyHookInline measureTab;
    static inline SafetyHookMid tabTransitionHook;
    static inline SafetyHookMid gameVisibilityHook;
    static inline SafetyHookMid resetScreenRowsHook;
    static inline SafetyHookMid resetScreenRowsHook2;
    static inline SafetyHookMid pageBackgroundHook;
    static inline int32_t callbackCount = 0;
    static inline std::map<int32_t, std::vector<SettingsTables::DisplayValue>> dynamicDisplays;
    static inline SettingsTables::Screen* screens = nullptr;
    static inline int32_t* currentScreen = nullptr;
    static inline SettingsTables::Option* (__thiscall* appendOption)(SettingsTables::Array<SettingsTables::Option>*, int32_t);
    static inline SettingsTables::DisplayValue* (__thiscall* appendDisplay)(SettingsTables::Array<SettingsTables::DisplayValue>*, int32_t);
    static inline void* appendOptionLegacy = nullptr;
    static inline void* appendDisplayLegacy = nullptr;
    static inline SafetyHookInline fillMenu;
    static inline SafetyHookInline scrollMenu;
    static inline SafetyHookInline selectOption;
    static inline SafetyHookInline processMenu;
    static inline thread_local int32_t selectedButton = -1;
    static inline thread_local int32_t inputMenu = -1;
    static inline int32_t* menuHandles = nullptr;
    static inline uint8_t** menuInstances = nullptr;
    static inline float* rowHeight = nullptr;
    static inline SafetyHookMid sliderDrawRow;
    static inline SafetyHookMid sliderMouseRow;

    static SettingsTables::Option* AppendOption(SettingsTables::Array<SettingsTables::Option>* array)
    {
        if (legacyExecutable)
            return static_cast<SettingsTables::Option*>(SettingsTables::AppendLegacy(array, appendOptionLegacy));
        return appendOption(array, 0);
    }

    static SettingsTables::DisplayValue* AppendDisplay(SettingsTables::Array<SettingsTables::DisplayValue>* array)
    {
        if (legacyExecutable)
            return static_cast<SettingsTables::DisplayValue*>(SettingsTables::AppendLegacy(array, appendDisplayLegacy));
        return appendDisplay(array, 0);
    }

    static void AdjustSliderRow(SafetyHookContext& regs, bool drawing)
    {
        auto row = drawing ? *reinterpret_cast<int32_t*>(regs.esp + (legacyExecutable ? 0x24 : 0x30)) :
            static_cast<int32_t>(legacyExecutable ? regs.esi : regs.ebp);
        auto menu = drawing ? 0 : inputMenu;
        if (menu < 0 || *currentScreen < 0 || *currentScreen >= 73 || row < 0 || row >= 50)
            return;
        auto& options = screens[*currentScreen].options;
        if (row >= options.count || !options.data || (options.data[row].display != 101 && options.data[row].display != 108) ||
            options.data[row].preference < firstCustomID)
            return;
        auto handle = menuHandles[menu];
        if (handle < 0 || !menuInstances[handle])
            return;
        auto visible = menuInstances[handle] + 0x33E4;
        auto hidden = std::count(visible, visible + row, uint8_t(0));
        auto aspect = *reinterpret_cast<float*>(regs.esp + (drawing ? (legacyExecutable ? 0x34 : 0x3C) : 0x18));
        auto correction = hidden * (aspect >= 1.0f ? *rowHeight : 0.017f);
        if (legacyExecutable)
            *reinterpret_cast<float*>(regs.esp + (drawing ? 0x1C : 0x10)) -= correction;
        else if (drawing)
        {
            float y;
            std::memcpy(&y, &regs.eax, sizeof(y));
            y -= correction;
            std::memcpy(&regs.eax, &y, sizeof(y));
        }
        else
            *reinterpret_cast<float*>(regs.esp + 0x14) -= correction;
    }

    static void InjectMenu(int32_t screen)
    {
        // Rebuild code-defined displays after every frontend XML reload. All
        // allocation remains with the game's allocator and cleanup routines.
        for (auto& [id, values] : dynamicDisplays)
        {
            auto& display = SettingsTables::displays[id];
            display.id = id;
            if (display.values.count == 0)
                for (auto& value : values)
                    *AppendDisplay(&display.values) = value;
        }
        if (screen < 0 || screen >= 73)
            return;
        auto& options = screens[screen].options;
        auto logical = GetLogicalScreen(screen);
        auto page = FindPage(logical);
        if (page)
        {
            // Pages only hold code-defined rows, rebuilt on every fill
            options.count = 1;
            options.data[0] = {};
            options.data[0].action = 46;
        }
        if (!options.data || options.count == 0 || options.count > options.capacity || options.data[options.count - 1].action != 46)
            return;
        if (!page)
            RecordGraphicsRows(screen);

        // Rows of the XML that are removed (after the rows inserted before them) or get another scaler
        auto editRows = [&](bool remove)
        {
            if (page)
                return;
            for (auto& edit : rowEdits)
            {
                if (edit.screen != screen || edit.remove != remove)
                    continue;
                for (int32_t row = 0; row + 1 < options.count; ++row)
                {
                    auto& option = options.data[row];
                    if (option.action == 127 || edit.label != option.label || (edit.preference >= 0 && edit.preference != option.preference))
                        continue;
                    if (remove)
                    {
                        std::memmove(&options.data[row], &options.data[row + 1], (options.count - row - 1) * sizeof(SettingsTables::Option));
                        --options.count;
                        --row;
                    }
                    else if (edit.scaler >= 0)
                        option.scaler = static_cast<uint8_t>(edit.scaler);
                }
            }
        };
        editRows(false);

        auto episode = uint8_t(1) << (_dwCurrentEpisode ? std::clamp(*_dwCurrentEpisode, 0, 2) : 0);
        for (auto& added : dynamicOptions)
        {
            if (added.screen != logical || (added.episodes & episode) == 0)
                continue;
            auto found = std::find_if(options.data, options.data + options.count, [&](const auto& option)
            {
                return option.action == added.option.action && option.preference == added.option.preference &&
                    std::strcmp(option.label, added.option.label) == 0;
            });
            // A hidden row is taken out again when the XML's rows still have it
            if (added.visible && !added.visible())
            {
                if (found != options.data + options.count && !page)
                {
                    std::memmove(found, found + 1, (options.data + options.count - found - 1) * sizeof(SettingsTables::Option));
                    --options.count;
                }
                continue;
            }
            if (found != options.data + options.count)
                continue;
            // The frontend instance has visibility/layout storage for 50 rows,
            // including END_OF_MENU_OPTIONS. Growing the XML array alone is not
            // sufficient to lift that limit.
            if (options.count >= (page ? static_cast<uint16_t>(page->rows.size()) : 50))
                return;
            // Before END_OF_MENU_OPTIONS, or before the row with the label or the preference given by 'before'
            auto position = options.count - 1;
            if (!added.before.empty())
                for (uint16_t row = 0; row + 1 < options.count; ++row)
                    if (added.beforePreference >= 0 ? options.data[row].preference == added.beforePreference && options.data[row].action != 127 :
                        added.before == options.data[row].label)
                    {
                        position = row;
                        break;
                    }
            auto sentinel = options.data[options.count - 1];
            if (page)
                options.data[options.count++] = sentinel;
            else
                *AppendOption(&options) = sentinel;
            std::memmove(&options.data[position + 1], &options.data[position], (options.count - 2 - position) * sizeof(SettingsTables::Option));
            options.data[position] = added.option;
        }
        editRows(true);
        shownVisibility = VisibleRows(logical);
    }

    // Empty lines added through the API: MENUOPT_NONE rows with a label of their own, shown without text. Like the
    // empty lines of the XML the menu can't select them, it only selects rows with a text.
    static bool IsGap(const SettingsTables::Option& option)
    {
        return option.action == 0 && std::strncmp(option.label, "FFGAP", 5) == 0;
    }

    // After the rows of another category are shown: the selection goes to the row selected there before, else to
    // the first selectable row (CE)
    static void FixSelection(int32_t menu, bool reset, int32_t preferred = -1)
    {
        auto handle = menuHandles[menu];
        if (!selectRow || handle < 0 || !menuInstances[handle] || *currentScreen < 0 || *currentScreen >= 73)
            return;
        auto instance = menuInstances[handle];
        auto& options = screens[*currentScreen].options;
        auto count = std::min<int32_t>(options.count, 50);
        // Like the menu's own navigation: shown, selectable, enabled and with a text
        auto selectable = [&](int32_t row)
        {
            return row >= 0 && row < count && options.data[row].action != 46 && !IsHeading(options.data[row]) && instance[0x33E4 + row] != 0 &&
                instance[0x3416 + row] != 0 && instance[0x34AC + row] == 0 && *reinterpret_cast<const wchar_t*>(instance + 4 + 120 * row) != 0;
        };
        auto selected = *reinterpret_cast<int32_t*>(instance + 0x361C);
        if (!reset && preferred < 0 && selectable(selected))
            return;
        auto target = selectable(preferred) ? preferred : -1;
        if (auto categories = GetActiveCategories(); target < 0 && reset && categories && selectable(categories->selectedRows[categories->current]))
            target = categories->selectedRows[categories->current];
        for (int32_t row = 0; target < 0 && row < count; ++row)
            if (selectable(row))
                target = row;
        if (target < 0)
            return;
        *reinterpret_cast<int32_t*>(instance + 0x362C) = 0;
        selectRow(menu, target);
        // The menu highlights the selected row only
        std::memset(instance + 0x3448, 0, 50);
        instance[0x3448 + target] = 1;
    }

    // ---------------------------------------------------------------------------------------------
    // Column of categories left of the options of Display and Graphics (CE only)

    static CategoryScreen* FindCategories(int32_t screen)
    {
        for (auto& categories : categoryScreens)
            if (categories.screen == screen)
                return &categories;
        return nullptr;
    }

    // The categories of the current screen while its column is shown: a screen's own column, not in pages opened from
    // its rows, or the column of the page whose category is shown
    static CategoryScreen* GetActiveCategories()
    {
        if (!categoriesEnabled || *currentScreen < 0 || *currentScreen >= 73)
            return nullptr;
        auto page = FindPage(activePage);
        if (!page)
            return FindCategories(*currentScreen);
        if (!page->category || activeHost != *currentScreen)
            return nullptr;
        return FindCategories(page->parent);
    }

    static uint8_t* GetMenu()
    {
        auto handle = menuHandles[0];
        return handle >= 0 ? menuInstances[handle] : nullptr;
    }

    // The rows of the category selected in the column of the current screen replace its own rows
    static void ShowCategory()
    {
        if (!categoriesEnabled)
            return;
        auto page = FindPage(activePage);
        auto categories = !page ? FindCategories(*currentScreen) : page->category && activeHost == *currentScreen ? FindCategories(page->parent) : nullptr;
        if (!categories || categories->Count() == 0)
            return;
        categories->current = std::clamp(categories->current, 0, categories->Count() - 1);
        if (auto shown = categories->PageOf(categories->current); shown != -1)
            ActivatePage(shown);
        else
            RestoreDisplay();
    }

    // The name of a page: its label is a key of FF's texts, else the text itself
    static const wchar_t* GetPageName(const Page& page)
    {
        if (auto text = CText::FindMenuText(GetHash(page.label)))
            return text;
        return page.text.c_str();
    }

    static void ResetCategories()
    {
        RestoreLayout();
        for (auto& categories : categoryScreens)
        {
            categories.current = 0;
            std::fill(categories.selectedRows.begin(), categories.selectedRows.end(), -1);
        }
        categoryFocus = true;
        categorySwitched = false;
    }

    static bool HasBodyFocus()
    {
        return GetActiveCategories() && tabState && tabState[0] == 0 && tabState[1] == 0 && hasTabModal() == 0;
    }

    // The options move right of the column, the values stay where they are. The layout stays while the column is
    // shown: the render thread draws the menu while the main thread processes its input, a layout changed around
    // either of them shows up in the other one. Main thread only.
    static void ApplyLayout()
    {
        auto instance = GetMenu();
        if (!instance || !GetActiveCategories())
            return;
        auto position = reinterpret_cast<float*>(instance + 0x35D0);
        auto widths = reinterpret_cast<float*>(instance + 0x35C8);
        // The first time, or the menu was laid out again since: its values are the original ones
        if (layoutMenu != instance || position[0] != appliedLayout[0] || widths[0] != appliedLayout[1] || widths[1] != appliedLayout[2])
        {
            layoutMenu = instance;
            std::memcpy(originalPosition.data(), position, sizeof(originalPosition));
            std::memcpy(originalWidths.data(), widths, sizeof(originalWidths));
            std::memcpy(originalAutoScale.data(), instance + 0x3605, originalAutoScale.size());
            float width[2]{};
            getColumnWidth(width);
            width[1] = 0.0f;
            adjustForWidescreen(2, nullptr, width, nullptr);
            valueColumnWidth = width[0];
        }
        // The column takes room from the labels, the values stay where the menu draws their sliders. A page's column of
        // plugins has longer names than a screen's categories.
        sidebarWidth = valueColumnWidth * (GetActiveCategories()->ownRows ? 0.38f : 0.55f);
        position[0] = bodyPosition[0] + sidebarWidth;
        widths[0] = valueColumnWidth - sidebarWidth;
        widths[1] = std::max(0.05f, std::min(originalWidths[1], 0.97f - bodyPosition[0] - valueColumnWidth));
        instance[0x3605] = 1;
        instance[0x3606] = 1;
        appliedLayout = { position[0], widths[0], widths[1] };
    }

    // The layout of the current screen: with the column of categories or without
    static void UpdateLayout()
    {
        if (GetActiveCategories())
            ApplyLayout();
        else
            RestoreLayout();
    }

    static void RestoreLayout()
    {
        if (!layoutMenu)
            return;
        if (GetMenu() == layoutMenu)
        {
            std::memcpy(layoutMenu + 0x35D0, originalPosition.data(), sizeof(originalPosition));
            std::memcpy(layoutMenu + 0x35C8, originalWidths.data(), sizeof(originalWidths));
            std::memcpy(layoutMenu + 0x3605, originalAutoScale.data(), originalAutoScale.size());
        }
        layoutMenu = nullptr;
    }

    static void SelectCategory(CategoryScreen& categories, int32_t category)
    {
        if (auto instance = GetMenu())
            categories.selectedRows[categories.current] = *reinterpret_cast<int32_t*>(instance + 0x361C);
        categories.current = category;
        categoryFocus = true;
        categorySwitched = true;
        FillMenu(0);
    }

    static void MoveCategory(CategoryScreen& categories, int32_t direction)
    {
        auto count = categories.Count();
        SelectCategory(categories, (categories.current + count + direction) % count);
    }

    static int32_t HitTestCategory(const CategoryScreen& categories)
    {
        auto x = mousePosition[0] * mouseScale[0];
        auto y = mousePosition[3] * mouseScale[2];
        for (size_t i = 0; i < std::min<size_t>(categories.Count(), categoryBounds.size()); ++i)
        {
            auto& bounds = categoryBounds[i];
            if (x >= bounds.left && x < bounds.right && y >= bounds.top && y < bounds.bottom)
                return static_cast<int32_t>(i);
        }
        return -1;
    }

    // Up and down change the category while the column has the focus, accept and right go to the options. Taken
    // input isn't seen by the menu.
    static bool CategoryInput(int32_t menu)
    {
        auto categories = GetActiveCategories();
        if (menu != 0 || !categories || !tabState || tabState[0] != 0 || hasTabModal() != 0 || confirming)
            return false;
        ApplyLayout();
        auto current = static_cast<uint32_t>(mousePosition[2]);
        auto previous = static_cast<uint32_t>(mousePosition[1]);
        if ((current & (current ^ previous) & 1) != 0)
        {
            if (auto hit = HitTestCategory(*categories); hit >= 0)
            {
                tabState[1] = 0;
                SelectCategory(*categories, hit);
                return true;
            }
            if (mousePosition[0] * mouseScale[0] >= bodyPosition[0] + sidebarWidth)
                categoryFocus = false;
        }
        if (!HasBodyFocus() || !categoryFocus)
            return false;
        auto query = [](int32_t input) { return queryInput.ccall<uint8_t>(input, uint8_t(1), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0)) != 0; };
        if (query(0))
            MoveCategory(*categories, -1);
        else if (query(1))
            MoveCategory(*categories, 1);
        else if (query(8) || query(3))
            categoryFocus = false;
        return true;
    }

    // Back in the options goes to the column of categories. Taken for the rest of the frame, the menu checks it more
    // than once.
    static bool ConsumeBack()
    {
        if (!HasBodyFocus() || (categoryFocus && !backConsumed))
            return false;
        categoryFocus = true;
        backConsumed = true;
        return true;
    }

    static void DrawCategories(const CategoryScreen& categories)
    {
        auto instance = GetMenu();
        if (!instance || !tabState || tabState[0] != 0 || hasTabModal() != 0)
            return;
        auto count = std::min<size_t>(categories.Count(), categoryBounds.size());
        std::vector<std::wstring> names(count);
        for (size_t i = 0; i < count; ++i)
        {
            auto page = categories.PageOf(static_cast<int32_t>(i));
            names[i] = page == -1 ? CText::getText(screens[categories.screen].header) : GetPageName(*FindPage(page));
        }

        auto x = bodyPosition[0];
        auto y = originalPosition[1] + 0.04f;
        auto step = *reinterpret_cast<float*>(instance + 0x3600) * 1.35f;
        auto scaleX = *reinterpret_cast<float*>(instance + 0x35DC) * *reinterpret_cast<float*>(instance + 0x35E4);
        auto scaleY = *reinterpret_cast<float*>(instance + 0x35E0) * *reinterpret_cast<float*>(instance + 0x35E8);

        // The font of the menu's rows: its style and scale, outlined like them. The font state of the menu is restored
        // afterwards.
        auto font = fontStates + getFontContext() * 0x48;
        std::array<uint8_t, 0x48> saved;
        std::memcpy(saved.data(), font, saved.size());
        font[0x14] = 0;
        font[0x15] = 0;
        font[0x16] = 1;
        font[0x1A] = 255;
        setFontOrientation(1);
        setFontStyle(*reinterpret_cast<int32_t*>(instance + 0x35D8));
        setTabScale(scaleX, scaleY);
        setFontEdge(*reinterpret_cast<float*>(instance + 0x3608));
        setFontEdgeColor(uint32_t(instance[0x363C]) << 24);
        // Names stay on one line: the font is made narrower down to 2/3, longer names are cut with "..."
        auto available = sidebarWidth * 0.92f;
        auto width = 0.0f;
        for (size_t i = 0; i < count; ++i)
            width = std::max(width, measureTab.ccall<float>(names[i].c_str(), uint8_t(1)));
        auto scale = width > available ? std::max(0.66f, available / width) : 1.0f;
        setTabScale(scaleX * scale, scaleY);
        for (auto& name : names)
        {
            if (measureTab.ccall<float>(name.c_str(), uint8_t(1)) <= available)
                continue;
            while (name.size() > 1 && measureTab.ccall<float>((name + L"...").c_str(), uint8_t(1)) > available)
                name.pop_back();
            while (!name.empty() && name.back() == L' ')
                name.pop_back();
            name += L"...";
        }
        setFontWrap(x - 0.02f, x + 1.0f);

        for (size_t i = 0; i < count; ++i)
        {
            categoryBounds[i] = { x, y, x + sidebarWidth * 0.9f, y + step };
            bool selected = static_cast<int32_t>(i) == categories.current;
            bool highlighted = selected && categoryFocus && tabState[1] == 0;
            setTabColor(*reinterpret_cast<uint32_t*>(instance + (highlighted ? 0x3634 : 0x3630)));
            if (selected)
                printTab.ccall<void>(x - 0.015f, y, L">", -1, -1);
            printTab.ccall<void>(x, y, names[i].c_str(), -1, -1);
            y += step;
        }
        std::memcpy(font, saved.data(), saved.size());
    }

    // Render thread: the layout is left as the main thread set it
    static void __cdecl MenuDraw(int32_t handle)
    {
        StyleRows(handle);
        // While the column of categories has the focus, the rows right of it show no selected row: the menu draws a
        // row in its selected colour only while its highlight flag is set, so the flags are taken away while it draws
        auto instance = handle >= 0 ? menuInstances[handle] : nullptr;
        std::array<uint8_t, 50> highlights{};
        bool unfocused = instance && handle == menuHandles[0] && categoryFocus && GetActiveCategories() && tabState && tabState[0] == 0;
        if (unfocused)
        {
            std::memcpy(highlights.data(), instance + 0x3448, highlights.size());
            std::memset(instance + 0x3448, 0, highlights.size());
        }
        menuDraw.ccall<void>(handle);
        if (unfocused)
            std::memcpy(instance + 0x3448, highlights.data(), highlights.size());
        if (auto categories = GetActiveCategories(); categories && handle == menuHandles[0] && layoutMenu && layoutMenu == GetMenu())
            DrawCategories(*categories);
        DrawBreadcrumb(handle);
        DrawDescription(handle);
        NoteHdrPage(handle);
    }

    // The HDR page is shown: HDR draws its calibration pattern for the selected row at the end of the frame
    static void NoteHdrPage(int32_t handle)
    {
        if (handle < 0 || handle != menuHandles[0] || !menuInstances[handle] || !tabState || hasTabModal() != 0 || confirming ||
            activePage == -1 || activeHost != *currentScreen || std::find(hdrScreens.begin(), hdrScreens.end(), static_cast<MenuScreen>(activePage)) == hdrScreens.end())
            return;
        std::string id;
        auto& options = screens[*currentScreen].options;
        auto row = CMenu::getSelectedItem();
        if (options.data && row >= 0 && row < options.count && options.data[row].action != 127)
            if (auto it = rowSettings.find(options.data[row].preference); it != rowSettings.end() && it->second.setting)
                id = it->second.setting->id;
        hdrPageRow = std::move(id);
    }

    static int32_t __cdecl FillMenu(int32_t menu)
    {
        // Other plugins' pages are looked for when the menu is opened, the title menu included; texts follow the
        // game's language
        FindOtherPlugins();
        ResolveMenuTexts(false);
        if (tabsInitialized)
        {
            if (*currentScreen == DisplayScreen && pendingPage != -1)
                ActivatePage(pendingPage);
            else if (*currentScreen != activeHost)
                RestoreDisplay();
            ShowCategory();
        }

        InjectMenu(*currentScreen);
        int32_t result = 0;
        if (legacyExecutable)
            fillMenu.ccall<void>(menu);
        else
            result = fillMenu.ccall<int32_t>(menu);

        auto handle = menuHandles[menu];
        if (handle >= 0 && menuInstances[handle] && *currentScreen >= 0 && *currentScreen < 73)
        {
            // The game hides rows of its screens by index, pages show all of theirs
            auto& options = screens[*currentScreen].options;
            if (activePage != -1 && *currentScreen == activeHost)
                for (uint16_t row = 0; row + 1 < options.count; ++row)
                    hideRow(handle, row, 0);
        }
        if (menu == 0)
        {
            UpdateLayout();
            // A page that was opened or returned to: the row asked for, else the first one that can be selected, and the
            // list has the focus, not the tabs
            if (auto row = std::exchange(pendingSelection, -2); row != -2)
            {
                FixSelection(menu, true, row);
                if (tabState && !GetActiveCategories())
                    tabState[1] = 0;
            }
            else if (GetActiveCategories())
                FixSelection(menu, std::exchange(categorySwitched, false));
        }
        return result;
    }

    static int32_t FindButton(int32_t row)
    {
        if (*currentScreen < 0 || *currentScreen >= 73 || row < 0)
            return -1;
        auto& options = screens[*currentScreen].options;
        if (row >= options.count || !options.data || options.data[row].action != 127)
            return -1;
        auto index = options.data[row].preference;
        if (index < 0 || static_cast<size_t>(index) >= dynamicOptions.size() ||
            (!dynamicOptions[index].selected && dynamicOptions[index].submenu == -1) ||
            dynamicOptions[index].screen != GetLogicalScreen(*currentScreen))
            return -1;
        return index;
    }

    static uint8_t __cdecl ScrollMenu(int32_t row, int32_t accepted, int32_t direction, uint8_t* adjusted)
    {
        if (auto button = FindButton(row); button >= 0)
        {
            if (accepted >= 0)
                selectedButton = button;
            return 0;
        }
        return scrollMenu.ccall<uint8_t>(row, accepted, direction, adjusted);
    }

    static uint8_t __cdecl SelectOption(int32_t menu, int32_t row, uint8_t adjusted)
    {
        if (auto button = FindButton(row); button >= 0)
        {
            if (button == selectedButton)
            {
                selectedButton = -1;
                if (auto page = FindPage(dynamicOptions[button].submenu))
                {
                    // Opened once the menu input is processed
                    page->parentRow = row;
                    openSubmenu = dynamicOptions[button].submenu;
                }
                else
                {
                    auto callback = dynamicOptions[button].selected;
                    callback();
                    FillMenu(menu);
                }
            }
            return 0;
        }
        if (legacyExecutable)
        {
            selectOption.ccall<void>(menu, row, adjusted);
            return 0;
        }
        return selectOption.ccall<uint8_t>(menu, row, adjusted);
    }

    static uint8_t __cdecl ProcessMenu(int32_t menu)
    {
        if (menu == 0 && ProcessConfirmation())
            return 0;
        if (CategoryInput(menu))
            return 0;
        if (menu == 0)
        {
            ResetSelectedRow();
            // Enter on a Preset row asks to apply the chosen preset, the menu doesn't step its value
            auto selector = FindSelector(SelectedSettingPreference());
            if (selector && queryInput.ccall<uint8_t>(8, uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0)) != 0)
            {
                ConfirmPreset(*selector);
                blockAccept = true;
            }
            // Leaving a Preset row forgets its choice
            for (auto& [key, other] : presetSelectors)
                if (&other != selector && other.pending >= 0)
                    other = { -1, other.preference, other.plugin, false };
        }
        auto previous = std::exchange(selectedButton, -1);
        auto previousMenu = std::exchange(inputMenu, menu);
        auto previousSubmenu = std::exchange(openSubmenu, -1);
        auto result = processMenu.ccall<uint8_t>(menu);
        blockAccept = false;
        auto submenu = openSubmenu;
        selectedButton = previous;
        inputMenu = previousMenu;
        openSubmenu = previousSubmenu;
        // A click on the label of a row that opens a page: the menu only selects the row (a click on its value
        // accepts it). The row under the mouse is found again every frame (-1 when there is none).
        if (menu == 0 && submenu == -1 && !confirming)
        {
            auto instance = GetMenu();
            auto current = static_cast<uint32_t>(mousePosition[2]);
            auto previousButtons = static_cast<uint32_t>(mousePosition[1]);
            if (instance && (current & (current ^ previousButtons) & 1) != 0)
            {
                auto row = *reinterpret_cast<int32_t*>(instance + 0x34E0);
                if (row >= 0 && row == *reinterpret_cast<int32_t*>(instance + 0x361C))
                    if (auto button = FindButton(row); button >= 0 && dynamicOptions[button].submenu != -1)
                        if (auto page = FindPage(dynamicOptions[button].submenu))
                        {
                            page->parentRow = row;
                            submenu = dynamicOptions[button].submenu;
                        }
            }
        }
        if (menu == 0 && submenu != -1)
            OpenPage(submenu);
        // Back in a page of the title menu, taken over by QueryInput
        if (menu == 0 && std::exchange(pendingTitleBack, false))
            if (auto page = FindPage(activePage))
                GoBack(*page);
        return result;
    }

    // ---------------------------------------------------------------------------------------------
    // Custom screens and tabs (CE only)

    static Page* FindPage(int32_t screen)
    {
        auto index = static_cast<uint32_t>(screen) - FirstCustomScreen;
        return index < pageCount ? &pages[index] : nullptr;
    }

    static int32_t GetLogicalScreen(int32_t screen)
    {
        return screen == activeHost && activePage != -1 ? activePage : screen;
    }

    // The settings screens of the title menu, which has no tabs
    static bool IsTitleScreen(int32_t screen)
    {
        return screen >= static_cast<int32_t>(MenuScreen::TitleControls) && screen <= static_cast<int32_t>(MenuScreen::TitleGame);
    }

    // Pages of the pause menu are shown on the Display screen, pages of the title menu on their title screen,
    // categories on the screen they're a category of
    static int32_t HostOf(int32_t page)
    {
        if (auto found = FindPage(page); found && found->category)
            return FindPage(found->parent) ? HostOf(found->parent) : found->parent;
        auto root = GetRootScreen(page);
        return IsTitleScreen(root) ? root : DisplayScreen;
    }

    // The page whose look the shown page has: a category of a page looks like that page
    static Page* ShownPage()
    {
        auto page = FindPage(activePage);
        if (page && page->category)
            if (auto parent = FindPage(page->parent))
                return parent;
        return page;
    }

    // Where Back goes from a page: the row of its parent that opened it. A category of a page goes back to where the
    // page was opened from, a category of a screen leaves the screen (-1).
    static std::pair<int32_t, int32_t> BackTarget(const Page& page)
    {
        if (!page.category)
            return { page.parent, page.parentRow };
        if (auto parent = FindPage(page.parent))
            return { parent->parent, parent->parentRow };
        return { -1, 0 };
    }

    static bool GoBack(const Page& page)
    {
        auto [screen, row] = BackTarget(page);
        if (screen == -1)
            return false;
        OpenPage(screen, row);
        return true;
    }

    static int32_t GetRootScreen(int32_t screen)
    {
        while (auto page = FindPage(screen))
        {
            if (page->parent == -1)
                break;
            screen = page->parent;
        }
        return screen;
    }

    static void ActivatePage(int32_t screen)
    {
        auto page = FindPage(screen);
        if (!page || activePage == screen)
            return;
        auto host = HostOf(screen);
        if (activePage != -1 && activeHost != host)
            RestoreDisplay();
        auto& options = screens[host].options;
        if (activePage == -1)
            displayOptions = options;
        activePage = screen;
        activeHost = host;
        options.data = page->rows.data();
        options.count = 1;
        options.capacity = static_cast<uint16_t>(page->rows.size());
        page->rows[0] = {};
        page->rows[0].action = 46;
    }

    static void RestoreDisplay()
    {
        if (activePage == -1)
            return;
        screens[activeHost].options = displayOptions;
        displayOptions = {};
        activePage = -1;
        pendingTitleBack = false;
    }

    // Before the frontend XML is reloaded or its rows are reset: the game must free its own array
    static void ResetPages()
    {
        RestoreDisplay();
        ResetCategories();
        pendingPage = -1;
        for (auto& tab : tabs)
            tab.visible = false;
    }

    static void OpenPage(int32_t screen, int32_t row = 0)
    {
        // A page with a column of pages shows the category selected last, the column has the focus
        if (auto categories = FindCategories(screen); categories && !categories->ownRows && categoriesEnabled)
        {
            if (categories->Count() == 0)
                return;
            categories->current = std::clamp(categories->current, 0, categories->Count() - 1);
            screen = categories->PageOf(categories->current);
            row = 0;
            ActivatePage(screen);
            categoryFocus = true;
            categorySwitched = true;
            pendingPage = -1;
            if (tabState && !IsTitleScreen(activeHost))
                *reinterpret_cast<int32_t*>(tabState + 0x18) = activeHost;
            switchToNewScreen.ccall<int32_t>(0, activeHost, row);
            return;
        }
        // Back to a screen with categories: the options keep the focus, a category is selected in the column
        auto target = FindPage(screen);
        if (auto categories = FindCategories(target && target->category ? target->parent : screen))
        {
            auto found = std::find(categories->pages.begin(), categories->pages.end(), screen);
            if (found != categories->pages.end())
                categories->current = static_cast<int32_t>(found - categories->pages.begin()) + 1;
            categoryFocus = false;
        }
        bool custom = target != nullptr;
        if (custom)
            ActivatePage(screen);
        else
            RestoreDisplay();
        auto physical = custom ? activeHost : screen;
        pendingPage = -1;
        pendingSelection = custom && row == 0 ? -1 : row;
        // The tab bar of the pause menu
        if (tabState && !IsTitleScreen(physical))
            *reinterpret_cast<int32_t*>(tabState + 0x18) = physical;
        switchToNewScreen.ccall<int32_t>(0, physical, row);
    }

    static void PrepareTabs()
    {
        tabTextEnabled = false;
        float spacing[2]{};
        tabSpacing = getTabWidget(spacing, 3)[0];
        for (auto& tab : tabs)
        {
            if (FindPage(tab.id))
                continue;
            auto text = getTabText.fastcall<const wchar_t*>(tabTextObject, nullptr, screens[tab.id].header);
            wcsncpy_s(tabLabels[tab.id].data(), tabLabels[tab.id].size(), text ? text : L"", _TRUNCATE);
            tabText[tab.id] = tabLabels[tab.id];
        }
        tabTextEnabled = true;
    }

    static const wchar_t* GetTabLabel(const Tab& tab)
    {
        if (auto page = FindPage(tab.id))
            return GetPageName(*page);
        return tabLabels[tab.id].data();
    }

    static int32_t HitTestTab()
    {
        auto x = mousePosition[0] * mouseScale[0];
        auto y = mousePosition[3] * mouseScale[2];
        for (auto& tab : tabs)
            if (tab.visible && x > tab.left && x < tab.right && y > tab.top && y < tab.bottom)
                return tab.id;
        return -1;
    }

    static bool HasTabs(const uint8_t* state)
    {
        if (pageCount == 0 || state[0] != 0)
            return false;
        switch (*reinterpret_cast<const int32_t*>(state + 0x18))
        {
        case 0: case 1: case 2: case 3: case 4: case 5: case 7: case 8: case 49: return true;
        default: return false;
        }
    }

    static int32_t AdjacentTab(int32_t screen, int32_t direction)
    {
        if (screen == 1)
            screen = 0;
        auto found = std::find_if(tabs.begin(), tabs.end(), [screen](const Tab& tab) { return tab.id == screen; });
        if (found == tabs.end())
            return -1;
        auto count = static_cast<int32_t>(tabs.size());
        auto index = static_cast<int32_t>(found - tabs.begin());
        bool network = isNetworkGame() != 0;
        int32_t next;
        do
        {
            index = (index + count + direction) % count;
            next = tabs[index].id;
        } while (network && next == 4);
        return next == 0 && network ? 1 : next;
    }

    // The tab functions are __thiscall in CE and __stdcall with the state as first argument in 1.1.2.0
    static uint8_t CallProcessTabs(uint8_t* state, uint8_t inside)
    {
        return legacyExecutable ? processTabs.stdcall<uint8_t>(state, inside) : processTabs.thiscall<uint8_t>(state, inside);
    }

    static int32_t CallDrawTabs(uint8_t* state)
    {
        return legacyExecutable ? drawTabs.stdcall<int32_t>(state) : drawTabs.thiscall<int32_t>(state);
    }

    static int32_t CallCheckForBackInput(uint8_t* state)
    {
        return legacyExecutable ? checkForBackInput.stdcall<int32_t>(state) : checkForBackInput.thiscall<int32_t>(state);
    }

    static uint8_t __stdcall ProcessTabsLegacy(uint8_t* state, uint8_t inside) { return ProcessTabs(state, nullptr, inside); }
    static int32_t __stdcall DrawTabsLegacy(uint8_t* state) { return DrawTabs(state, nullptr); }
    static int32_t __stdcall CheckForBackInputLegacy(uint8_t* state) { return CheckForBackInput(state, nullptr); }

    static uint8_t __fastcall ProcessTabs(uint8_t* state, void*, uint8_t inside)
    {
        tabState = state;
        // A new frame of the menu: the column of categories has the focus when it's shown again
        backConsumed = false;
        UpdateLayout();
        if (!GetActiveCategories())
            categoryFocus = true;
        if (!HasTabs(state))
            return CallProcessTabs(state, inside);

        PrepareTabs();
        tabInputState = state;
        requestedTab = -1;
        mouseTabRequest = false;
        tabTransitionComplete = false;

        auto hovered = HitTestTab();
        auto current = static_cast<uint32_t>(mousePosition[2]);
        auto previous = static_cast<uint32_t>(mousePosition[1]);
        if ((current & (current ^ previous) & 1) != 0 && hasTabModal() == 0 && !confirming &&
            (FindPage(hovered) || (hovered == DisplayScreen && activePage != -1)))
        {
            requestedTab = hovered;
            mouseTabRequest = true;
            inside = 0;
        }

        auto result = CallProcessTabs(state, inside);
        tabInputState = nullptr;
        tabTextEnabled = false;
        mouseTabRequest = false;
        requestedTab = -1;
        return result;
    }

    static uint8_t __cdecl QueryInput(int32_t input, uint8_t sound, uint8_t flags, uint8_t force, uint8_t tabsInput, uint8_t unknown, uint8_t repeat)
    {
        if ((blockAccept || swallowFrames > 0) && input == 8)
            return 0;
        if (swallowFrames > 0 && input == 11)
            return 0;
        if (tabInputState && mouseTabRequest && (input == 2 || input == 8))
        {
            if (input == 2)
                *reinterpret_cast<int32_t*>(tabInputState + 0x18) = NoTab;
            return input == 2 || tabTransitionComplete ? 1 : 0;
        }

        auto result = queryInput.ccall<uint8_t>(input, sound, flags, force, tabsInput, unknown, repeat);

        if (result != 0 && input == 11)
        {
            if ((checkingBack || inputMenu >= 0) && ConsumeBack())
                return 0;
            pendingPage = -1;
            // Back from a page opened from a row returns to it, from a category of a screen it leaves the screen
            if (auto page = FindPage(activePage); page && BackTarget(*page).first != -1)
            {
                if (checkingBack)
                {
                    submenuBack = true;
                    return 0;
                }
                // The title menu doesn't always check Back through CheckForBackInput: the page goes back to
                // its parent after the menu input
                if (IsTitleScreen(activeHost))
                {
                    pendingTitleBack = true;
                    return 0;
                }
            }
        }

        if (result != 0 && tabInputState && (input == 2 || input == 3))
        {
            auto current = GetRootScreen(GetLogicalScreen(*reinterpret_cast<int32_t*>(tabInputState + 0x18)));
            auto next = AdjacentTab(current, input == 2 ? -1 : 1);
            if (activePage != -1 || FindPage(next))
            {
                requestedTab = next;
                *reinterpret_cast<int32_t*>(tabInputState + 0x18) = NoTab;
            }
        }
        return result;
    }

    static int32_t __fastcall CheckForBackInput(uint8_t* state, void*)
    {
        auto previousChecking = checkingBack;
        auto previousBack = submenuBack;
        tabState = state;
        checkingBack = true;
        submenuBack = false;
        auto result = CallCheckForBackInput(state);
        auto back = submenuBack;
        checkingBack = previousChecking;
        submenuBack = previousBack;
        if (auto page = FindPage(activePage); back && page)
            GoBack(*page);
        return result;
    }

    static int32_t __cdecl SwitchToNewScreen(int32_t menu, int32_t screen, int32_t row)
    {
        tabTransitionComplete = tabInputState != nullptr;
        if (screen == DisplayScreen && pendingPage != -1)
        {
            ActivatePage(pendingPage);
            row = 0;
        }
        else if (screen != activeHost || (tabInputState && requestedTab == DisplayScreen))
            RestoreDisplay();
        pendingPage = -1;
        return switchToNewScreen.ccall<int32_t>(menu, screen, row);
    }

    static int32_t __fastcall DrawTabs(uint8_t* state, void*)
    {
        tabState = state;
        auto hovered = HitTestTab();
        for (auto& tab : tabs)
            tab.visible = false;
        if (!HasTabs(state))
            return CallDrawTabs(state);

        PrepareTabs();
        tabDrawState = state;
        auto& selected = *reinterpret_cast<int32_t*>(state + 0x18);
        auto saved = selected;
        auto hover = *tabHover;
        if (FindPage(hovered))
            *tabHover = NoTab;
        if (activePage != -1 && selected == DisplayScreen)
        {
            auto root = GetRootScreen(activePage);
            selected = FindPage(root) ? NoTab : root;
        }

        auto result = CallDrawTabs(state);
        selected = saved;
        *tabHover = hover;
        tabDrawState = nullptr;
        tabTextEnabled = false;
        return result;
    }

    static const wchar_t* __fastcall GetTabText(void* text, void* edx, const char* key)
    {
        if (tabTextEnabled)
            for (auto& tab : tabs)
                if (tab.id < 73 && key == screens[tab.id].header)
                    return tabText[tab.id].data();
        if (key)
            if (auto found = CText::FindMenuText(GetHash(key)))
                return found;
        return getTabText.fastcall<const wchar_t*>(text, edx, key);
    }

    static void __cdecl SetTabScale(float x, float y)
    {
        setTabScale(x, y);
        if (!tabTextEnabled)
            return;
        float stock = 0.0f;
        float added = 0.0f;
        for (auto& tab : tabs)
        {
            auto width = measureTab.ccall<float>(GetTabLabel(tab), uint8_t(1));
            (FindPage(tab.id) ? added : stock) += width;
        }
        if (stock > 0.0f && added > 0.0f)
        {
            auto count = static_cast<float>(customTabCount);
            tabSpacing = std::min(tabSpacing, stock / (2.0f * count));
            auto available = stock - tabSpacing * count;
            setTabScale(x * available / (stock + added), y);
        }
    }

    static std::vector<Tab>::iterator FindTabByText(const wchar_t* text)
    {
        return std::find_if(tabs.begin(), tabs.end(), [text](const Tab& tab) { return tab.id < 73 && text == tabText[tab.id].data(); });
    }

    // A stock tab followed by custom tabs measures and prints them together
    static float __cdecl MeasureTab(const wchar_t* text, uint8_t full)
    {
        if (!tabTextEnabled)
            return measureTab.ccall<float>(text, full);
        auto found = FindTabByText(text);
        if (found == tabs.end())
            return measureTab.ccall<float>(text, full);
        auto width = measureTab.ccall<float>(GetTabLabel(*found), uint8_t(1));
        for (auto tab = found + 1; tab != tabs.end() && FindPage(tab->id); ++tab)
            width += tabSpacing + measureTab.ccall<float>(GetTabLabel(*tab), uint8_t(1));
        return width;
    }

    static void __cdecl PrintTab(float x, float y, const wchar_t* text, int32_t first, int32_t last)
    {
        auto found = tabDrawState ? FindTabByText(text) : tabs.end();
        if (found == tabs.end())
        {
            printTab.ccall<void>(x, y, text, first, last);
            return;
        }

        auto height = getTabHeight();
        auto mouseX = mousePosition[0] * mouseScale[0];
        auto mouseY = mousePosition[3] * mouseScale[2];
        for (auto tab = found; tab != tabs.end(); ++tab)
        {
            if (tab != found && !FindPage(tab->id))
                break;
            auto width = measureTab.ccall<float>(GetTabLabel(*tab), uint8_t(1));
            tab->left = x;
            tab->top = y;
            tab->right = x + width;
            tab->bottom = y + height;
            tab->visible = true;
            if (tab != found)
            {
                uint32_t color;
                bool hovered = mouseX > tab->left && mouseX < tab->right && mouseY > tab->top && mouseY < tab->bottom;
                auto palette = tab->id == GetRootScreen(activePage) ? (tabDrawState[1] != 0 ? 63 : 62) : (hovered ? 1 : 59);
                getHudColour(&color, palette);
                setTabColor(color);
            }
            printTab.ccall<void>(x, y, GetTabLabel(*tab), first, last);
            x += width + tabSpacing;
        }
    }

    // Submenu rows show the page's text, which is longer than a label. Empty lines have no text, so the menu can't
    // select them.
    static const char* __cdecl CopyRowLabel(const char* label, wchar_t* text)
    {
        auto& options = screens[*currentScreen].options;
        for (int32_t row = 0; row < options.count; ++row)
        {
            if (label != options.data[row].label)
                continue;
            if (IsGap(options.data[row]))
            {
                text[0] = L'\0';
                return nullptr;
            }
            if (IsHeading(options.data[row]))
            {
                auto heading = CText::FindMenuText(GetHash(label));
                wcsncpy_s(text, 60, heading ? heading : L"", _TRUNCATE);
                return nullptr;
            }
            if (auto button = FindButton(row); button >= 0)
            {
                if (auto page = FindPage(dynamicOptions[button].submenu))
                {
                    // The row's text has room for 59 characters, the mark of rows that open a page included
                    std::wstring name = GetPageName(*page);
                    name = name.substr(0, 59 - std::wcslen(SubmenuMark)) + SubmenuMark;
                    wcsncpy_s(text, 60, name.c_str(), _TRUNCATE);
                    return nullptr;
                }
            }
            break;
        }
        return copyRowLabel(label, text);
    }

    static void InitializeTabs()
    {
        auto branch = [](void* address) { return injector::GetBranchDestination(address).as_int(); };
        auto resetRows = [](SafetyHookContext&) { ResetPages(); };

        // Where the executables differ: tab text and drawing, mouse, background of the Display screen, whether
        // the game renders behind the menu, and the tab, back input and row label functions
        uint8_t* gameVisibilityHookAddress = nullptr;
        void* processTabsAddress = nullptr;
        void* drawTabsAddress = nullptr;
        void* checkForBackAddress = nullptr;
        void* switchScreenAddress = nullptr;
        void* getTabTextAddress = nullptr;
        void* printTabAddress = nullptr;
        void* queryInputAddress = nullptr;
        std::array<void*, 2> scaleCalls{};
        void* rowLabel = nullptr;
        if (legacyExecutable)
        {
            auto print = hook::pattern("B9 ? ? ? ? E8 ? ? ? ? D9 44 24 2C 50 83 EC 08 D9 5C 24 04 D9 84 24 ? ? ? ? D9 1C 24 E8 ? ? ? ? 83 C4 14 83 3D").get_first<uint8_t>();
            tabTextObject = *reinterpret_cast<void**>(print + 1);
            getTabTextAddress = reinterpret_cast<void*>(branch(print + 5));
            printTabAddress = reinterpret_cast<void*>(branch(print + 0x20));
            tabHover = *reinterpret_cast<int32_t**>(print + 0x2A);
            setTabColor = reinterpret_cast<decltype(setTabColor)>(branch(print - 0x11));

            // 1.1.2.0, 1.0.8.0: same layout, other stack offset
            auto mouse = find_pattern("E8 ? ? ? ? F3 0F 2A 05 ? ? ? ? D9 5C 24 1C F3 0F 59 05",
                "E8 ? ? ? ? F3 0F 2A 05 ? ? ? ? D9 5C 24 28 F3 0F 59 05").get_first<uint8_t>();
            getTabHeight = reinterpret_cast<decltype(getTabHeight)>(branch(mouse));
            mousePosition = *reinterpret_cast<int32_t**>(mouse + 9) - 3;
            mouseScale = *reinterpret_cast<float**>(mouse + 0x15) - 2;

            pageBackground = reinterpret_cast<uintptr_t>(hook::pattern("A1 ? ? ? ? 83 F8 08 74 0C 83 F8 31").get_first());
            auto backgroundBranch = hook::pattern("83 F8 08 0F 85 ? ? ? ? E8 ? ? ? ? 84 C0 74 1A").get_first<uint8_t>();
            drawPageBackground = reinterpret_cast<uintptr_t>(backgroundBranch + 9 + *reinterpret_cast<int32_t*>(backgroundBranch + 5));
            afterPageBackground = reinterpret_cast<uintptr_t>(hook::pattern("EB 04 8A 5C 24 10 80 3D ? ? ? ? 00 0F 85").get_first(2));

            auto gameVisibility = hook::pattern("39 35 ? ? ? ? 75 07 C6 05 ? ? ? ? 01").get_first<uint8_t>();
            renderGame = *reinterpret_cast<uint8_t**>(gameVisibility + 0x0A);
            gameVisibilityHookAddress = gameVisibility + 0x0F;
            frontendActive = *hook::pattern("A0 ? ? ? ? C6 05 ? ? ? ? 01 8D 73 03").get_first<uint8_t*>(1);

            auto drawScale = hook::pattern("D9 1C 24 E8 ? ? ? ? 6A 01 E8 ? ? ? ? 83 C4 0C 8D 44 24 18 6A 03 50 E8").get_first<uint8_t>();
            scaleCalls = { hook::pattern("D9 1C 24 E8 ? ? ? ? 6A 01 E8 ? ? ? ? 83 C4 0C BE 03 00 00 00").get_first(3), drawScale + 3 };
            getTabWidget = reinterpret_cast<decltype(getTabWidget)>(branch(drawScale + 0x19));
            getHudColour = reinterpret_cast<decltype(getHudColour)>(hook::pattern("56 8B 74 24 0C 8B 04 B5 ? ? ? ? 8B C8 C1 E9 18").get_first());
            isNetworkGame = reinterpret_cast<decltype(isNetworkGame)>(hook::pattern("53 8A 1D ? ? ? ? 84 DB 74 37 A1").get_first());
            auto query = hook::pattern("83 EC 0C 80 3D ? ? ? ? 00 55 8B 6C 24 14 75 0C 83 FD 0B").get_first<uint8_t>();
            queryInputAddress = query;
            hasTabModal = reinterpret_cast<decltype(hasTabModal)>(branch(query + 0x1E));
            hideRow = reinterpret_cast<decltype(hideRow)>(hook::pattern("56 8B 74 24 08 83 FE A6 74 29 8B 0C B5").get_first());

            // 1.1.2.0, 1.0.8.0: other stack frame size
            processTabsAddress = find_pattern("81 EC DC 00 00 00 A1 ? ? ? ? 33 C4 89 84 24 D4 00 00 00 80 BC 24 E4 00 00 00 00",
                "81 EC D4 00 00 00 A1 ? ? ? ? 33 C4 89 84 24 CC 00 00 00 80 BC 24 DC 00 00 00 00").get_first();
            drawTabsAddress = hook::pattern("81 EC 90 00 00 00 80 3D ? ? ? ? 00 53 55 8B AC 24 9C 00 00 00").get_first();
            checkForBackAddress = hook::pattern("80 3D ? ? ? ? 00 0F 85 FA 00 00 00 80 3D").get_first();
            switchScreenAddress = reinterpret_cast<void*>(branch(hook::pattern("0F B6 55 18 6A 00 52 6A 00 E8").get_first(9)));
            rowLabel = hook::pattern("83 C0 01 50 E8 ? ? ? ? 83 C4 08 C7 46 FC FF FF FF FF").get_first(4);

            // The screens' rows are freed in two places
            auto reset = hook::pattern("81 FE ? ? ? ? 7C CE BE ? ? ? ? 88 5E F0").count(2);
            resetScreenRowsHook = safetyhook::create_mid(reset.get(0).get<void>(8), resetRows);
            resetScreenRowsHook2 = safetyhook::create_mid(reset.get(1).get<void>(8), resetRows);
        }
        else
        {
            auto print = hook::pattern("83 C4 14 83 3D ? ? ? ? 00 75").get_first<uint8_t>();
            tabTextObject = *reinterpret_cast<void**>(print - 0x32);
            getTabTextAddress = reinterpret_cast<void*>(branch(print - 0x25));
            printTabAddress = reinterpret_cast<void*>(branch(print - 5));
            tabHover = *reinterpret_cast<int32_t**>(print + 5);
            setTabColor = reinterpret_cast<decltype(setTabColor)>(hook::pattern("0F 2F C1 76 35 C1 E8 18").get_first(-0x2A));

            auto mouse = hook::pattern("D9 5C 24 58 66 0F 6E 1D").get_first<uint8_t>();
            getTabHeight = reinterpret_cast<decltype(getTabHeight)>(branch(mouse - 5));
            mousePosition = *reinterpret_cast<int32_t**>(mouse + 8) - 3;
            mouseScale = *reinterpret_cast<float**>(mouse + 0x1E) - 2;

            pageBackground = reinterpret_cast<uintptr_t>(hook::pattern("83 F8 08 74 17 0F B6 0D").get_first(-5));
            auto backgroundBranch = hook::pattern("83 F8 08 75 C1 E8").get_first<uint8_t>();
            drawPageBackground = reinterpret_cast<uintptr_t>(backgroundBranch + 5 + *reinterpret_cast<int8_t*>(backgroundBranch + 4));
            afterPageBackground = reinterpret_cast<uintptr_t>(hook::pattern("75 41 E8 ? ? ? ? 84 C0 75 38").get_first(-7));

            auto gameVisibility = hook::pattern("83 FE 03 75 07 C6 05").get_first<uint8_t>();
            renderGame = *reinterpret_cast<uint8_t**>(gameVisibility + 7);
            gameVisibilityHookAddress = gameVisibility + 0x0C;
            frontendActive = *hook::pattern("8A 1D ? ? ? ? C6 05 ? ? ? ? 01 7F").get_first<uint8_t*>(2);

            scaleCalls = { hook::pattern("8D 44 24 68 6A 03 50").get_first(-0x0F), hook::pattern("8D 44 24 28 6A 03 50").get_first(-0x0F) };
            getTabWidget = reinterpret_cast<decltype(getTabWidget)>(branch(hook::pattern("8D 44 24 68 6A 03 50").get_first(7)));
            getHudColour = reinterpret_cast<decltype(getHudColour)>(hook::pattern("C1 EE 18 C1 E8 10 0F B6 C0").get_first(-0x10));
            isNetworkGame = reinterpret_cast<decltype(isNetworkGame)>(branch(hook::pattern("33 D2 6A 01 FF 35").get_first(-5)));
            auto query = hook::pattern("83 FE 0B 75 07 32 C0").get_first<uint8_t>();
            queryInputAddress = query - 0x11;
            hasTabModal = reinterpret_cast<decltype(hasTabModal)>(branch(query + 0x0D));
            hideRow = reinterpret_cast<decltype(hideRow)>(hook::pattern("88 84 0E E4 33 00 00").get_first(-0x21));

            processTabsAddress = hook::pattern("83 EC 78 80 7C 24 7C 00").get_first();
            drawTabsAddress = hook::pattern("89 4C 24 18 0F 85 ? ? ? ? 6A 07").get_first(-0x1F);
            checkForBackAddress = hook::pattern("00 55 8B E9 0F 85 ? ? ? ? 80 3D").get_first(-6);
            switchScreenAddress = hook::pattern("8B 44 24 08 83 EC 10 A3").get_first();
            rowLabel = hook::pattern("8D 47 01 03 C1 50 E8").get_first(6);

            resetScreenRowsHook = safetyhook::create_mid(hook::pattern("7C D5 5F 5E C3").get_first(-0x2E), resetRows);

            // The column of categories: the menu's position, fonts, the width of its first column and its drawing
            bodyPosition = *hook::pattern("8B 01 8D 34 FD ? ? ? ? 8D 14 FD").get_first<float*>(5);
            auto color = hook::pattern("0F 2F C1 76 35 C1 E8 18").get_first<uint8_t>();
            fontStates = *reinterpret_cast<uint8_t**>(color - 4);
            getFontContext = reinterpret_cast<decltype(getFontContext)>(branch(color - 0x2A));
            selectRow = reinterpret_cast<decltype(selectRow)>(hook::pattern("6A 01 FF 74 24 18 FF 34 BD").get_first(-0x15));
            setFontStyle = reinterpret_cast<decltype(setFontStyle)>(hook::pattern("8B 5C 24 0C 53 8D 34 C0").get_first(-7));
            setFontOrientation = reinterpret_cast<decltype(setFontOrientation)>(branch(hook::pattern("8D 44 24 28 6A 03 50").get_first(-8)));
            setFontWrap = reinterpret_cast<decltype(setFontWrap)>(hook::pattern("F3 0F 10 44 24 08 F3 0F 11 04 C5 ? ? ? ? C3").get_first(-0x17));
            // The menu's title: proportional, an outline of 1 and its colour
            auto outline = hook::pattern("6A 01 E8 ? ? ? ? 83 C4 08 C7 04 24 00 00 80 3F E8 ? ? ? ? 68 00 00 00 FF E8").get_first<uint8_t>();
            setFontEdge = reinterpret_cast<decltype(setFontEdge)>(branch(outline + 0x11));
            setFontEdgeColor = reinterpret_cast<decltype(setFontEdgeColor)>(branch(outline + 0x1B));
            auto columns = hook::pattern("8D 44 24 4C 50 E8 ? ? ? ? F3 0F 10 00").get_first<uint8_t>();
            getColumnWidth = reinterpret_cast<decltype(getColumnWidth)>(branch(columns + 5));
            adjustForWidescreen = reinterpret_cast<decltype(adjustForWidescreen)>(branch(columns + 0x27));
            menuDraw = safetyhook::create_inline(hook::pattern("83 38 00 75 09 89 4C 24 04").get_first(-0x0F), MenuDraw);
            // The info text of Graphics: its place and font, and the index of the row it describes
            auto info = hook::pattern("8D 44 24 40 6A 08 50 E8 ? ? ? ? 8D 44 24 58 50 E8 ? ? ? ? F3 0F 10 00 83 C4 0C 8D 44 24 50 6A 16 50");
            auto infoWidth = hook::pattern("50 E8 ? ? ? ? F3 0F 10 00 83 C4 04 8D 44 24 28 6A 00 50 6A 00 6A 02");
            auto infoRow = hook::pattern("83 C4 04 83 F8 A4 0F 84 ? ? ? ? 83 F8 0B 0F 87");
            if (!info.empty() && !infoWidth.empty() && !infoRow.empty())
            {
                getInfoPosition = reinterpret_cast<decltype(getInfoPosition)>(branch(info.get_first<uint8_t>(0x11)));
                setFontDropShadow = reinterpret_cast<decltype(setFontDropShadow)>(branch(info.get_first<uint8_t>(0x7E)));
                getInfoLabelWidth = reinterpret_cast<decltype(getInfoLabelWidth)>(branch(infoWidth.get_first<uint8_t>(1)));
                infoTextRow = safetyhook::create_mid(infoRow.get_first(), InfoTextRow);
            }
            categoriesEnabled = true;

            // The language: FillMenu hides its row of Display (9, 10 in TLAD) and of the title menu's Display (6)
            auto language = hook::pattern("83 F9 08 75 19 83 3D ? ? ? ? 01 B8 09 00 00 00 B9 0A 00 00 00 0F 44 C1 6A 01 50 EB 09 83 F9 3D 75 18");
            if (!language.empty())
            {
                // jnz over the Display check -> jmp past both checks
                injector::WriteMemory<uint8_t>(language.get_first<uint8_t>(3), 0xEB, true);
                injector::WriteMemory<uint8_t>(language.get_first<uint8_t>(4), 0x36, true);
            }
        }
        setTabScale = reinterpret_cast<decltype(setTabScale)>(branch(scaleCalls[0]));

        if (legacyExecutable)
        {
            processTabs = safetyhook::create_inline(processTabsAddress, ProcessTabsLegacy);
            drawTabs = safetyhook::create_inline(drawTabsAddress, DrawTabsLegacy);
            checkForBackInput = safetyhook::create_inline(checkForBackAddress, CheckForBackInputLegacy);
        }
        else
        {
            processTabs = safetyhook::create_inline(processTabsAddress, ProcessTabs);
            drawTabs = safetyhook::create_inline(drawTabsAddress, DrawTabs);
            checkForBackInput = safetyhook::create_inline(checkForBackAddress, CheckForBackInput);
        }
        queryInput = safetyhook::create_inline(queryInputAddress, QueryInput);
        switchToNewScreen = safetyhook::create_inline(switchScreenAddress, SwitchToNewScreen);
        getTabText = safetyhook::create_inline(getTabTextAddress, GetTabText);
        printTab = safetyhook::create_inline(printTabAddress, PrintTab);
        measureTab = safetyhook::create_inline(hook::pattern("D9 EE C3 89 44 24 04").get_first(-8), MeasureTab);
        for (auto call : scaleCalls)
            injector::MakeCALL(call, SetTabScale, true);

        copyRowLabel = reinterpret_cast<decltype(copyRowLabel)>(branch(rowLabel));
        injector::MakeCALL(rowLabel, CopyRowLabel, true);

        // Tab changes are handled by OnTabTransition from MenuBackgroundHook4 at the same place
        // The game keeps rendering behind pages created with displayGame
        gameVisibilityHook = safetyhook::create_mid(gameVisibilityHookAddress, [](SafetyHookContext&)
        {
            if (auto page = ShownPage(); page && page->displayGame && activeHost == DisplayScreen && *frontendActive != 0 &&
                *currentScreen == DisplayScreen)
                *renderGame = 1;
        });
        // mov eax, [current screen]; pages get the menu background unless they show the game, categories look like
        // their screen
        pageBackgroundHook = safetyhook::create_mid(pageBackground, [](SafetyHookContext& regs)
        {
            regs.eax = static_cast<uintptr_t>(*currentScreen);
            if (auto page = ShownPage(); page && !page->category && activeHost == DisplayScreen && *currentScreen == DisplayScreen)
                regs.eip = page->displayGame ? afterPageBackground : drawPageBackground;
            else
                regs.eip = pageBackground + 5;
        });

        tabsInitialized = true;
    }

    static void InitializeMenuAPI()
    {
        // Code-defined toggles/enums/sliders use the same preference registry
        // and display tables as XML options. Row positioning needs correction
        // when preceding rows are hidden (see the supplied pausemenu example).
        if (legacyExecutable)
        {
            auto scroll = hook::pattern("56 8B 74 24 08 85 F6 57 0F 8C ? ? ? ? 8B 15 ? ? ? ? 8D 04 52").get_first<uint8_t>();
            currentScreen = *reinterpret_cast<int32_t**>(scroll + 0x10);
            screens = reinterpret_cast<SettingsTables::Screen*>(*reinterpret_cast<uintptr_t*>(scroll + 0x1F) - 0x14);
            appendOptionLegacy = hook::pattern("0F B7 46 06 66 39 46 04 0F 85 86 00 00 00 83 C0 10").get_first();
            appendDisplayLegacy = hook::pattern("0F B7 46 06 66 39 46 04 0F 85 8B 00 00 00 83 C0 10").get_first();
            auto process = hook::pattern("8B 04 BD ? ? ? ? 50 C7 05 ? ? ? ? FF FF FF 7F E8 ? ? ? ? A1").get_first<uint8_t>(-0x1E);
            menuHandles = *reinterpret_cast<int32_t**>(process + 0x21);
            menuInstances = *hook::pattern("8B 80 1C 36 00 00 C3 33 C0").get_first<uint8_t**>(-0x11);
            rowHeight = *hook::pattern("F3 0F 10 15 ? ? ? ? 0F 2F 4C 24 18").get_first<float*>(4);
            sliderDrawRow = safetyhook::create_mid(hook::pattern("6A 00 E8 ? ? ? ? 83 C4 04 84 C0 53 6A 3D").get_first(), [](SafetyHookContext& regs) { AdjustSliderRow(regs, true); });
            sliderMouseRow = safetyhook::create_mid(hook::pattern("8D 54 24 30 6A 0F 52 E8").get_first(), [](SafetyHookContext& regs) { AdjustSliderRow(regs, false); });
            fillMenu = safetyhook::create_inline(hook::pattern("83 EC 40 A1 ? ? ? ? 53 8D 04 40 55 0F B7 2C C5").get_first(), FillMenu);
            scrollMenu = safetyhook::create_inline(scroll, ScrollMenu);
            selectOption = safetyhook::create_inline(hook::pattern("83 EC 10 57 8B 7C 24 1C 85 FF 0F 8C ? ? ? ? 8B 0D").get_first(), SelectOption);
            processMenu = safetyhook::create_inline(process, ProcessMenu);
            // The game's warning screen for its own questions (1.1.2.0, 1.0.8.0)
            if (auto warning = hook::pattern("8B 44 24 04 50 C7 05 ? ? ? ? 01 00 00 00 E8 ? ? ? ? 8B 4C 24 0C 51 E8"); !warning.empty())
            {
                setWarningMessage = reinterpret_cast<decltype(setWarningMessage)>(warning.get_first());
                warningState = *warning.get_first<int32_t*>(7);
            }
            // The number of MENU_DISPLAY_VALUE_SLIDERBAR is printed from a buffer at esp+0xF0, the row offset is in EDI
            auto valueText = hook::pattern("83 C4 0C 6A FF 6A FF 8D 8C 24 F8 00 00 00 51 E9");
            if (!valueText.empty())
                valueTextHook = safetyhook::create_mid(valueText.get_first(3), [](SafetyHookContext& regs) { SetValueText(regs.edi, regs.esp + 0xF0); });
            InitializeTabs();
            return;
        }
        auto scroll = hook::pattern("0F B7 14 CD ? ? ? ? 8B C2").get_first<uint8_t>(-0x15);
        currentScreen = *reinterpret_cast<int32_t**>(scroll + 0x0E);
        screens = reinterpret_cast<SettingsTables::Screen*>(*reinterpret_cast<uintptr_t*>(scroll + 0x19) - 0x14);
        appendOption = reinterpret_cast<decltype(appendOption)>(hook::pattern("6B D2 16 66 89 46 06").get_first(-0x14));
        appendDisplay = reinterpret_cast<decltype(appendDisplay)>(hook::pattern("56 8B F1 0F B7 46 06 66 39 46 04 75 77 53 83 C0 10").get_first());
        menuHandles = *hook::pattern("C7 05 ? ? ? ? FF FF FF 7F FF 34 BD").get_first<int32_t*>(0x0D);
        menuInstances = *hook::pattern("8B 81 1C 36 00 00 C3 33 C0").get_first<uint8_t**>(-0x11);
        rowHeight = *hook::pattern("F3 0F 10 1D ? ? ? ? 0F 2F C2").get_first<float*>(4);
        sliderDrawRow = safetyhook::create_mid(hook::pattern("84 C0 57 6A 3D 8D").get_first(-0x0C), [](SafetyHookContext& regs) { AdjustSliderRow(regs, true); });
        sliderMouseRow = safetyhook::create_mid(hook::pattern("8D 44 24 24 6A 0F 50").get_first(), [](SafetyHookContext& regs) { AdjustSliderRow(regs, false); });
        fillMenu = safetyhook::create_inline(hook::pattern("8D 04 40 BA 84 00 00 00").get_first(-0x10), FillMenu);
        scrollMenu = safetyhook::create_inline(scroll, ScrollMenu);
        selectOption = safetyhook::create_inline(hook::pattern("0F B7 3C D5 ? ? ? ? 3B F7").get_first(-0x25), SelectOption);
        processMenu = safetyhook::create_inline(hook::pattern("C7 05 ? ? ? ? FF FF FF 7F FF 34 BD").get_first(-0x1E), ProcessMenu);
        // The game's warning screen for its own questions: title (not shown), text key, keys (0x48: yes and no), ...
        if (auto warning = hook::pattern("FF 74 24 04 C7 05 ? ? ? ? 01 00 00 00 E8 ? ? ? ? FF 74 24 0C E8"); !warning.empty())
        {
            setWarningMessage = reinterpret_cast<decltype(setWarningMessage)>(warning.get_first());
            warningState = *warning.get_first<int32_t*>(6);
        }
        // The number of MENU_DISPLAY_VALUE_SLIDERBAR is printed from a buffer at esp+0xF0, the row offset is in EBX
        auto valueText = hook::pattern("83 C4 0C 8D 84 24 F0 00 00 00 6A FF 6A FF 50 E9");
        if (!valueText.empty())
            valueTextHook = safetyhook::create_mid(valueText.get_first(3), [](SafetyHookContext& regs) { SetValueText(regs.ebx, regs.esp + 0xF0); });
        InitializeTabs();
    }

    static std::optional<int32_t> FindRegisteredName(const char* name, bool preferences)
    {
        if (!name || !*name)
            return std::nullopt;
        if (preferences)
            for (auto& pref : aMenuPrefs)
                if (_stricmp(pref.name, name) == 0)
                    return pref.prefID;
        for (auto& [key, id] : displayIDs)
            if (_stricmp(key.c_str(), name) == 0)
                return id;
        return std::nullopt;
    }

    static int32_t __fastcall LookupName(const char* name, void*)
    {
        if (!name || !*name)
            return 0;
        if (auto id = FindRegisteredName(name, true))
            return *id;
        return nameLookup.fastcall<int32_t>(name, nullptr);
    }

    static int32_t __fastcall LookupDisplay(const char* name, void*)
    {
        if (!name || !*name)
            return 0;
        if (auto id = FindRegisteredName(name, false))
            return *id;
        return displayLookup.fastcall<int32_t>(name, nullptr);
    }

    void InitializeTables()
    {
        auto prefPattern = hook::pattern("FF 34 FD ? ? ? ? 56 E8 ? ? ? ? 83 C4 08 85 C0 0F 84 ? ? ? ? 47 81 FF");
        legacyExecutable = prefPattern.empty();
        if (legacyExecutable)
            prefPattern = hook::pattern("8B 04 F5 ? ? ? ? 50 57 E8 ? ? ? ? 83 C4 08 85 C0 74 ? 83 C6 01 81 FE");
        auto prefNames = prefPattern.get_first<uint8_t>();
        auto originalNames = reinterpret_cast<MenuPrefs*>(*reinterpret_cast<uintptr_t*>(prefNames + 3) - offsetof(MenuPrefs, name));
        auto originalCount = *reinterpret_cast<uint32_t*>(prefNames + (legacyExecutable ? 26 : 27));
        aMenuPrefs.assign(originalNames, originalNames + originalCount);
        firstCustomID = 0;
        for (auto& pref : aMenuPrefs)
            firstCustomID = std::max(firstCustomID, static_cast<int32_t>(pref.prefID) + 1);
        if (firstCustomID != originalCount)
            throw std::runtime_error("Unexpected native preference ID range");

        auto originalPrefs = *find_pattern("89 1C 95 ? ? ? ? E8 ? ? ? ? A1 ? ? ? ? 83 C4 04 8D 04 40", "89 1C 8D ? ? ? ? E8 ? ? ? ? A1 ? ? ? ? 8D 0C 40").get_first<int32_t*>(3);
        auto originalScalers = *find_pattern("8B 44 24 04 8B 0C 85 ? ? ? ? B8 01 00 00 00 85 C9 0F 4F C1 C3", "8B 44 24 04 8B 04 85 ? ? ? ? 85 C0 7F 05").get_first<int32_t*>(7);
        auto originalDisplays = *find_pattern<2>("8D 0C 40 8D 1C 8D ? ? ? ? 8B 4D 04 E8", "8D 2C 40 8D 2C AD ? ? ? ? E8 ? ? ? ? 55 56").count(2).get(0).get<SettingsTables::Display*>(6);
        auto enumNames = find_pattern("FF 34 FD ? ? ? ? 56 E8 ? ? ? ? 83 C4 08 85 C0 0F 84 ? ? ? ? 47 83 FF 3C", "8B 14 F5 ? ? ? ? 52 57 E8 ? ? ? ? 83 C4 08 85 C0 74 ? 83 C6 01 83 FE 3C").get_first<uint8_t>();
        auto originalEnumNames = reinterpret_cast<MenuPrefs*>(*reinterpret_cast<uintptr_t*>(enumNames + 3) - offsetof(MenuPrefs, name));
        for (int32_t i = 0; i < 60; ++i)
            displayIDs.emplace(originalEnumNames[i].name, originalEnumNames[i].prefID);

        std::copy_n(originalScalers, originalCount, SettingsTables::scalers.begin());
        std::copy_n(originalDisplays, 60, SettingsTables::displays.begin());
        // Leave native preferences in place: gameplay, save/load and defaults
        // access individual globals directly. Custom values live in CSetting;
        // only the shared frontend readers/writers need to call Get/Set.
        mPrefs = originalPrefs;

        // Relocate only the XML helper tables. Match instructions, not every
        // occurrence of an address, and collect operands before patching them.
        std::vector<std::pair<uint32_t*, uintptr_t>> references;
        auto replaceReferences = [&](const std::string& signature, size_t count, ptrdiff_t operandOffset, uintptr_t replacement)
        {
            auto pattern = hook::pattern(signature).count(count);
            pattern.for_each_result([&](hook::pattern_match match)
            {
                references.emplace_back(match.get<uint32_t>(operandOffset), replacement);
            });
        };

        // Scaler parser writes and lookup differ between the executables.
        auto scalers = reinterpret_cast<uintptr_t>(SettingsTables::scalers.data());
        auto oldScalers = pattern_str(to_bytes(reinterpret_cast<uintptr_t>(originalScalers)));
        if (legacyExecutable)
        {
            // The old aMenuPrefs2 patch was this int32_t scaler table, not a
            // second name registry. XML parsing and input must share its storage.
            replaceReferences("89 14 8D " + oldScalers + "66 8B 0D", 1, 3, scalers);
            replaceReferences("89 3C AD " + oldScalers + "0F B7 3A 83 C6 01", 1, 3, scalers);
            replaceReferences("89 04 8D " + oldScalers + "8B 74 24 10", 1, 3, scalers);
            replaceReferences("8B 04 85 " + oldScalers + "85 C0 7F 05", 1, 3, scalers);
        }
        else
        {
            replaceReferences("89 0C 85 " + oldScalers + "66 8B 0D", 1, 3, scalers);
            replaceReferences("89 14 8D " + oldScalers + "0F B7 0E 43", 1, 3, scalers);
            replaceReferences("89 0C 85 " + oldScalers + "F3 0F 7E 44 24 2C", 2, 3, scalers);
            replaceReferences("8B 0C 85 " + oldScalers + "B8 01 00 00 00", 1, 3, scalers);
        }

        // Display parser, readers and lifetime instructions.
        // These operands address fields of the first record; subsequent records
        // are indexed using the native 12-byte stride.
        auto displays = reinterpret_cast<uintptr_t>(SettingsTables::displays.data());
        auto oldDisplays = reinterpret_cast<uintptr_t>(originalDisplays);
        auto ids = pattern_str(to_bytes(oldDisplays));
        auto data = pattern_str(to_bytes(oldDisplays + 4));
        auto counts = pattern_str(to_bytes(oldDisplays + 8));
        auto capacity = pattern_str(to_bytes(oldDisplays + 10));
        if (legacyExecutable)
        {
            replaceReferences("8D 2C AD " + ids, 2, 3, displays);
            replaceReferences("B9 " + ids, 5, 1, displays);
            replaceReferences("B8 " + ids, 1, 1, displays);
            replaceReferences("BE " + data, 2, 1, displays + 4);
            replaceReferences("8B 0C 8D " + data, 2, 3, displays + 4);
            replaceReferences("8B 89 " + data, 2, 2, displays + 4);
            replaceReferences("8B 90 " + data, 2, 2, displays + 4);
            replaceReferences("8B 80 " + data, 1, 2, displays + 4);
            replaceReferences("8B 14 95 " + data, 1, 3, displays + 4);
            replaceReferences("B9 " + counts, 1, 1, displays + 8);
            replaceReferences("0F B7 94 09 " + counts, 3, 4, displays + 8);
            replaceReferences("0F B7 14 95 " + counts, 1, 4, displays + 8);
            replaceReferences("0F B7 94 00 " + counts, 2, 4, displays + 8);
            replaceReferences("0F B7 8C 00 " + counts, 1, 4, displays + 8);
            replaceReferences("B8 " + capacity, 1, 1, displays + 10);
        }
        else
        {
            replaceReferences("8D 1C 8D " + ids, 2, 3, displays);
            replaceReferences("B8 " + ids, 6, 1, displays);
            replaceReferences("BE " + data, 2, 1, displays + 4);
            replaceReferences("8B 04 BD " + data, 1, 3, displays + 4);
            replaceReferences("8B 04 95 " + data, 3, 3, displays + 4);
            replaceReferences("8B 04 85 " + data, 3, 3, displays + 4);
            replaceReferences("8B 04 9D " + data, 1, 3, displays + 4);
            replaceReferences("B8 " + counts, 1, 1, displays + 8);
            replaceReferences("0F B7 04 BD " + counts, 1, 4, displays + 8);
            replaceReferences("0F B7 04 95 " + counts, 4, 4, displays + 8);
            replaceReferences("0F B7 04 9D " + counts, 1, 4, displays + 8);
            replaceReferences("0F B7 1C 85 " + counts, 1, 4, displays + 8);
            replaceReferences("B8 " + capacity, 1, 1, displays + 10);
        }

        auto originalEnd = reinterpret_cast<uintptr_t>(originalDisplays + 60);
        auto newEnd = reinterpret_cast<uintptr_t>(SettingsTables::displays.data() + SettingsTables::DisplayCapacity);
        // The old end address also names screen headers: never replace it
        // indiscriminately.
        if (legacyExecutable)
        {
            replaceReferences("81 F9 " + pattern_str(to_bytes(originalEnd)) + "7C", 5, 2, newEnd);
            replaceReferences("3D " + pattern_str(to_bytes(originalEnd)) + "7C", 1, 1, newEnd);
            replaceReferences("81 FE " + pattern_str(to_bytes(originalEnd + 4)) + "7C", 2, 2, newEnd + 4);
            replaceReferences("81 F9 " + pattern_str(to_bytes(originalEnd + 8)) + "7C", 1, 2, newEnd + 8);
            replaceReferences("BF 3B 00 00 00 BE " + pattern_str(to_bytes(originalEnd + 4)) + "8D 64 24 00", 1, 6, newEnd + 4);
        }
        else
        {
            replaceReferences("3D " + pattern_str(to_bytes(originalEnd)) + "7C", 6, 1, newEnd);
            replaceReferences("81 FE " + pattern_str(to_bytes(originalEnd + 4)) + "7C", 2, 2, newEnd + 4);
            replaceReferences("3D " + pattern_str(to_bytes(originalEnd + 8)) + "7C", 1, 1, newEnd + 8);
            replaceReferences("BF 3B 00 00 00 BE " + pattern_str(to_bytes(originalEnd + 4)) + "66 83 7E FA 00 8D 76 F4", 1, 6, newEnd + 4);
        }

        // Display constructor/destructor counts and action lookup
        // bounds. Do not change the screen-table uses of the old end address.
        auto constructCount = find_pattern("B9 3B 00 00 00 B8 ? ? ? ? 8D 9B 00 00 00 00 33 D2 49", "B9 3B 00 00 00 B8 ? ? ? ? 33 D2 8D 64 24 00").get_first<uint32_t>(1);
        auto destructCount = find_pattern("BF 3B 00 00 00 BE ? ? ? ? 66 83 7E FA 00 8D 76 F4", "BF 3B 00 00 00 BE ? ? ? ? 8D 64 24 00 83 EE 0C").get_first<uint32_t>(1);
        std::array<uint8_t*, 2> actionBounds;
        if (legacyExecutable)
        {
            actionBounds[0] = hook::pattern("3C 3C 73 26 0F B6 C0 8D 04 40").get_first<uint8_t>(1);
            actionBounds[1] = hook::pattern("80 F9 3C 73 26 0F B6 C1").get_first<uint8_t>(2);
        }
        else
        {
            auto pattern = hook::pattern("3C 3C 73 24 0F B6 C0").count(2);
            for (size_t i = 0; i < actionBounds.size(); ++i)
                actionBounds[i] = pattern.get(i).get<uint8_t>(1);
        }
        auto lookup = find_pattern("56 8B F1 85 F6 75 04 33 C0 5E C3 57 33 FF", "85 FF 75 03 33 C0 C3 56 33 F6").get_first<uint8_t>();
        auto lookupDisplay = find_pattern("56 57 8B F9 85 FF 75 04 38 0F 74 4E", "85 FF 56 75 05 80 3F 00 74 53").get_first();

        for (auto [operand, value] : references)
            injector::WriteMemory(operand, value, true);
        injector::WriteMemory<uint32_t>(constructCount, SettingsTables::DisplayCapacity - 1, true);
        injector::WriteMemory<uint32_t>(destructCount, SettingsTables::DisplayCapacity - 1, true);
        for (auto bound : actionBounds)
            injector::WriteMemory<uint8_t>(bound, SettingsTables::DisplayCapacity, true);
        if (legacyExecutable)
        {
            // 1.1.2.0 passes the string in EDI, not ECX. Unknown names continue
            // through the original routine with all registers/stack intact.
            static auto lookupReturn = reinterpret_cast<uintptr_t>(lookup + 6);
            static auto nameHook = safetyhook::create_mid(lookup, [](SafetyHookContext& regs)
            {
                if (auto id = FindRegisteredName(reinterpret_cast<const char*>(regs.edi), true))
                {
                    regs.eax = *id;
                    regs.eip = lookupReturn;
                }
            });
            static auto displayHook = safetyhook::create_mid(lookupDisplay, [](SafetyHookContext& regs)
            {
                if (auto id = FindRegisteredName(reinterpret_cast<const char*>(regs.edi), false))
                {
                    regs.eax = *id;
                    regs.eip = lookupReturn;
                }
            });
        }
        else
        {
            nameLookup = safetyhook::create_inline(lookup, LookupName);
            displayLookup = safetyhook::create_inline(lookupDisplay, LookupDisplay);
        }
    }

    std::optional<std::string> GetPrefNameByID(auto prefID)
    {
        auto it = std::find_if(std::begin(aMenuPrefs), std::end(aMenuPrefs), [&prefID](auto& it)
        {
            return it.prefID == prefID;
        });
        if (it != std::end(aMenuPrefs))
            return std::string(it->name);
        return std::nullopt;
    }
    static std::optional<int32_t> GetPrefIDByName(auto prefName)
    {
        auto it = std::find_if(std::begin(aMenuPrefs), std::end(aMenuPrefs), [&prefName](auto& it)
        {
            return std::string_view(it.name) == prefName;
        });
        if (it != std::end(aMenuPrefs))
            return it->prefID;
        return std::nullopt;
    }
public:

    CSettings()
    {
        // Fusion Fix's own description and texts, for its rows in the menu
        GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            reinterpret_cast<LPCWSTR>(&SettingsPlugin), &ownPlugin.module);
        asi_settings::ReadModule(ownPlugin.module, ownPlugin.file);

        InitializeTables();

        // The preferences of the menu and of scripts (GET_NUMBER_OF_INSTANCES_OF_STREAMED_SCRIPT), by the setting of
        // the description that stores them. IDs and XML display records are independent of registration order.
        static constexpr std::pair<const char*, const char*> arr[] = {
            { "PREF_SKIP_INTRO",             "MAIN.SkipIntro" },
            { "PREF_SKIP_MENU",              "MAIN.SkipMenu" },
            { "PREF_FPS_LIMIT_PRESET",       "FRAMELIMIT.FpsLimitPreset" },
            { "PREF_BLOOM",                  "MAIN.Bloom" },
            { "PREF_CONSOLE_GAMMA",          "MISC.ConsoleGamma" },
            { "PREF_TIMECYC",                "MISC.ScreenFilter" },
            { "PREF_WINDOWMODE",             "MAIN.WindowMode" },
            { "PREF_DEFINITION",             "MAIN.Definition" },
            { "PREF_SHADOWFILTER",           "SHADOWS.ShadowFilter" },
            { "PREF_TREE_LIGHTING",          "MISC.TreeLighting" },
            { "PREF_TCYC_DOF",               "MISC.DepthOfField" },
            { "PREF_MOTIONBLUR",             "MAIN.MotionBlur" },
            { "PREF_LEDILLUMINATION",        "MISC.LightSyncRGB" },
            { "PREF_TREEALPHA",              "MISC.TreeAlpha" },
            { "PREF_SUNSHAFTS",              "MISC.SunShafts" },
            { "PREF_FPSCOUNTER",             "FRAMELIMIT.DisplayFpsCounter" },
            { "PREF_ALWAYSRUN",              "MISC.AlwaysRun" },
            { "PREF_ALTDIALOGUE",            "MISC.AltDialogue" },
            { "PREF_KBCAMCENTERDELAY",       "MISC.DelayBeforeCenteringCameraKB" },
            { "PREF_PADCAMCENTERDELAY",      "MISC.DelayBeforeCenteringCameraPad" },
            { "PREF_CUSTOMFOV",              "MISC.FieldOfView" },
            { "PREF_RAWINPUT",               "MISC.RawInput" },
            { "PREF_BUTTONS",                "MISC.Buttons" },
            { "PREF_CUTSCENEBORDERS",        "MISC.CutsceneBorders" },
            { "PREF_ANTIALIASING",           "MISC.Antialiasing" },
            { "PREF_UPDATE",                 "UPDATE.CheckForUpdates" },
            { "PREF_BLOCKONLOSTFOCUS",       "MAIN.BlockOnLostFocus" },
            { "PREF_TRANSPARENTMAPMENU",     "MISC.TransparentMapMenu" },
            { "PREF_EXTENDEDSNIPERCONTROLS", "MISC.ExtendedSniperControls" },
            { "PREF_TIMEDEVENTS",            "MISC.TimedEvents" },
            { "PREF_VOLUMETRICFOG",          "FOG.VolumetricFog" },
            { "PREF_TONEMAPPING",            "MISC.ToneMapping" },
            { "PREF_ZOOMEDMOVEMENT",         "MISC.ZoomedMovement" },
            { "PREF_UNCLAMPLIGHTING",        "MISC.UnclampLighting" },
            { "PREF_CENTEREDCAMERA",         "MISC.CenteredVehCam" },
            { "PREF_CENTEREDCAMERAFOOT",     "MISC.CenteredFootCam" },
            { "PREF_CAMERASHAKE",            "MAIN.CameraShake" },
            { "PREF_TURNINDICATORS",         "MISC.TurnIndicators" },
            { "PREF_EXTRANIGHTSHADOWS",      "SHADOWS.ExtraNightShadows" },
            { "PREF_GRAPHICSAPI",            "MAIN.GraphicsAPI" },
            { "PREF_BULLETTRACES",           "MISC.AlwaysShowBulletTraces" },
            { "PREF_AUTOEXPOSURE",           "MISC.ConsoleAutoExposure" },
            { "PREF_KBCAMCENTERDELAYVEH",    "MISC.DelayBeforeCenteringCameraKBInCar" },
            { "PREF_PADCAMCENTERDELAYVEH",   "MISC.DelayBeforeCenteringCameraPadInCar" },
            { "PREF_KBCAMTURNSPEEDVEH",      "MISC.CameraTurnSpeedKBInCar" },
            { "PREF_PADCAMTURNSPEEDVEH",     "MISC.CameraTurnSpeedPadInCar" },
            { "PREF_PADLOOKSENSITIVITY",     "MISC.PadLookSensitivity" },
            { "PREF_PADAIMSENSITIVITY",      "MISC.PadAimSensitivity" },
            { "PREF_MOUSEAIMSENSITIVITY",    "MISC.MouseAimSensitivity" },
            { "PREF_NOWARDROBEFADING",       "MISC.DisableWardrobeTransition" },
            { "PREF_STOPTAXI",               "MISC.InstantStopTaxi" },
            { "PREF_SAO",                    "MISC.AmbientOcclusion" },
            { "PREF_AUTOCLIMBLADDERS",       "MISC.AutoClimbLadders" },
            { "PREF_STUNTJUMPCAM",           "MISC.StuntJumpCamera" },
            { "PREF_ACTIONCAM",              "MISC.ActionCamera" },
            { "PREF_CONFINECURSOR",          "MAIN.ConfineCursor" },
        };

        for (auto& [name, id] : arr)
            RegisterPreference(name, id);

        // The graphics API is MAIN.GraphicsAPI, applied by the plugin at its init (graphicsapi.ixx,
        // source/upscaler/graphics_api.h; no d3d9.dll proxy, no d3d9.cfg). Vulkan needs DXVK as vulkan.dll next to the exe (the
        // build FusionFixHub installs or the player's own), DirectX 12 Fusion Fix's own D3D9On12 driver next to the exe (FusionFixHub
        // installs it), never Windows' slow one. When the API didn't start, the next start falls back to DirectX 9 and
        // sets this back (the crash guard of graphics_api.h).
        FusionFixSettings.SetAvailability("PREF_GRAPHICSAPI", [](int32_t value) -> bool
        {
            return FusionFixGraphicsApi::Available(static_cast<FusionFixGraphicsApi::Api>(std::clamp(value, 0, 2)));
        });
        InitializeMenuAPI();
        RegisterMenuRows();
        SnapshotRestartValues(ownPlugin);
    }

    // The options of Fusion Fix in the menus, the frontend XMLs are the original ones. Fusion Fix's settings are where
    // they have always been in the game's lists; settings Fusion Fix didn't have there before are on pages opened from
    // a row (HDR), other plugins on the Plugins page of Game. Every list fits the menu at Fusion Fix's font size
    // (PageRows). Rows name the settings of the description (settings/GTAIV.EFLC.FusionFix.json), which give their
    // texts and values.
    void RegisterMenuRows()
    {
        constexpr uint8_t NotTLAD = 1 | 4;

        // CE hides these rows every time the menu is filled, after the menu has shown them: they flicker. Marketplace
        // (TLAD, TBoGT) and voice output.
        if (!legacyExecutable)
        {
            RemoveOption(MenuScreen::Game, "MO_MARKETPLACE");
            for (auto screen : { MenuScreen::Audio, MenuScreen::TitleAudio })
                RemoveOption(screen, "MO_VOUT", "PREF_VOICE_OUTPUT");
        }

        // Game: the pause menu before Exit Game, the title menu at the end. The Plugins page follows them: one entry per
        // plugin, Fusion Fix first with its Preset row (its settings are on the game's tabs), then the other plugins
        // (FindOtherPlugins). A preset of all plugins is FusionFixHub's.
        for (auto [screen, before] : { std::pair{ MenuScreen::Game, "MO_EXITGAM" }, std::pair{ MenuScreen::TitleGame, "" } })
        {
            if (screen == MenuScreen::TitleGame)
                AddEmptyLine(screen, before);
            AddSetting(screen, "MAIN.SkipIntro", before);
            AddSetting(screen, "MAIN.SkipMenu", before);
            AddEmptyLine(screen, before);
            AddSetting(screen, "MISC.CutsceneBorders", before);
            AddSetting(screen, "MISC.TransparentMapMenu", before);
            AddSetting(screen, "FRAMELIMIT.DisplayFpsCounter", before);
            AddEmptyLine(screen, before);
            AddSetting(screen, "MAIN.WindowMode", before);
            AddSetting(screen, "MAIN.BlockOnLostFocus", before);
            // Shown on the game, with a warning in the help line while it's on
            AddSetting(screen, "SHADOWS.ExtraNightShadows", before);
            AddEmptyLine(screen, before);
            AddSetting(screen, "MISC.LightSyncRGB", before);
            AddSetting(screen, "MISC.TimedEvents", before);
            AddEmptyLine(screen, before);
            AddSetting(screen, "UPDATE.CheckForUpdates", before);
            AddEmptyLine(screen, before);
            pluginsBefore[static_cast<int32_t>(screen)] = before;
            if (auto own = AddCategory(PluginsPage(screen), MenuKey(ownPlugin, ownPlugin.file.description.name)); own != MenuScreen::Invalid)
                AddPresetRow(own, ownPlugin);
            // Exit Game apart from the Plugins row, like the groups above
            if (screen == MenuScreen::Game)
                AddEmptyLine(screen, before);
        }

        // Controls, at the end. TLAD has no wardrobe.
        for (auto screen : { MenuScreen::Controls, MenuScreen::TitleControls })
        {
            AddEmptyLine(screen);
            AddSetting(screen, "MISC.AlwaysRun");
            AddSetting(screen, "MISC.ZoomedMovement");
            AddSetting(screen, "MISC.ExtendedSniperControls");
            AddEmptyLine(screen);
            AddSetting(screen, "MAIN.CameraShake");
            AddSetting(screen, "MISC.CenteredVehCam");
            AddSetting(screen, "MISC.CenteredFootCam");
            AddSetting(screen, "MISC.StuntJumpCamera");
            AddSetting(screen, "MISC.ActionCamera");
            AddEmptyLine(screen);
            AddSetting(screen, "MISC.TurnIndicators");
            AddSetting(screen, "MISC.AlwaysShowBulletTraces");
            AddSetting(screen, "MISC.DisableWardrobeTransition", {}, NotTLAD);
            AddSetting(screen, "MISC.InstantStopTaxi");
            AddSetting(screen, "MISC.AutoClimbLadders");
        }

        // Keyboard options: 21 steps of mouse sensitivity, the camera settings before Remap Keyboard
        SetOptionScaler(MenuScreen::KeyboardOptions, "MO_MOUSE", 21);
        AddSetting(MenuScreen::KeyboardOptions, "MISC.MouseAimSensitivity", "MO_MOWHEEL");
        AddSetting(MenuScreen::KeyboardOptions, "MISC.DelayBeforeCenteringCameraKB", "MO_KBMAP");
        AddSetting(MenuScreen::KeyboardOptions, "MISC.DelayBeforeCenteringCameraKBInCar", "MO_KBMAP");
        AddSetting(MenuScreen::KeyboardOptions, "MISC.CameraTurnSpeedKBInCar", "MO_KBMAP");
        AddSetting(MenuScreen::KeyboardOptions, "MISC.RawInput", "MO_KBMAP");
        AddSetting(MenuScreen::KeyboardOptions, "MAIN.ConfineCursor", "MO_KBMAP");
        AddEmptyLine(MenuScreen::KeyboardOptions, "MO_KBMAP");

        // Controller options: look and aim sensitivity sliders instead of the three sensitivity steps
        for (auto screen : { MenuScreen::ControllerOptions, MenuScreen::NetworkControls })
        {
            constexpr auto sensitivity = "PREF_CONTROLLER_SENSITIVITY";
            AddSetting(screen, "MISC.PadLookSensitivity", sensitivity);
            AddSetting(screen, "MISC.PadAimSensitivity", sensitivity);
            if (screen == MenuScreen::ControllerOptions)
            {
                AddEmptyLine(screen, sensitivity);
                AddSetting(screen, "MISC.DelayBeforeCenteringCameraPad", sensitivity);
                AddSetting(screen, "MISC.DelayBeforeCenteringCameraPadInCar", sensitivity);
                AddSetting(screen, "MISC.CameraTurnSpeedPadInCar", sensitivity);
                AddSetting(screen, "MISC.Buttons", sensitivity);
            }
            RemoveOption(screen, "MO_SENS", sensitivity);
        }

        // Audio: no microphone, the options at the end
        for (auto screen : { MenuScreen::Audio, MenuScreen::TitleAudio })
        {
            RemoveOption(screen, "MO_VMIC");
            AddEmptyLine(screen);
            AddSetting(screen, "MISC.AltDialogue");
        }

        // Display: Field of View after Saturation, the image and lighting options before Restore Defaults, then the HDR
        // page, whose rows HDR adds (GetHdrScreens)
        for (auto screen : { MenuScreen::Display, MenuScreen::TitleDisplay })
        {
            AddSetting(screen, "MISC.FieldOfView", "MO_SUB");
            AddSetting(screen, "MAIN.Definition", "MO_DEF");
            AddSetting(screen, "MISC.ConsoleGamma", "MO_DEF");
            AddSetting(screen, "MISC.ConsoleAutoExposure", "MO_DEF");
            AddSetting(screen, "MAIN.MotionBlur", "MO_DEF");
            AddSetting(screen, "MISC.DepthOfField", "MO_DEF");
            AddSetting(screen, "MISC.TreeLighting", "MO_DEF");
            AddSetting(screen, "MISC.TreeAlpha", "MO_DEF");
            AddEmptyLine(screen, "MO_DEF");
            AddSetting(screen, "MAIN.Bloom", "MO_DEF");
            AddSetting(screen, "MISC.ScreenFilter", "MO_DEF");
            AddEmptyLine(screen, "MO_DEF");
            hdrScreens.push_back(AddPage(screen, "@ff.page.hdr", "MO_DEF", true, [] { return SettingValueText(ownPlugin, "HDR.HDR"); }));
            AddEmptyLine(screen, "MO_DEF");
        }

        // Graphics: after VSync, before Graphics Analyzer
        for (auto screen : { MenuScreen::Graphics, MenuScreen::TitleGraphics })
        {
            // Definition is in Display
            RemoveOption(screen, "MO_DOF", "PREF_DOF");
            AddSetting(screen, "FRAMELIMIT.FpsLimitPreset", "MO_ANALYZER");
            AddSetting(screen, "MISC.Antialiasing", "MO_ANALYZER");
            AddEmptyLine(screen, "MO_ANALYZER");
            AddSetting(screen, "FOG.VolumetricFog", "MO_ANALYZER");
            AddSetting(screen, "MISC.SunShafts", "MO_ANALYZER");
            AddSetting(screen, "MISC.UnclampLighting", "MO_ANALYZER");
            AddSetting(screen, "MISC.ToneMapping", "MO_ANALYZER");
            AddSetting(screen, "MISC.AmbientOcclusion", "MO_ANALYZER");
            AddSetting(screen, "SHADOWS.ShadowFilter", "MO_ANALYZER");
            AddSetting(screen, "MAIN.GraphicsAPI", "MO_ANALYZER");
        }
    }


public:
    // A setting of Fusion Fix or another plugin applied after a restart was changed since the game was started: the
    // line below the menu says the game must be restarted (FF_RESTART), as it always did for the graphics API
    static bool RestartPending() { return PendingRestarts() > 0; }
    // A text of Fusion Fix's description (settings/*.json) in the game's language, e.g. "@ff.update.hub"
    static std::string Text(std::string_view key) { return ownPlugin.file.texts.Resolve(key); }
    // A text the game shows for a key of its own (help text, menu rows), e.g. for PRINT_HELP
    static void SetGameText(const char* key, std::wstring text) { CText::menuTexts[GetHash(key)] = std::move(text); }
    static void SetGameText(const char* key, std::string_view text) { SetGameText(key, ToGameText(text)); }
    // A restart setting the plugin changed itself while starting (the graphics API's crash guard set it back): its
    // value now is the one the game started with, no restart is pending for it
    static void RestartBaseline(std::string_view id)
    {
        auto key = ownPlugin.file.id + "|" + std::string(id);
        if (restartValues.contains(key))
            restartValues[key] = ownPlugin.Get(std::string(id));
    }

    // A custom screen is shown in place of the Display screen
    static bool IsCustomScreenActive() { return activePage != -1; }
    // The tab the shown custom screen belongs to, -1 for none
    static int32_t CustomScreenRoot() { return activePage != -1 ? GetRootScreen(activePage) : -1; }
    // Before the game frees the screens' rows (frontend XML reload), and after the menu is closed
    static void ResetCustomScreens() { ResetPages(); }
    // Tab change, the tab state is in EBX (EBP in 1.1.2.0): a custom tab is shown on the Display screen
    static void OnTabTransition(uintptr_t ebx, uintptr_t ebp)
    {
        if (!tabsInitialized)
            return;
        auto state = legacyExecutable ? ebp : ebx;
        pendingPage = FindPage(requestedTab) ? requestedTab : -1;
        if (requestedTab != -1)
            *reinterpret_cast<int32_t*>(state + 0x18) = pendingPage != -1 ? DisplayScreen : requestedTab;
    }

    // Screens from AddScreen/AddSubmenu get ids from 256 on
    enum class MenuScreen : int32_t
    {
        Invalid = -1,
        Game = 0, NetworkGame = 1, Brief = 2, Map = 3, Stats = 4, Controls = 5, NetworkControls = 6, Audio = 7, Display = 8, Graphics = 49,
        TitleControls = 59, TitleAudio = 60, TitleDisplay = 61, TitleGraphics = 62, TitleGame = 63,
        KeyboardOptions = 71, ControllerOptions = 72
    };

    struct EnumValue
    {
        int32_t value;
        std::wstring_view text;     // 1..59 characters
    };

    // A new tab of the pause menu after a settings tab or another custom tab, up to 4 (CE only). displayGame
    // keeps the game visible behind it, like the Display tab. The label is UTF-8, up to 15 bytes.
    MenuScreen AddScreen(std::string_view label, MenuScreen after, bool displayGame = false)
    {
        if (!tabsInitialized || label.empty() || label.size() >= sizeof(SettingsTables::Screen::header) || label.find('\0') != label.npos)
            return MenuScreen::Invalid;
        for (size_t i = 0; i < pageCount; ++i)
            if (pages[i].parent == -1 && label == pages[i].label)
                return static_cast<MenuScreen>(FirstCustomScreen + i);
        if (pageCount == MaxPages || customTabCount == 4)
            return MenuScreen::Invalid;
        auto afterId = static_cast<int32_t>(after);
        if (after != MenuScreen::Controls && after != MenuScreen::Audio && after != MenuScreen::Display && after != MenuScreen::Graphics && !FindPage(afterId))
            return MenuScreen::Invalid;
        auto position = std::find_if(tabs.begin(), tabs.end(), [afterId](const Tab& tab) { return tab.id == afterId; });
        auto text = ToWide(label, 16);
        if (position == tabs.end() || text.empty())
            return MenuScreen::Invalid;

        auto screen = FirstCustomScreen + static_cast<int32_t>(pageCount);
        auto& page = pages.emplace_back();
        ++pageCount;
        std::memcpy(page.label, label.data(), label.size());
        page.text = std::move(text);
        page.rows[0].action = 46;
        page.displayGame = displayGame;
        ++customTabCount;
        tabs.insert(position + 1, Tab{ screen });
        return static_cast<MenuScreen>(screen);
    }

    // A page opened from a row of a settings screen of the pause menu or the title menu, or of a custom screen;
    // Back returns to it. The label is a key of FF's texts, or UTF-8 text up to 59 characters. displayGame keeps
    // the game visible behind a page of the pause menu. The row is inserted before the row labelled 'before', else
    // added at the end.
    static MenuScreen AddSubmenu(MenuScreen parent, std::string_view label, bool displayGame = false, std::string_view before = {})
    {
        auto parentId = static_cast<int32_t>(parent);
        if (!tabsInitialized || label.empty() || label.size() >= sizeof(Page::label) || label.find('\0') != label.npos ||
            (!FindPage(parentId) && parent != MenuScreen::Game && parent != MenuScreen::Controls && parent != MenuScreen::Audio && parent != MenuScreen::Display &&
                parent != MenuScreen::Graphics && !IsTitleScreen(parentId)))
            return MenuScreen::Invalid;
        for (size_t i = 0; i < pageCount; ++i)
            if (pages[i].parent == parentId && !pages[i].category && label == pages[i].label)
                return static_cast<MenuScreen>(FirstCustomScreen + i);
        auto text = ToWide(label, 60);
        if (text.empty() || pageCount == MaxPages)
            return MenuScreen::Invalid;

        auto screen = FirstCustomScreen + static_cast<int32_t>(pageCount);
        char key[16]{};
        std::snprintf(key, sizeof(key), "NY_SUB_%03u", static_cast<uint32_t>(pageCount));
        if (!AddOption(parent, key, static_cast<int32_t>(dynamicOptions.size()), 100, {}, screen, before))
            return MenuScreen::Invalid;

        auto& page = pages.emplace_back();
        ++pageCount;
        std::memcpy(page.label, label.data(), label.size());
        page.parent = parentId;
        // The title menu shows no game
        page.displayGame = displayGame && !IsTitleScreen(GetRootScreen(parentId));
        page.rows[0].action = 46;
        CText::menuTexts[GetHash(key)] = text;
        page.text = std::move(text);
        return static_cast<MenuScreen>(screen);
    }

    // Options whose value lives in the caller. The menu reads it through get and changes it through set.
    bool AddToggle(MenuScreen screen, std::string_view label, std::function<bool()> get, std::function<void(bool)> set)
    {
        if (!get || !set || !CanAddOption(screen, label))
            return false;
        auto id = RegisterCallbackPreference(1, [get] { return get() ? 1 : 0; }, [set](int32_t value) { set(value != 0); });
        return id >= 0 && AddOption(screen, label, id, 0);
    }

    // Values outside the list show as "Custom"
    bool AddEnum(MenuScreen screen, std::string_view label, std::span<const EnumValue> values, std::function<int32_t()> get, std::function<void(int32_t)> set)
    {
        if (values.size() < 2 || values.size() > 254 || !get || !set || !CanAddOption(screen, label))
            return false;
        for (size_t i = 0; i < values.size(); ++i)
        {
            if (values[i].text.empty() || values[i].text.size() >= 60 || values[i].text.find(L'\0') != values[i].text.npos)
                return false;
            for (size_t j = 0; j < i; ++j)
                if (values[j].value == values[i].value)
                    return false;
        }

        // The display shows GXT keys, which resolve to the value texts
        auto index = callbackCount;
        std::vector<std::string> keys;
        for (size_t i = 0; i <= values.size(); ++i)
            keys.push_back(std::format("FFM{:03}_{:03}", index, i));
        std::vector<std::string_view> labels(keys.begin(), keys.end());
        auto display = RegisterEnum(std::format("MENU_DISPLAY_FFM{:03}", index), labels);
        if (display < 0)
            return false;
        for (size_t i = 0; i < values.size(); ++i)
            CText::menuTexts[GetHash(keys[i].c_str())] = values[i].text;
        CText::menuTexts[GetHash(keys.back().c_str())] = L"Custom";

        std::vector<int32_t> choices;
        for (auto& value : values)
            choices.push_back(value.value);
        auto count = static_cast<int32_t>(choices.size());
        auto id = RegisterCallbackPreference(count - 1, [get, choices, count]
        {
            auto found = std::find(choices.begin(), choices.end(), get());
            return found != choices.end() ? static_cast<int32_t>(found - choices.begin()) : count;
        }, [set, choices](int32_t position) { set(choices[position]); });
        return id >= 0 && AddOption(screen, label, id, display);
    }

    bool AddEnum(MenuScreen screen, std::string_view label, std::initializer_list<EnumValue> values, std::function<int32_t()> get, std::function<void(int32_t)> set)
    {
        return AddEnum(screen, label, std::span<const EnumValue>(values.begin(), values.size()), std::move(get), std::move(set));
    }

    // From minimum to maximum in steps, 2..255 positions. The value is shown next to the slider.
    bool AddSlider(MenuScreen screen, std::string_view label, float minimum, float maximum, float step, std::function<float()> get, std::function<void(float)> set)
    {
        if (!std::isfinite(minimum) || !std::isfinite(maximum) || !std::isfinite(step) || minimum >= maximum || step <= 0.0f || !get || !set ||
            !CanAddOption(screen, label))
            return false;
        auto steps = std::ceil((static_cast<double>(maximum) - minimum) / step);
        if (steps > 1.0 && static_cast<float>(minimum + (steps - 1.0) * step) >= maximum)
            steps -= 1.0;
        if (steps < 1.0 || steps > 254.0)
            return false;
        auto count = static_cast<int32_t>(steps);
        auto valueAt = [minimum, maximum, step, count](int32_t position)
        {
            return position >= count ? maximum : static_cast<float>(minimum + static_cast<double>(position) * step);
        };
        for (int32_t i = 1; i <= count; ++i)
            if (valueAt(i) <= valueAt(i - 1))
                return false;   // the step is below the precision of the values

        auto id = RegisterCallbackPreference(count, [get, minimum, maximum, step, count]
        {
            auto value = get();
            if (!std::isfinite(value) || value <= minimum)
                return 0;
            if (value >= maximum)
                return count;
            auto position = (static_cast<double>(value) - minimum) / step;
            auto lower = std::min(static_cast<int32_t>(position), count - 1);
            auto lowerValue = static_cast<double>(minimum) + lower * static_cast<double>(step);
            auto upperValue = std::min(static_cast<double>(maximum), lowerValue + step);
            return lower + (value - lowerValue >= upperValue - value ? 1 : 0);
        }, [get, set, valueAt](int32_t position)
        {
            auto value = valueAt(position);
            if (get() != value)
                set(value);
        });
        if (id < 0)
            return false;
        // MENU_DISPLAY_VALUE_SLIDERBAR where the game's number can be replaced, else MENU_DISPLAY_SLIDERBAR
        if (valueTextHook)
            valueTexts[id] = [get] { return std::format(L"{}", get()); };
        return AddOption(screen, label, id, valueTextHook ? 108 : 101);
    }

    // The HDR page of Display, in the pause menu and in the title menu. Options for both menus are added to each.
    const std::vector<MenuScreen>& GetHdrScreens() const
    {
        return hdrScreens;
    }

    // Render thread: whether the menu showed the HDR page in the frame drawn since the last call, and the setting of
    // its selected row (empty for a row that changes no setting)
    static std::optional<std::string> TakeHdrPageRow()
    {
        return std::exchange(hdrPageRow, std::nullopt);
    }

    // A line below the description of a setting's row in the menu's info text, e.g. how to set it; empty for none.
    // Called on the render thread while the row is selected.
    static void SetRowHint(std::string settingId, std::function<std::string()> hint)
    {
        rowHints[std::move(settingId)] = std::move(hint);
    }

    // A text of the game's (GXT), the fallback when the game's texts don't have the key
    static std::string GameText(const char* key, std::string_view fallback)
    {
        return CText::g_text && GameHasText(key) ? FromGameText(CText::getText(key)) : std::string(fallback);
    }

    // Call registration APIs on the game thread, or during startup before the
    // frontend is loaded. Values are zero-based positions, also for sliders.
    // An in-memory preference. Callback runs on Set().
    static int32_t RegisterPreference(std::string name, int32_t maximum, int32_t defaultValue = 0, std::function<void(int32_t)> callback = {})
    {
        if (!name.starts_with("PREF_") || name.size() == 5 || maximum < 1 || maximum > 254 ||
            GetPrefIDByName(name) || aMenuPrefs.size() >= SettingsTables::PreferenceCapacity)
            return -1;
        auto id = static_cast<int32_t>(aMenuPrefs.size());
        CSetting setting;
        setting.value = std::clamp(defaultValue, 0, maximum);
        setting.prefName = std::move(name);
        setting.callback = std::move(callback);
        setting.idEnd = maximum;
        auto& stored = mFusionPrefs.emplace(id, std::move(setting)).first->second;
        aMenuPrefs.emplace_back(id, stored.prefName.data());
        SettingsTables::scalers[id] = maximum + 1;
        return id;
    }

    // A preference stored by a setting of the description (SECTION.Key): a bool, a choice of the values 0, 1, 2...,
    // or an int from 0 on. Its value is the setting's value in effect; the callback runs on the game's thread after
    // it changed, whoever changed it (the menu, a preset, an editor, availability).
    static int32_t RegisterPreference(std::string name, std::string_view settingId, std::function<void(int32_t)> callback = {})
    {
        auto setting = SettingsPlugin().GetDescription().Find(settingId);
        if (!setting || !name.starts_with("PREF_") || name.size() == 5 || GetPrefIDByName(name) ||
            aMenuPrefs.size() >= SettingsTables::PreferenceCapacity)
            return -1;
        int32_t maximum = 1;
        switch (setting->type)
        {
        case asi_settings::Type::Bool:
            break;
        case asi_settings::Type::Choice:
            maximum = static_cast<int32_t>(setting->choices.size()) - 1;
            for (int32_t i = 0; i <= maximum; ++i)
                if (!setting->choices[i].value.IsNumber() || setting->choices[i].value.number != i)
                    return -1;
            break;
        case asi_settings::Type::Int:
            if (setting->min.value_or(-1.0) != 0.0 || !setting->max || setting->step.value_or(1.0) != 1.0)
                return -1;
            maximum = static_cast<int32_t>(*setting->max);
            break;
        default:
            return -1;
        }
        if (maximum < 1 || maximum > 254)
            return -1;

        auto id = static_cast<int32_t>(aMenuPrefs.size());
        CSetting stored;
        stored.prefName = std::move(name);
        stored.id = setting->id;
        stored.value = SettingsPlugin().Int(setting->id);
        stored.callback = std::move(callback);
        stored.idEnd = maximum;
        // Values that can't be used now are skipped in the menu (SetAvailability)
        stored.available = [settingId = setting->id](int32_t value) { return SettingsPlugin().Usable(settingId, std::to_string(value)); };
        auto& added = mFusionPrefs.emplace(id, std::move(stored)).first->second;
        aMenuPrefs.emplace_back(id, added.prefName.data());
        SettingsTables::scalers[id] = maximum + 1;

        // The values' texts: keys of the game's texts, else the description's text
        if (setting->type == asi_settings::Type::Choice)
        {
            std::vector<std::string> keys;
            for (auto& choice : setting->choices)
                keys.push_back(MenuKey(ownPlugin, choice.label.empty() ? asi_settings::detail::JsonToText(choice.value) : choice.label));
            DefineDisplay(ChoiceDisplay(added.prefName), keys);
        }
        SettingsPlugin().OnChange(setting->id, [id] { OnPluginChange(id); });
        return id;
    }
    // Text entries are GXT keys or literal labels, using the native 15-byte
    // field. Register before XML loading when the name will be used in XML.
    static int32_t RegisterEnum(std::string name, std::span<const std::string_view> labels)
    {
        if (!name.starts_with("MENU_DISPLAY_") || name.size() == 13 || displayIDs.contains(name) ||
            nextDisplayID >= SettingsTables::DisplayCapacity || labels.size() < 2 || labels.size() > 255)
            return -1;
        std::vector<SettingsTables::DisplayValue> values;
        for (auto label : labels)
        {
            if (label.empty() || label.size() >= sizeof(SettingsTables::DisplayValue::text) || label.find('\0') != label.npos)
                return -1;
            SettingsTables::DisplayValue value;
            std::memcpy(value.text, label.data(), label.size());
            value.value = static_cast<int32_t>(values.size());
            values.push_back(value);
        }
        auto id = nextDisplayID++;
        displayIDs.emplace(std::move(name), id);
        dynamicDisplays.emplace(id, std::move(values));
        return id;
    }

    bool AddToggle(MenuScreen screen, std::string_view label, std::string_view preference)
    {
        auto id = GetPrefIDByName(preference);
        if (!id || !mFusionPrefs.contains(*id) || mFusionPrefs.at(*id).idEnd != 1)
            return false;
        return AddOption(screen, label, *id, 0);
    }

    bool AddEnum(MenuScreen screen, std::string_view label, std::string_view preference, std::string_view display)
    {
        auto id = GetPrefIDByName(preference);
        auto found = displayIDs.find(std::string(display));
        if (!id || !mFusionPrefs.contains(*id) || found == displayIDs.end())
            return false;
        auto count = dynamicDisplays.contains(found->second) ? dynamicDisplays.at(found->second).size() : SettingsTables::displays[found->second].values.count;
        if (count && count != mFusionPrefs.at(*id).idEnd + 1)
            return false;
        return AddOption(screen, label, *id, found->second);
    }

    bool AddSlider(MenuScreen screen, std::string_view label, std::string_view preference)
    {
        auto id = GetPrefIDByName(preference);
        return id && mFusionPrefs.contains(*id) && AddOption(screen, label, *id, 101);
    }

    bool AddButton(MenuScreen screen, std::string_view label, std::function<void()> callback)
    {
        if (!callback || dynamicOptions.size() >= SettingsTables::PreferenceCapacity)
            return false;
        return AddOption(screen, label, static_cast<int32_t>(dynamicOptions.size()), 100, std::move(callback));
    }

    // An empty line between groups of options. Inserted before the row labelled 'before' (or with that preference,
    // "PREF_..."), else added at the end. Episodes: 1 IV, 2 TLAD, 4 TBoGT.
    static bool AddEmptyLine(MenuScreen screen, std::string_view before = {}, uint8_t episodes = 0xFF)
    {
        auto label = std::format("FFGAP{:03}", gapCount++);
        CText::menuTexts[GetHash(label.c_str())] = L"";
        return AddOption(screen, label, 0, 100, {}, -1, before, 0, episodes, true);
    }

    // Removes a row of the frontend XML: the row with the label, and with the preference when one is given
    bool RemoveOption(MenuScreen screen, std::string_view label, std::string_view preference = {})
    {
        auto id = preference.empty() ? std::optional<int32_t>(-1) : GetPrefIDByName(preference);
        if (!id || label.empty())
            return false;
        rowEdits.push_back({ static_cast<int32_t>(screen), std::string(label), *id, true, -1 });
        return true;
    }

    // Changes the scaler of a row of the frontend XML: the number of values of its preference
    bool SetOptionScaler(MenuScreen screen, std::string_view label, int32_t scaler)
    {
        if (label.empty() || scaler < 1 || scaler > 255)
            return false;
        rowEdits.push_back({ static_cast<int32_t>(screen), std::string(label), -1, false, scaler });
        return true;
    }

    // A category of options of Display or Graphics in the pause menu, or of a page: a column of categories left of the
    // options (CE). The screen's own options are the first category of Display and Graphics, a page shows only its
    // categories. Elsewhere the category is a submenu opened from a row before the row labelled 'before'. The name is
    // a key of FF's texts or the text itself. Options of the category are added to the returned screen.
    static MenuScreen AddCategory(MenuScreen screen, std::string_view name, std::string_view before = {})
    {
        auto id = static_cast<int32_t>(screen);
        auto parentPage = FindPage(id);
        if (!categoriesEnabled || (screen != MenuScreen::Display && screen != MenuScreen::Graphics && (!parentPage || parentPage->category)))
            return AddSubmenu(screen, name, !parentPage || parentPage->displayGame, before);
        if (name.empty() || name.size() >= sizeof(Page::label) || name.find('\0') != name.npos)
            return MenuScreen::Invalid;
        for (size_t i = 0; i < pageCount; ++i)
            if (pages[i].category && pages[i].parent == id && name == pages[i].label)
                return static_cast<MenuScreen>(FirstCustomScreen + i);
        if (pageCount == MaxPages)
            return MenuScreen::Invalid;
        auto text = ToWide(name, 60);
        if (text.empty())
            return MenuScreen::Invalid;
        auto categories = FindCategories(id);
        if (!categories)
        {
            categories = &categoryScreens.emplace_back();
            categories->screen = id;
            categories->ownRows = parentPage == nullptr;
            if (categories->ownRows)
                categories->selectedRows.push_back(-1);
        }
        if (static_cast<size_t>(categories->Count()) >= categoryBounds.size())
            return MenuScreen::Invalid;

        auto page = FirstCustomScreen + static_cast<int32_t>(pageCount);
        auto& entry = pages.emplace_back();
        ++pageCount;
        std::memcpy(entry.label, name.data(), name.size());
        entry.text = std::move(text);
        entry.parent = id;
        entry.category = true;
        entry.displayGame = parentPage ? parentPage->displayGame : screen == MenuScreen::Display;
        entry.rows[0].action = 46;
        categories->pages.push_back(page);
        categories->selectedRows.push_back(-1);
        return static_cast<MenuScreen>(page);
    }

    // The preference of the selected row of the current menu screen, -1 for none
    static int32_t GetSelectedPreference()
    {
        if (!currentScreen || *currentScreen < 0 || *currentScreen >= 73)
            return -1;
        auto& options = screens[*currentScreen].options;
        auto row = CMenu::getSelectedItem();
        if (!options.data || row < 0 || row >= options.count)
            return -1;
        return options.data[row].preference;
    }

    // A row of a setting of Fusion Fix's description (SECTION.Key), which gives its text and values
    static bool AddSettingRow(MenuScreen screen, std::string_view id, std::string_view before = {}, uint8_t episodes = 0xFF)
    {
        return AddSetting(screen, id, before, episodes);
    }

    // -1 for an unknown name
    static int32_t GetPreferenceID(std::string_view name)
    {
        return GetPrefIDByName(name).value_or(-1);
    }

private:
    static inline int32_t gapCount = 0;

    // ---------------------------------------------------------------------------------------------
    // Rows of the settings of descriptions (ASI Settings): Fusion Fix's own and other plugins'

    // A text of a description in the menu, resolved in the game's language
    struct MenuTextSource
    {
        std::string key;
        std::function<std::string()> text;  // UTF-8
        bool gameText = false;              // a key of the game's texts (Fusion Fix's GXT), which win when they have it
    };
    static inline std::vector<MenuTextSource> menuTextSources;
    static inline std::string menuTextLanguage;
    static inline bool menuTextsChanged = true;

    // The BCP 47 tags of the game's languages (GameLanguage)
    static const char* GameLanguageTag()
    {
        static constexpr const char* tags[] = { "en", "fr", "de", "it", "es", "ru", "ja" };
        static auto language = GetPrefIDByName("PREF_CURRENT_LANGUAGE");
        auto value = language && mPrefs ? mPrefs[*language] : 0;
        return tags[value >= 0 && value < static_cast<int32_t>(std::size(tags)) ? value : 0];
    }

    static std::string AddMenuText(std::string key, std::function<std::string()> text, bool gameText = false)
    {
        if (std::none_of(menuTextSources.begin(), menuTextSources.end(), [&](auto& source) { return source.key == key; }))
            menuTextSources.push_back({ key, std::move(text), gameText });
        menuTextsChanged = true;
        return key;
    }

    // The key a row or a value shows for a text of a description. A text key of Fusion Fix's description that fits
    // the 15 bytes of a label is used as is, so a key of the game's own texts (MO_HIGH) shows the game's text; other
    // texts get a key of their own.
    static std::string MenuKey(const MenuPlugin& plugin, std::string_view text)
    {
        auto resolve = [&plugin, text = std::string(text)] { return plugin.file.texts.Resolve(text); };
        if (&plugin == &ownPlugin && text.size() > 1 && text.size() <= sizeof(SettingsTables::Option::label) && text[0] == '@' && text[1] != '@')
            return AddMenuText(std::string(text.substr(1)), resolve, true);
        return AddMenuText(std::format("FFT{:08X}", GetHash((plugin.file.id + "|" + std::string(text)).c_str())), resolve);
    }

    static bool GameHasText(const std::string& key)
    {
        if (gxtEntries.contains(GetHash(key.c_str())))
            return true;
        return VanillaHasText(key);
    }

    // Whether the game's own texts have the key, Fusion Fix's GXT aside
    static bool VanillaHasText(const std::string& key)
    {
        return CText::g_text && CText::shDoesTextLabelExist && CText::shDoesTextLabelExist.fastcall<char>(CText::g_text, nullptr, key.c_str()) != 0;
    }

    // The texts in the game's language, again after it changed or texts were added
    static void ResolveMenuTexts(bool force)
    {
        std::string language = GameLanguageTag();
        if (!force && !menuTextsChanged && language == menuTextLanguage)
            return;
        if (language != menuTextLanguage)
        {
            ownPlugin.file.texts.Select(language);
            for (auto& plugin : otherPlugins)
                plugin.file.texts.Select(language);
            menuTextLanguage = language;
        }
        menuTextsChanged = false;
        for (auto& source : menuTextSources)
        {
            auto hash = GetHash(source.key.c_str());
            if (source.gameText && VanillaHasText(source.key))
                CText::menuTexts.erase(hash);
            else
                CText::menuTexts[hash] = ToGameText(source.text());
        }
    }

    static std::string ChoiceDisplay(std::string_view preference)
    {
        return "MENU_DISPLAY_FF_" + std::string(preference.substr(5));
    }

    // Values as text: the same text, or the same number
    static bool SameValue(const std::string& a, const std::string& b)
    {
        float x, y;
        return asi_settings::text::IEquals(a, b) || (asi_settings::detail::ParseFloat(a, x) && asi_settings::detail::ParseFloat(b, y) && x == y);
    }

    // A row of a setting of Fusion Fix's description: like the <options> of the frontend XML with its preference
    // where it has one, else a row of its own
    static bool AddSetting(MenuScreen screen, std::string_view id, std::string_view before = {}, uint8_t episodes = 0xFF)
    {
        auto setting = ownPlugin.file.description.Find(id);
        if (!setting)
            return false;
        auto found = std::find_if(mFusionPrefs.begin(), mFusionPrefs.end(), [&](auto& pref) { return asi_settings::text::IEquals(pref.second.id, id); });
        if (found == mFusionPrefs.end())
        {
            std::function<bool()> visible;
            if (setting->visibleIf)
                visible = [setting] { return ownPlugin.Evaluate(*setting->visibleIf); };
            return AddDescribedRow({ &screen, 1 }, ownPlugin, *setting, visible, before, episodes);
        }
        auto& stored = found->second;
        auto display = setting->type == asi_settings::Type::Bool ? FindDisplay("MENU_DISPLAY_ON_OFF") :
            setting->type == asi_settings::Type::Choice ? FindDisplay(ChoiceDisplay(stored.prefName)) : FindDisplay("MENU_DISPLAY_SLIDERBAR");
        rowSettings[found->first] = { &ownPlugin, setting, {} };
        return display >= 0 && AddOption(screen, LabelKey(ownPlugin, *setting), static_cast<int32_t>(found->first), display, {}, -1, before,
            stored.idEnd + 1, episodes);
    }

    // The preference and display of a setting's rows, shared by the pause menu and the title menu
    struct DescribedRow
    {
        int32_t preference = -1;
        int32_t display = -1;
        int32_t scaler = 0;
    };
    static inline std::map<std::string, DescribedRow> describedRows;
    static inline int32_t describedDisplays = 0;

    // A row of any setting of a description, read and changed as text through its plugin: a toggle for bool, a list
    // for choices (and for numbers or text with special values or suggestions), a slider for numbers with a range.
    // The menu offers only usable values and refuses changes while enabledIf is false. Settings it can't show remain
    // in editors and the file.
    static bool AddDescribedRow(std::span<const MenuScreen> screens, const MenuPlugin& plugin, const asi_settings::Setting& setting,
        std::function<bool()> visible, std::string_view before = {}, uint8_t episodes = 0xFF)
    {
        using asi_settings::Type;
        if (!setting.HasValue() || setting.visibility != "menu")
            return false;
        auto p = &plugin;
        auto s = &setting;
        auto enabled = [p, s] { return !s->enabledIf || p->Evaluate(*s->enabledIf); };

        auto cacheKey = plugin.file.id + "|" + setting.id;
        auto cached = describedRows.find(cacheKey);
        if (cached == describedRows.end())
        {
            DescribedRow row;
            bool range = setting.min && setting.max && *setting.min < *setting.max && (setting.type == Type::Int || setting.type == Type::Float);
            bool listPreferred = !setting.special.empty() || !setting.suggestions.empty();
            for (auto& control : setting.controls)
            {
                if (control == "slider" || control == "stepper")
                    break;
                if (control == "cycler" || control == "dropdown" || control == "segmented")
                {
                    range = range && !listPreferred;
                    break;
                }
            }
            if (setting.type == Type::Bool)
            {
                row.preference = RegisterCallbackPreference(1, [p, s]
                {
                    int value = 0;
                    asi_settings::detail::ParseBool(p->Get(s->id), value);
                    return value;
                }, [p, s, enabled](int32_t value)
                {
                    if (enabled())
                        p->Set(s->id, value ? "1" : "0");
                });
                row.display = FindDisplay("MENU_DISPLAY_ON_OFF");
                row.scaler = 2;
                if (row.preference >= 0)
                    mFusionPrefs.at(row.preference).available = [p, s](int32_t value) { return MenuPlugin::IsUsable(p->Usable(*s), std::to_string(value)); };
            }
            else if (range)
            {
                double minimum = *setting.min, maximum = *setting.max;
                double step = setting.step ? *setting.step : setting.type == Type::Int ? 1.0 : (maximum - minimum) / 20.0;
                auto steps = std::ceil((maximum - minimum) / step - 0.000001);
                if (steps >= 1.0 && steps <= 253.0)
                {
                    auto count = static_cast<int32_t>(steps);
                    auto valueAt = [minimum, maximum, step, count](int32_t position)
                    {
                        return position >= count ? maximum : minimum + position * step;
                    };
                    auto integer = setting.type == Type::Int;
                    row.preference = RegisterCallbackPreference(count, [p, s, minimum, step, count]
                    {
                        float value = 0.0f;
                        if (!asi_settings::detail::ParseFloat(p->Get(s->id), value))
                            return 0;
                        return std::clamp(static_cast<int32_t>(std::lround((value - minimum) / step)), 0, count);
                    }, [p, s, enabled, valueAt, integer](int32_t position)
                    {
                        auto value = valueAt(position);
                        if (enabled())
                            p->Set(s->id, integer ? std::to_string(std::llround(value)) : asi_settings::text::FormatNumber(value));
                    });
                    row.display = valueTextHook ? 108 : 101;
                    row.scaler = count + 1;
                    auto decimals = setting.decimals >= 0 ? setting.decimals : integer ? 0 :
                        std::clamp(static_cast<int32_t>(std::ceil(-std::log10(step) - 0.000001)), 0, 6);
                    if (row.preference >= 0)
                        valueTexts[row.preference] = [p, s, decimals]
                        {
                            float value = 0.0f;
                            asi_settings::detail::ParseFloat(p->Get(s->id), value);
                            auto text = std::format("{:.{}f}", value, decimals) + (s->unit.empty() ? "" : (s->unit == "%" || s->unit == "\xC2\xB0" ? "" : " ") + s->unit);
                            return ToGameText(text);
                        };
                }
            }
            else
            {
                // The values and the text fields of their labels
                std::vector<std::pair<std::string, std::string>> list;
                auto addList = [&](const std::vector<asi_settings::Labelled>& items)
                {
                    for (auto& item : items)
                    {
                        auto value = asi_settings::detail::JsonToText(item.value);
                        list.emplace_back(value, item.label.empty() ? value : item.label);
                    }
                };
                if (setting.type == Type::Choice)
                    addList(setting.choices);
                else
                {
                    addList(setting.special);
                    addList(setting.suggestions);
                }
                if (list.size() >= 2 && list.size() <= 254 && nextDisplayID < static_cast<int32_t>(SettingsTables::DisplayCapacity))
                {
                    // A value outside the list shows as Custom
                    std::vector<std::string> keys;
                    for (auto& [value, label] : list)
                        keys.push_back(MenuKey(plugin, label));
                    keys.push_back(MenuKey(ownPlugin, "@ff.custom"));
                    std::vector<std::string_view> labels(keys.begin(), keys.end());
                    row.display = RegisterEnum(std::format("MENU_DISPLAY_FFH{:03}", describedDisplays++), labels);
                    std::vector<std::string> values;
                    for (auto& [value, label] : list)
                        values.push_back(value);
                    auto count = static_cast<int32_t>(values.size());
                    if (row.display >= 0)
                        row.preference = RegisterCallbackPreference(count - 1, [p, s, values, count]
                        {
                            auto current = p->Get(s->id);
                            for (int32_t i = 0; i < count; ++i)
                                if (SameValue(values[i], current))
                                    return i;
                            return count;
                        }, [p, s, enabled, values](int32_t position)
                        {
                            if (enabled() && position >= 0 && position < static_cast<int32_t>(values.size()))
                                p->Set(s->id, values[position]);
                        });
                    row.scaler = count;
                    if (row.preference >= 0)
                        mFusionPrefs.at(row.preference).available = [p, s, values](int32_t position)
                        {
                            return position < 0 || position >= static_cast<int32_t>(values.size()) || MenuPlugin::IsUsable(p->Usable(*s), values[position]);
                        };
                }
            }
            cached = describedRows.emplace(cacheKey, row).first;
        }
        auto& row = cached->second;
        if (row.preference < 0 || row.display < 0)
            return false;
        rowSettings[row.preference] = { &plugin, &setting, {} };
        auto label = LabelKey(plugin, setting);
        bool added = false;
        for (auto screen : screens)
            added |= AddOption(screen, label, row.preference, row.display, {}, -1, before, row.scaler, episodes, false, visible);
        return added;
    }

    // ---------------------------------------------------------------------------------------------
    // Pages, headings and the texts around rows

    // Rows the list of the menu shows at Fusion Fix's font size (update/common/data/frontend_pc.dat; 27, measured at
    // 16:10), the end of the list included: pages are laid out to fit them, the list doesn't scroll
    static constexpr int32_t PageRows = 28;

    // The setting a row changes: its help line, its default, whether it can be changed now
    struct RowSetting
    {
        const MenuPlugin* plugin = nullptr;
        const asi_settings::Setting* setting = nullptr;
        std::string description;            // a text of the description that replaces the setting's own
        std::function<void()> reset;        // rows of several settings
    };
    static inline std::unordered_map<int32_t, RowSetting> rowSettings;     // by preference
    static inline std::vector<MenuScreen> hdrScreens;
    static inline std::optional<std::string> hdrPageRow;                    // TakeHdrPageRow
    static inline std::unordered_map<std::string, std::function<std::string()>> rowHints;  // by setting
    static inline std::unordered_map<int32_t, std::string> pluginsBefore;  // the row the Plugins row goes before, by screen
    static inline std::unordered_map<int32_t, MenuScreen> pluginsPages;     // by screen
    static inline std::unordered_map<std::string, std::string> restartValues; // values of restart settings at start

    static std::string Narrow(std::wstring_view text)
    {
        if (text.empty())
            return {};
        auto size = WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int32_t>(text.size()), nullptr, 0, nullptr, nullptr);
        std::string out(size > 0 ? size : 0, '\0');
        if (size > 0)
            WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int32_t>(text.size()), out.data(), size, nullptr, nullptr);
        return out;
    }

    // UTF-8 text as the game draws it, in the font of the game's language
    static std::wstring ToGameText(std::string_view text)
    {
        return GameFont::ToGame(asi_settings::text::Widen(text), GameLanguageTag());
    }

    // A text of the game's as UTF-8
    static std::string FromGameText(const wchar_t* text)
    {
        return text ? Narrow(GameFont::ToUnicode(text, GameLanguageTag())) : std::string();
    }

    static std::string Upper(const std::string& text)
    {
        auto wide = asi_settings::text::Widen(text);
        if (!wide.empty())
            CharUpperBuffW(wide.data(), static_cast<DWORD>(wide.size()));
        return Narrow(wide);
    }

    // A text of a description: a key that the game's own texts have (MO_HIGH) is taken from there
    static std::string ResolveText(const MenuPlugin& plugin, std::string_view text)
    {
        if (&plugin == &ownPlugin && text.size() > 1 && text.size() <= sizeof(SettingsTables::Option::label) && text[0] == '@' && text[1] != '@')
            if (auto key = std::string(text.substr(1)); CText::g_text && VanillaHasText(key))
                return FromGameText(CText::getText(key.c_str()));
        return plugin.file.texts.Resolve(text);
    }

    static std::string OwnText(std::string_view key)
    {
        return ownPlugin.file.texts.Resolve(key);
    }

    // A text of Fusion Fix's with values for its {} (std::format), the text itself when a translation breaks it
    template<class... Args> static std::string FormatText(std::string_view key, Args&&... args)
    {
        auto text = OwnText(key);
        try
        {
            return std::vformat(text, std::make_format_args(args...));
        }
        catch (const std::format_error&)
        {
            return text;
        }
    }

    // The label of a row. Settings applied after a restart have no marker (as before ASI Settings): changing one shows
    // the game's restart warning in the line below the menu (extrainfo.ixx, CSettings::RestartPending).
    static std::string LabelKey(const MenuPlugin& plugin, const asi_settings::Setting& setting)
    {
        return MenuKey(plugin, setting.label);
    }

    // A value as the menu shows it: the label of a choice, On or Off, the number with its unit
    static std::string ValueText(const MenuPlugin& plugin, const asi_settings::Setting& setting, const std::string& value)
    {
        if (setting.type == asi_settings::Type::Bool)
        {
            int on = 0;
            asi_settings::detail::ParseBool(value, on);
            return CText::g_text ? FromGameText(CText::getText(on ? "MO_ON" : "MO_OFF")) : (on ? "On" : "Off");
        }
        for (auto list : { &setting.choices, &setting.special, &setting.suggestions })
            for (auto& item : *list)
                if (SameValue(asi_settings::detail::JsonToText(item.value), value))
                    return item.label.empty() ? value : ResolveText(plugin, item.label);
        return value + (setting.unit.empty() ? "" : (setting.unit == "%" || setting.unit == "\xC2\xB0" ? "" : " ") + setting.unit);
    }

    static std::wstring SettingValueText(const MenuPlugin& plugin, std::string_view id)
    {
        auto setting = plugin.file.description.Find(id);
        return setting ? ToGameText(ValueText(plugin, *setting, plugin.Get(setting->id))) : std::wstring();
    }

    static std::string DefaultText(const asi_settings::Setting& setting)
    {
        asi_settings::Slot slot;
        slot.setting = &setting;
        asi_settings::SetDefault(slot);
        return asi_settings::FormatValue(slot);
    }

    // A row with a text and nothing to change: a heading above a group of rows, in capitals
    static bool AddHeading(MenuScreen screen, std::string_view text, std::string_view before = {}, const MenuPlugin& plugin = ownPlugin)
    {
        if (text.empty())
            return false;
        auto p = &plugin;
        auto key = AddMenuText(std::format("FFHD{:08X}", GetHash((plugin.file.id + "|" + std::string(text)).c_str())), [p, text = std::string(text)]
        {
            return Upper(ResolveText(*p, text));
        });
        return AddOption(screen, key, 0, 100, {}, -1, before, 0, 0xFF, true);
    }

    static bool IsHeading(const SettingsTables::Option& option)
    {
        return option.action == 0 && std::strncmp(option.label, "FFHD", 4) == 0;
    }

    // A page of Fusion Fix's settings opened from a row; the row shows the value given, when there is one
    static MenuScreen AddPage(MenuScreen parent, std::string_view text, std::string_view before = {}, bool displayGame = false,
        std::function<std::wstring()> value = {})
    {
        auto screen = AddSubmenu(parent, MenuKey(ownPlugin, text), displayGame, before);
        if (auto page = FindPage(static_cast<int32_t>(screen)))
            page->value = std::move(value);
        return screen;
    }

    // The text of a row's label key
    static std::wstring RowText(const SettingsTables::Option& option)
    {
        return CText::g_text ? CText::getText(option.label) : L"";
    }

    // ---------------------------------------------------------------------------------------------
    // Presets (ASI Settings): a Preset row whose choice is applied with Enter after a confirmation that lists the changes

    struct PresetSelector
    {
        int32_t pending = -1;           // the preset chosen in the Preset row, -1 for the one the values match
        int32_t preference = -1;        // of its row
        const MenuPlugin* plugin = nullptr;
        bool armed = false;             // without the game's warning screen: Enter was pressed once
    };
    static inline std::map<std::string, PresetSelector> presetSelectors;
    static inline DescribedRow presetDisplay;

    // The game's warning screen (CE): the frontend shows it while its state isn't -1, the menu's input waits for it
    static inline void(__cdecl* setWarningMessage)(const char*, const char*, int32_t, uint8_t, int32_t, uint8_t, const wchar_t*, const wchar_t*) = nullptr;
    static inline int32_t* warningState = nullptr;
    static inline PresetSelector* confirming = nullptr;
    static inline int32_t confirmingPreset = -1;
    static inline bool confirmReady = false;        // the keys answer once the one that opened it was let go

    // The plugin a selector sets, when it has presets. Presets of all plugins at once are FusionFixHub's, the menu
    // offers each plugin's own in its page.
    static std::vector<const MenuPlugin*> PresetPlugins(const MenuPlugin* plugin)
    {
        std::vector<const MenuPlugin*> plugins;
        if (plugin && plugin->HasPresets())
            plugins.push_back(plugin);
        return plugins;
    }

    // The preset all the plugins match, -1 for Custom
    static int32_t CurrentPreset(const std::vector<const MenuPlugin*>& plugins)
    {
        int32_t preset = -1;
        for (size_t i = 0; i < plugins.size(); ++i)
        {
            auto current = plugins[i]->CurrentPreset();
            if (current < 0 || (i > 0 && current != preset))
                return -1;
            preset = current;
        }
        return preset;
    }

    static void ResetPresetSelectors()
    {
        for (auto& [key, selector] : presetSelectors)
        {
            selector.pending = -1;
            selector.armed = false;
        }
    }

    // Whether a plugin's values are those of a preset (presets can share values)
    static bool MatchesPreset(const MenuPlugin& plugin, int32_t preset)
    {
        for (auto& slot : plugin.Slots())
            if (slot.setting->InPresets() &&
                !asi_settings::detail::JsonEquals((*slot.setting->presets)[preset], asi_settings::FormatValue(slot), !slot.setting->StringValued()))
                return false;
        return true;
    }

    // A preset chosen in the row that the values don't match yet, -1 for none
    static int32_t PendingPreset(const PresetSelector& selector)
    {
        if (selector.pending < 0 || selector.pending > 2)
            return -1;
        auto plugins = PresetPlugins(selector.plugin);
        if (std::all_of(plugins.begin(), plugins.end(), [&](auto plugin) { return MatchesPreset(*plugin, selector.pending); }))
            return -1;
        return selector.pending;
    }

    static PresetSelector* FindSelector(int32_t preference)
    {
        for (auto& [key, selector] : presetSelectors)
            if (selector.preference == preference && preference >= 0)
                return &selector;
        return nullptr;
    }

    // The Preset row of a plugin, at the top of its page
    static void AddPresetRow(MenuScreen screen, const MenuPlugin& owner, std::string_view before = {})
    {
        if (!owner.HasPresets())
            return;
        auto plugin = &owner;
        auto key = plugin->file.id;
        auto selector = &presetSelectors[key];
        selector->plugin = plugin;
        if (presetDisplay.display < 0)
        {
            std::vector<std::string> keys = { MenuKey(ownPlugin, "@ff.vanilla"), MenuKey(ownPlugin, "@ff.recommended"),
                MenuKey(ownPlugin, "@ff.enhanced"), MenuKey(ownPlugin, "@ff.custom") };
            std::vector<std::string_view> labels(keys.begin(), keys.end());
            presetDisplay.display = RegisterEnum(std::format("MENU_DISPLAY_FFH{:03}", describedDisplays++), labels);
            if (presetDisplay.display < 0)
                return;
        }
        auto& row = describedRows["preset|" + key];
        if (row.preference < 0)
        {
            // Values 0-2 are chosen, 3 shows Custom
            row.preference = RegisterCallbackPreference(2, [selector, plugin]
            {
                if (selector->pending >= 0)
                    return selector->pending;
                auto current = CurrentPreset(PresetPlugins(plugin));
                return current < 0 ? 3 : current;
            }, [selector](int32_t value)
            {
                selector->pending = value;
                selector->armed = false;
            });
            row.display = presetDisplay.display;
            row.scaler = 3;
        }
        selector->preference = row.preference;
        auto label = MenuKey(ownPlugin, "@ff.preset");
        AddOption(screen, label, row.preference, row.display, {}, -1, before, row.scaler);
    }

    // What a preset changes, as the warning screen shows it: the question, per plugin the changed settings ("old >
    // new"), cut to the 600 characters of the screen
    static std::wstring ConfirmationText(const PresetSelector& selector, int32_t preset)
    {
        static constexpr const char* names[] = { "@ff.vanilla", "@ff.recommended", "@ff.enhanced" };
        struct Change { std::string text; bool restart = false; };
        struct PluginChanges { std::string name; std::vector<Change> changes; };
        std::vector<PluginChanges> plugins;
        int32_t restarts = 0;
        size_t total = 0;
        for (auto plugin : PresetPlugins(selector.plugin))
        {
            PluginChanges entry{ plugin->Name() };
            for (auto& slot : plugin->Slots())
            {
                auto& setting = *slot.setting;
                if (!setting.InPresets())
                    continue;
                auto& target = (*setting.presets)[preset];
                auto current = asi_settings::FormatValue(slot);
                if (asi_settings::detail::JsonEquals(target, current, !setting.StringValued()))
                    continue;
                auto value = asi_settings::detail::JsonToText(target);
                bool restart = setting.apply == "restart";
                auto text = ResolveText(*plugin, setting.label) + ": " + ValueText(*plugin, setting, current) + " > " +
                    ValueText(*plugin, setting, value);
                // A value that can't be used now stays as it is until it can
                if (!MenuPlugin::IsUsable(plugin->Usable(setting), value))
                    text += " " + OwnText("@ff.confirm.unavailable");
                restarts += restart;
                entry.changes.push_back({ std::move(text), restart });
            }
            total += entry.changes.size();
            if (!entry.changes.empty())
                plugins.push_back(std::move(entry));
        }

        auto presetName = ResolveText(ownPlugin, names[preset]);
        auto title = FormatText("@ff.confirm.title", presetName);
        std::string footer;
        if (restarts)
            footer += "~n~" + FormatText(restarts == 1 ? "@ff.confirm.restart1" : "@ff.confirm.restart", restarts);
        if (total == 0)
            footer = "~n~" + OwnText("@ff.confirm.none") + footer;

        // The screen copies 600 characters: changes are listed while they fit, the rest is counted
        constexpr size_t Budget = 590;
        auto body = "~y~" + title + "~s~~n~";
        size_t shown = 0;
        auto more = [&](size_t left) { return "~n~" + FormatText("@ff.confirm.more", left); };
        for (auto& plugin : plugins)
        {
            auto header = "~n~~y~" + plugin.name + " (" + std::to_string(plugin.changes.size()) + ")~s~";
            if (asi_settings::text::Widen(body + header + footer + more(total - shown)).size() > Budget)
                break;
            body += header;
            for (auto& change : plugin.changes)
            {
                auto line = "~n~" + change.text;
                if (asi_settings::text::Widen(body + line + footer + more(total - shown - 1)).size() > Budget)
                    break;
                body += line;
                ++shown;
            }
        }
        if (shown < total)
            body += more(total - shown);
        body += footer;
        auto wide = ToGameText(body);
        if (wide.size() > Budget)
            wide.resize(Budget);
        return wide;
    }

    static void ApplyPreset(const PresetSelector& selector, int32_t preset)
    {
        for (auto plugin : PresetPlugins(selector.plugin))
            plugin->ApplyPreset(preset);
        ResetPresetSelectors();
        ResolveMenuTexts(true);
    }

    // Enter on a Preset row with a preset chosen: the game's warning screen asks, without it a second Enter applies
    static bool ConfirmPreset(PresetSelector& selector)
    {
        auto preset = PendingPreset(selector);
        if (preset < 0)
            return false;
        if (setWarningMessage && warningState)
        {
            CText::menuTexts[GetHash("FF_CONFIRM")] = ConfirmationText(selector, preset);
            confirming = &selector;
            confirmingPreset = preset;
            confirmReady = false;
            return true;
        }
        if (!selector.armed)
            selector.armed = true;
        else
            ApplyPreset(selector, preset);
        return true;
    }

    // While the confirmation is shown: its own keys answer, the game's menu ignores other input meanwhile. Setting
    // the warning screen again would start its fade again.
    static bool ProcessConfirmation()
    {
        if (!confirming)
            return false;
        if (*warningState == -1)
            setWarningMessage(nullptr, "FF_CONFIRM", 0x48, 0, -1, 0, nullptr, nullptr);
        auto yes = queryInput.ccall<uint8_t>(8, uint8_t(1), uint8_t(2), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0)) != 0;
        auto no = !yes && queryInput.ccall<uint8_t>(11, uint8_t(1), uint8_t(2), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0)) != 0;
        // The Enter that opened it is still pressed in the frame it opened
        if (!confirmReady)
        {
            confirmReady = !yes && !no;
            return true;
        }
        if (yes || no)
        {
            // The key that answered isn't seen by the menu (Back would leave the list)
            swallowFrames = 2;
            *warningState = -1;
            auto selector = std::exchange(confirming, nullptr);
            if (yes)
                ApplyPreset(*selector, confirmingPreset);
            else
                selector->pending = -1;
            FillMenu(0);
        }
        return true;
    }

    // ---------------------------------------------------------------------------------------------
    // Other plugins with ASI Settings in the game's menu (Fusion Fix is the menu host)

    static inline std::set<HMODULE> knownModules;
    static inline bool pluginsSearched = false;             // since the menu was opened
    static inline std::atomic<bool> conditionsChanged = false;
    static inline std::string shownVisibility;              // the visible rows of the screen last filled

    static void __cdecl OnOtherPluginChanged(void*, const char*)
    {
        conditionsChanged = true;
    }

    // Each time the menu is opened: plugins found since are added to the menu. Only one menu host draws other
    // plugins' pages; without the role only Fusion Fix's own settings are shown.
    static void FindOtherPlugins()
    {
        if (pluginsSearched)
            return;
        pluginsSearched = true;
        if (!ownPlugin.module || !asi_settings::ClaimHostRole())
            return;
        for (auto& found : asi_settings::FindLoadedPlugins(ownPlugin.module, GameLanguageTag()))
        {
            if (found.module == ownPlugin.module || knownModules.contains(found.module))
                continue;
            knownModules.insert(found.module);
            auto& plugin = otherPlugins.emplace_back();
            plugin.module = found.module;
            plugin.api = found.api;
            plugin.file = std::move(found.file);
            // Rows follow changes made elsewhere: presets, editors through Reload, other hosts
            if (plugin.Has(offsetof(AsiSettingsApi, Subscribe)) && plugin.api->Subscribe)
                plugin.api->Subscribe(OnOtherPluginChanged, nullptr);
            SnapshotRestartValues(plugin);
            AddPluginPages(plugin);
            menuTextsChanged = true;
        }
    }

    // The Plugins page of the Game tab (and of the title menu's Game): a row below the empty line after Fusion Fix's
    // rows, before Exit Game. Its column lists Fusion Fix (the presets) and the other plugins (CE), elsewhere they're
    // rows of the page that open their own.
    static MenuScreen PluginsPage(MenuScreen screen)
    {
        if (auto it = pluginsPages.find(static_cast<int32_t>(screen)); it != pluginsPages.end())
            return it->second;
        auto& before = pluginsBefore[static_cast<int32_t>(screen)];
        auto page = AddPage(screen, "@ff.plugins", before);
        pluginsPages[static_cast<int32_t>(screen)] = page;
        return page;
    }

    // The entries of a plugin in the Plugins page: the plugin, or when its settings don't fit one page, its pages,
    // then its groups ("Plugin: Group"), then parts of them. The first one starts with its presets.
    struct PluginEntry
    {
        std::string title;              // a text of the description, empty for the plugin's name
        std::vector<const asi_settings::Setting*> settings;
    };

    static int32_t RowsOf(const std::vector<const asi_settings::Setting*>& settings)
    {
        int32_t rows = 0;
        std::string group;
        for (auto setting : settings)
        {
            if (!setting->group.empty() && setting->group != group)
                ++rows;
            group = setting->group;
            ++rows;
        }
        return rows;
    }

    static std::vector<PluginEntry> PluginEntries(const MenuPlugin& plugin)
    {
        auto& description = plugin.file.description;
        auto settingsOf = [&](const std::string& page)
        {
            std::vector<const asi_settings::Setting*> settings;
            for (auto& setting : description.settings)
                if ((page.empty() || asi_settings::text::IEquals(setting.page, page)) && setting.InMenu() && setting.HasValue())
                    settings.push_back(&setting);
            std::stable_sort(settings.begin(), settings.end(), [](auto a, auto b) { return a->order < b->order; });
            return settings;
        };
        // Rows for the settings and the end of the list; the first page also has the Preset row
        const int32_t budget = PageRows - 1 - (plugin.HasPresets() ? 1 : 0);
        std::vector<PluginEntry> entries;
        auto all = settingsOf({});
        if (all.empty())
            return entries;
        if (RowsOf(all) <= budget)
            return { { {}, all } };

        std::vector<PluginEntry> pages;
        for (auto& page : description.pages)
            if (auto settings = settingsOf(page.id); !settings.empty())
                pages.push_back({ page.title, settings });
        if (pages.size() < 2)
            pages = { { {}, all } };
        for (auto& page : pages)
        {
            if (RowsOf(page.settings) <= budget)
            {
                entries.push_back(page);
                continue;
            }
            // By groups, a group in parts of what fits
            PluginEntry part;
            for (auto setting : page.settings)
            {
                auto next = part.settings;
                next.push_back(setting);
                if (!part.settings.empty() && (setting->group != part.settings.back()->group || RowsOf(next) > budget))
                {
                    entries.push_back(std::move(part));
                    part = {};
                }
                if (part.settings.empty())
                    part.title = setting->group.empty() ? page.title : setting->group;
                part.settings.push_back(setting);
            }
            if (!part.settings.empty())
                entries.push_back(std::move(part));
        }
        return entries;
    }

    // A plugin's settings in the Plugins page of the Game tab, its Preset row at the top of its first page
    static void AddPluginPages(const MenuPlugin& plugin)
    {
        auto entries = PluginEntries(plugin);
        auto p = &plugin;
        for (auto& entry : entries)
        {
            auto key = plugin.file.id + "|" + entry.title + "|" + std::to_string(entry.settings.front()->order) + entry.settings.front()->id;
            auto name = AddMenuText(std::format("FFPG{:08X}", GetHash(key.c_str())), [p, title = entry.title]
            {
                return title.empty() ? p->Name() : p->Name() + ": " + p->file.texts.Resolve(title);
            });
            for (auto screen : { MenuScreen::Game, MenuScreen::TitleGame })
            {
                auto plugins = PluginsPage(screen);
                if (plugins == MenuScreen::Invalid)
                    continue;
                auto target = AddCategory(plugins, name);
                if (target == MenuScreen::Invalid)
                    continue;
                if (&entry == &entries.front())
                    AddPresetRow(target, plugin);
                std::string group;
                for (auto setting : entry.settings)
                {
                    // The group of an entry named after it has no heading
                    if (!setting->group.empty() && setting->group != group && setting->group != entry.title)
                        AddHeading(target, setting->group, {}, plugin);
                    group = setting->group;
                    AddDescribedRow({ &target, 1 }, plugin, *setting, [p, setting]
                    {
                        return !p->Hidden(setting->id) && (!setting->visibleIf || p->Evaluate(*setting->visibleIf));
                    });
                }
            }
        }
    }

    static void SnapshotRestartValues(const MenuPlugin& plugin)
    {
        for (auto& setting : plugin.file.description.settings)
            if (setting.apply == "restart" && setting.HasValue())
                restartValues.try_emplace(plugin.file.id + "|" + setting.id, plugin.Get(setting.id));
    }

    // Restart settings changed since the game was started
    static int32_t PendingRestarts()
    {
        int32_t count = 0;
        auto check = [&](const MenuPlugin& plugin)
        {
            for (auto& setting : plugin.file.description.settings)
                if (auto it = restartValues.find(plugin.file.id + "|" + setting.id); it != restartValues.end() && !SameValue(it->second, plugin.Get(setting.id)))
                    ++count;
        };
        check(ownPlugin);
        for (auto& plugin : otherPlugins)
            check(plugin);
        return count;
    }

    // ---------------------------------------------------------------------------------------------
    // The help line and the look of the rows

    static bool RowEnabled(const RowSetting& row)
    {
        return !row.setting || !row.setting->enabledIf || row.plugin->Evaluate(*row.setting->enabledIf);
    }

    // Why a row can't be changed: the setting it waits for, when the condition names one
    static std::string DisabledNote(const RowSetting& row)
    {
        auto& condition = *row.setting->enabledIf;
        if (!condition.setting.empty())
            if (auto other = row.plugin->file.description.Find(condition.setting))
            {
                auto label = ResolveText(*row.plugin, other->label);
                return FormatText("@ff.note.disabled", label);
            }
        return OwnText("@ff.note.disabled.other");
    }

    static std::string Cut(std::string text, size_t length)
    {
        auto wide = asi_settings::text::Widen(text);
        if (wide.size() <= length)
            return text;
        wide.resize(length - 3);
        while (!wide.empty() && wide.back() == L' ')
            wide.pop_back();
        return Narrow(wide) + "...";
    }

    // The note of a row, at most one: why it's greyed, a warning for its value, values that can't be used now, a restart
    static std::string RowNote(const RowSetting& row)
    {
        if (!row.setting)
            return {};
        auto& setting = *row.setting;
        if (!RowEnabled(row))
            return DisabledNote(row);
        if (row.plugin == &ownPlugin)
        {
            auto gameText = [](const char* key) { return FromGameText(CText::getText(key)); };
            if (setting.id == "SHADOWS.ExtraNightShadows" && SettingsPlugin().Int(setting.id) != 0)
                return gameText("FF_WARN2");
            if (setting.id == "SHADOWS.ShadowFilter" && SettingsPlugin().Int(setting.id) == 2 && GetPrefIDByName("PREF_SHADOW_QUALITY") &&
                mPrefs[*GetPrefIDByName("PREF_SHADOW_QUALITY")] < 4)
                return gameText("FF_WARN6");
        }
        if (setting.type == asi_settings::Type::Choice || setting.type == asi_settings::Type::Bool)
        {
            if (auto usable = row.plugin->Usable(setting))
            {
                std::string missing;
                for (auto& choice : setting.choices)
                    if (auto value = asi_settings::detail::JsonToText(choice.value); !MenuPlugin::IsUsable(usable, value))
                        missing += (missing.empty() ? "" : ", ") + ValueText(*row.plugin, setting, value);
                if (!missing.empty())
                {
                    auto note = FormatText("@ff.note.unavailable", missing);
                    if (row.plugin == &ownPlugin && setting.id == "MISC.Antialiasing")
                        note += " " + OwnText("@ff.note.upscaler");
                    if (row.plugin == &ownPlugin && setting.id == "MAIN.GraphicsAPI" && !MenuPlugin::IsUsable(usable, "2"))
                        note += " " + OwnText("@ff.note.d3d9on12");
                    return note;
                }
            }
        }
        if (setting.apply == "restart")
        {
            auto pending = PendingRestarts();
            return pending > 1 ? FormatText("@ff.note.restarts", pending) : OwnText("@ff.note.restart");
        }
        return {};
    }

    // What a page opened from a row contains: the labels of its rows
    static std::string PageHelp(int32_t screen)
    {
        if (auto it = std::find_if(pluginsPages.begin(), pluginsPages.end(), [screen](auto& entry) { return static_cast<int32_t>(entry.second) == screen; });
            it != pluginsPages.end())
        {
            std::string names = ownPlugin.Name();
            for (auto& plugin : otherPlugins)
                names += ", " + plugin.Name();
            return FormatText("@ff.plugins.description", names);
        }
        std::string labels;
        for (auto& added : dynamicOptions)
        {
            if (added.screen != screen || added.gap || (added.visible && !added.visible()))
                continue;
            labels += (labels.empty() ? "" : ", ") + FromGameText(RowText(added.option).c_str());
        }
        return FormatText("@ff.page.contains", labels);
    }

    // The info text of the selected row of Fusion Fix or another plugin: its description and at most one note below
    // it. Empty for rows of the game.
    static std::wstring DescriptionText()
    {
        if (!currentScreen || *currentScreen < 0 || *currentScreen >= 73 || confirming)
            return {};
        auto& options = screens[*currentScreen].options;
        auto row = CMenu::getSelectedItem();
        if (!options.data || row < 0 || row >= options.count)
            return {};
        auto& option = options.data[row];
        std::string description, note;
        if (auto button = FindButton(row); button >= 0 && dynamicOptions[button].submenu != -1)
            description = PageHelp(dynamicOptions[button].submenu);
        else if (option.action == 127 || option.preference < firstCustomID)
            return {};
        else if (auto selector = FindSelector(option.preference))
        {
            description = OwnText(selector->plugin == &ownPlugin ? "@ff.preset.description.own" : "@ff.preset.description.plugin");
            if (auto preset = PendingPreset(*selector); preset >= 0)
                note = OwnText(setWarningMessage && warningState ? "@ff.preset.enter" : selector->armed ? "@ff.preset.again" : "@ff.preset.enter");
        }
        else if (auto it = rowSettings.find(option.preference); it != rowSettings.end() && it->second.setting)
        {
            auto& found = it->second;
            description = ResolveText(*found.plugin, found.description.empty() ? found.setting->description : found.description);
            note = RowNote(found);
            if (auto hint = rowHints.find(found.setting->id); hint != rowHints.end() && hint->second)
                if (auto text = hint->second(); !text.empty())
                    description = Cut(description, 300) + (description.empty() ? "" : "~n~~n~") + text;
        }
        else
            return {};
        std::wstring lines = ToGameText(Cut(description, 600));
        if (!note.empty())
            lines += (lines.empty() ? L"" : L"~n~~n~") + ToGameText(note.starts_with("~") ? note : "~r~" + note);
        return lines;
    }

    static constexpr const wchar_t* SubmenuMark = L" >";

    // Render thread, before the menu draws its rows: rows that open a page show the page's value. Rows keep the
    // menu's own colours (idle, hovered, selected); their labels are never changed, so they don't flicker.
    static void StyleRows(int32_t handle)
    {
        auto instance = handle >= 0 ? menuInstances[handle] : nullptr;
        if (!instance || handle != menuHandles[0] || *currentScreen < 0 || *currentScreen >= 73)
            return;
        auto& options = screens[*currentScreen].options;
        auto count = std::min<int32_t>(options.count, 50);
        for (int32_t row = 0; row < count && options.data; ++row)
        {
            auto button = FindButton(row);
            if (button < 0 || dynamicOptions[button].submenu == -1)
                continue;
            auto page = FindPage(dynamicOptions[button].submenu);
            if (!page || !page->value)
                continue;
            auto text = reinterpret_cast<wchar_t*>(instance + 4 + 6000 + 120 * row);
            if (auto value = page->value(); value != text)
                wcsncpy_s(text, 60, value.c_str(), _TRUNCATE);
        }
    }

    // The way to the page shown above its rows: "GAME > STARTUP AND WINDOW"
    static void DrawBreadcrumb(int32_t handle)
    {
        auto instance = handle >= 0 ? menuInstances[handle] : nullptr;
        auto page = ShownPage();
        if (!instance || handle != menuHandles[0] || !page || activeHost != *currentScreen || !tabState || hasTabModal() != 0 || confirming)
            return;
        auto root = GetRootScreen(activePage);
        if (root < 0 || root >= 73)
            return;
        auto text = ToGameText(Upper(FromGameText(CText::getText(screens[root].header)) + " > " + FromGameText(GetPageName(*page))));
        auto position = reinterpret_cast<float*>(instance + 0x35D0);
        auto scaleX = *reinterpret_cast<float*>(instance + 0x35DC) * *reinterpret_cast<float*>(instance + 0x35E4);
        auto scaleY = *reinterpret_cast<float*>(instance + 0x35E0) * *reinterpret_cast<float*>(instance + 0x35E8);
        auto font = fontStates + getFontContext() * 0x48;
        std::array<uint8_t, 0x48> saved;
        std::memcpy(saved.data(), font, saved.size());
        font[0x14] = 0;
        font[0x15] = 0;
        font[0x16] = 1;
        font[0x1A] = 255;
        setFontOrientation(1);
        setFontStyle(*reinterpret_cast<int32_t*>(instance + 0x35D8));
        setTabScale(scaleX * 0.6f, scaleY * 0.6f);
        setFontEdge(*reinterpret_cast<float*>(instance + 0x3608));
        setFontEdgeColor(uint32_t(instance[0x363C]) << 24);
        auto x = GetActiveCategories() ? bodyPosition[0] : position[0];
        setFontWrap(x - 0.02f, x + 1.0f);
        setTabColor(*reinterpret_cast<uint32_t*>(instance + 0x3630));
        printTab.ccall<void>(x, position[1] + 0.019f, (L"~m~" + text).c_str(), -1, -1);
        std::memcpy(font, saved.data(), saved.size());
    }

    // The menu's info text at the top right (Graphics: the video memory and what the selected row affects), for rows
    // of Fusion Fix and other plugins on every tab: at the place, in the font and the colour of the game's
    static inline float* (__cdecl* getInfoPosition)(float*) = nullptr;
    static inline float* (__cdecl* getInfoLabelWidth)(float*) = nullptr;
    static inline void(__cdecl* setFontDropShadow)(float) = nullptr;
    static inline SafetyHookMid infoTextRow;
    static inline std::array<std::vector<std::string>, 2> graphicsLabels;  // rows of the XML of Graphics, title Graphics

    static void DrawDescription(int32_t handle)
    {
        auto instance = handle >= 0 ? menuInstances[handle] : nullptr;
        if (!instance || handle != menuHandles[0] || !getInfoPosition || !tabState || hasTabModal() != 0 || confirming)
            return;
        if (GetActiveCategories() && categoryFocus)
            return;
        auto text = DescriptionText();
        if (text.empty())
            return;
        auto font = fontStates + getFontContext() * 0x48;
        std::array<uint8_t, 0x48> saved;
        std::memcpy(saved.data(), font, saved.size());
        float scale[2], position[2], width[2], right[2];
        getTabWidget(scale, 8);
        getInfoPosition(position);
        float x[2] = { position[0], position[1] };
        adjustForWidescreen(2, x, scale, nullptr);
        getInfoLabelWidth(width);
        width[1] = 0.0f;
        adjustForWidescreen(2, nullptr, width, nullptr);
        getTabWidget(right, 17);
        font[0x14] = 0;
        font[0x15] = 0;
        font[0x16] = 1;
        font[0x1A] = 255;
        setFontOrientation(1);
        setFontStyle(*reinterpret_cast<int32_t*>(instance + 0x35D8));
        setTabScale(scale[0], scale[1]);
        setFontDropShadow(0.0f);
        setFontEdge(1.0f);
        setFontEdgeColor(0xFF000000);
        auto left = x[0] + 2.0f * width[0];
        setFontWrap(left, right[0]);
        uint32_t colour = 0;
        setTabColor(*getHudColour(&colour, 59));
        printTab.ccall<void>(left, position[1] + 0.04f, text.c_str(), -1, -1);
        std::memcpy(font, saved.data(), saved.size());
    }

    // Graphics: the game picks its info text by the index of the selected row in its XML. Rows are found by their
    // label among the XML's rows; rows of Fusion Fix and pages have none of the game's.
    static void RecordGraphicsRows(int32_t screen)
    {
        auto slot = screen == 49 ? 0 : screen == 62 ? 1 : -1;
        if (slot < 0 || !graphicsLabels[slot].empty())
            return;
        auto& options = screens[screen].options;
        if (!options.data || options.count == 0)
            return;
        for (int32_t row = 0; row < options.count; ++row)
            if (options.data[row].action == 127 || options.data[row].preference >= firstCustomID)
                return;
        for (int32_t row = 0; row < options.count; ++row)
            graphicsLabels[slot].emplace_back(options.data[row].label, strnlen(options.data[row].label, sizeof(options.data[row].label)));
    }

    static void InfoTextRow(SafetyHookContext& regs)
    {
        constexpr auto None = static_cast<uintptr_t>(-92);
        auto screen = *currentScreen;
        auto slot = screen == 49 ? 0 : screen == 62 ? 1 : -1;
        auto row = static_cast<int32_t>(regs.eax);
        if (slot < 0 || row < 0)
            return;
        auto& options = screens[screen].options;
        if ((activePage != -1 && activeHost == screen) || !options.data || row >= options.count || graphicsLabels[slot].empty())
        {
            regs.eax = None;
            return;
        }
        auto& option = options.data[row];
        auto& labels = graphicsLabels[slot];
        auto found = std::find(labels.begin(), labels.end(), std::string(option.label, strnlen(option.label, sizeof(option.label))));
        regs.eax = option.action == 127 || option.preference >= firstCustomID || found == labels.end() ? None :
            static_cast<uintptr_t>(found - labels.begin());
    }

    // The preference of the selected row when it changes a value, -1 for rows that open a page or do something
    static int32_t SelectedSettingPreference()
    {
        if (!currentScreen || *currentScreen < 0 || *currentScreen >= 73)
            return -1;
        auto& options = screens[*currentScreen].options;
        auto row = CMenu::getSelectedItem();
        if (!options.data || row < 0 || row >= options.count || options.data[row].action != 1)
            return -1;
        return options.data[row].preference;
    }

    // Del on the keyboard, X on the gamepad: the selected row of Fusion Fix or another plugin goes back to its default
    static void ResetSelectedRow()
    {
        static bool deleteDown = false;
        auto down = IsKeyboardKeyPressed(VK_DELETE);
        auto pressed = (down && !deleteDown) || queryInput.ccall<uint8_t>(9, uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0)) != 0;
        deleteDown = down;
        auto preference = SelectedSettingPreference();
        if (!pressed || preference < firstCustomID)
            return;
        auto it = rowSettings.find(preference);
        if (it == rowSettings.end() || !it->second.setting || !RowEnabled(it->second))
            return;
        if (it->second.reset)
            it->second.reset();
        else
            it->second.plugin->Set(it->second.setting->id, DefaultText(*it->second.setting));
    }

    // Which rows of a screen with conditions are shown
    static std::string VisibleRows(int32_t logical)
    {
        std::string rows;
        for (auto& added : dynamicOptions)
            if (added.screen == logical && added.visible)
                rows += added.visible() ? '1' : '0';
        return rows;
    }

    // ---------------------------------------------------------------------------------------------
    // Changes of Fusion Fix's settings

    static inline DWORD gameThread = 0;
    static inline std::mutex changesMutex;
    static inline std::vector<int32_t> pendingChanges;

    // The plugin changed a value in effect: the preference follows and the feature applies it on the game's thread.
    // Changes from other threads (editors through Reload, other menu hosts) are applied with the next frame.
    static void OnPluginChange(int32_t preference)
    {
        if (gameThread && GetCurrentThreadId() != gameThread)
        {
            std::lock_guard lock(changesMutex);
            pendingChanges.push_back(preference);
            return;
        }
        ApplyChange(preference);
    }

    static void ApplyChange(int32_t preference)
    {
        auto it = mFusionPrefs.find(preference);
        if (it == mFusionPrefs.end() || it->second.id.empty())
            return;
        it->second.value = SettingsPlugin().Int(it->second.id);
        // Rows whose conditions depend on it are shown or left out
        conditionsChanged = true;
        if (it->second.callback)
            it->second.callback(it->second.value);
    }

    static std::string SettingId(std::string_view name)
    {
        if (!name.starts_with("PREF_"))
            return std::string(name);
        auto preference = GetPrefIDByName(name);
        if (preference && mFusionPrefs.contains(*preference))
            return mFusionPrefs.at(*preference).id;
        return {};
    }

public:
    // Once per frame on the game's thread: changes made on other threads, rows of other plugins whose conditions
    // changed
    static void ProcessChanges()
    {
        gameThread = GetCurrentThreadId();
        std::vector<int32_t> changes;
        {
            std::lock_guard lock(changesMutex);
            changes.swap(pendingChanges);
        }
        for (auto preference : changes)
            ApplyChange(preference);
        // The menu isn't processed while the warning screen is shown
        if (swallowFrames > 0)
            --swallowFrames;
        if (confirming && GetMenu())
            ProcessConfirmation();
        if (conditionsChanged.exchange(false) && GetMenu() && currentScreen && *currentScreen >= 0 && *currentScreen < 73 &&
            VisibleRows(GetLogicalScreen(*currentScreen)) != shownVisibility)
            FillMenu(0);
    }

    // The menu was closed: the next one looks for plugins again, presets wait for a new confirmation
    static void OnMenuClosed()
    {
        pluginsSearched = false;
        ResetPresetSelectors();
        if (std::exchange(confirming, nullptr) && warningState)
            *warningState = -1;
    }

private:
    static int32_t FindDisplay(std::string_view name)
    {
        // Value types drawn by the menu itself
        constexpr std::pair<std::string_view, int32_t> types[] =
        {
            { "MENU_DISPLAY_NONE", 100 }, { "MENU_DISPLAY_SLIDERBAR", 101 }, { "MENU_DISPLAY_ONE_NUMBER", 102 },
            { "MENU_DISPLAY_TWO_NUMBERS", 103 }, { "MENU_DISPLAY_VALUE_SLIDERBAR", 108 },
        };
        for (auto& [type, id] : types)
            if (type == name)
                return id;
        auto it = displayIDs.find(std::string(name));
        return it != displayIDs.end() ? it->second : -1;
    }

    // The values of a display (MENU_DISPLAY_...) that used to be defined in the frontend XML: GXT keys or literals
    static void DefineDisplay(std::string name, std::initializer_list<std::string_view> labels)
    {
        DefineDisplay(std::move(name), std::vector<std::string>(labels.begin(), labels.end()));
    }

    static void DefineDisplay(std::string name, const std::vector<std::string>& labels)
    {
        auto it = displayIDs.find(name);
        auto id = it != displayIDs.end() ? it->second : nextDisplayID;
        if (id >= static_cast<int32_t>(SettingsTables::DisplayCapacity))
            return;
        std::vector<SettingsTables::DisplayValue> values;
        for (auto label : labels)
        {
            SettingsTables::DisplayValue value;
            std::memcpy(value.text, label.data(), std::min(label.size(), sizeof(value.text) - 1));
            value.value = static_cast<int32_t>(values.size());
            values.push_back(value);
        }
        if (it == displayIDs.end())
        {
            displayIDs.emplace(std::move(name), id);
            ++nextDisplayID;
        }
        dynamicDisplays[id] = std::move(values);
    }

    static std::wstring ToWide(std::string_view text, int32_t capacity)
    {
        std::wstring wide(capacity, L'\0');
        auto length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), static_cast<int32_t>(text.size()), wide.data(), capacity - 1);
        wide.resize(length > 0 ? length : 0);
        return GameFont::ToGame(std::move(wide), GameLanguageTag());
    }

    // An in-memory preference whose value is read and written through the callbacks
    static int32_t RegisterCallbackPreference(int32_t maximum, std::function<int32_t()> getter, std::function<void(int32_t)> setter)
    {
        auto id = RegisterPreference(std::format("PREF_FFMENU_{}", callbackCount++), maximum);
        if (id >= 0)
        {
            mFusionPrefs.at(id).getter = std::move(getter);
            mFusionPrefs.at(id).setter = std::move(setter);
        }
        return id;
    }

    // Labels up to 15 bytes are stored in the row, longer ones as a text key resolved by CText
    static std::string StoredLabel(std::string_view label)
    {
        if (label.size() < sizeof(SettingsTables::Option::label))
            return std::string(label);
        return std::format("FFL{:08X}", GetHash(std::string(label).c_str()));
    }

    static bool CanAddOption(MenuScreen screen, std::string_view label)
    {
        auto id = static_cast<int32_t>(screen);
        auto page = FindPage(id);
        if (!page)
        {
            switch (screen)
            {
            case MenuScreen::Game: case MenuScreen::Controls: case MenuScreen::NetworkControls: case MenuScreen::Audio:
            case MenuScreen::Display: case MenuScreen::Graphics: case MenuScreen::TitleControls: case MenuScreen::TitleAudio:
            case MenuScreen::TitleDisplay: case MenuScreen::TitleGraphics: case MenuScreen::TitleGame:
            case MenuScreen::KeyboardOptions: case MenuScreen::ControllerOptions: break;
            default: return false;
            }
        }
        if (label.empty() || label.size() >= 60 || label.find('\0') != label.npos)
            return false;
        auto stored = StoredLabel(label);
        for (auto& added : dynamicOptions)
            if (added.screen == id && stored == added.option.label)
                return false;
        if (page)
        {
            // Rows of a page, END_OF_MENU_OPTIONS included
            auto rows = std::count_if(dynamicOptions.begin(), dynamicOptions.end(), [id](auto& added) { return added.screen == id; });
            return static_cast<size_t>(rows) + 1 < page->rows.size();
        }
        auto& options = screens[id].options;
        size_t pending = 1;
        for (auto& added : dynamicOptions)
            if (added.screen == id)
            {
                bool present = false;
                for (size_t i = 0; i < options.count; ++i)
                    present |= std::strcmp(options.data[i].label, added.option.label) == 0;
                pending += !present;
            }
        return options.count + pending <= 50;
    }

    static bool AddOption(MenuScreen screen, std::string_view label, int32_t preference, int32_t display, std::function<void()> callback = {},
        int32_t submenu = -1, std::string_view before = {}, int32_t scaler = -1, uint8_t episodes = 0xFF, bool gap = false,
        std::function<bool()> visible = {})
    {
        if (!CanAddOption(screen, label))
            return false;
        auto stored = StoredLabel(label);
        if (stored != label)
            CText::menuTexts[GetHash(stored.c_str())] = ToWide(label, 60);
        bool action = callback || submenu != -1;
        SettingsTables::Option option;
        option.action = gap ? 0 : action ? 127 : 1; // MENUOPT_NONE, custom, MENUOPT_ADJUST
        std::memcpy(option.label, stored.data(), stored.size());
        option.preference = static_cast<int16_t>(preference);
        option.scaler = gap || action ? 0 : static_cast<uint8_t>(scaler >= 0 ? scaler : SettingsTables::scalers[preference]);
        option.display = static_cast<uint8_t>(display);
        // Rows are also inserted before the row of a preference
        auto beforePreference = before.starts_with("PREF_") ? GetPrefIDByName(before).value_or(-1) : -1;
        dynamicOptions.push_back({ static_cast<int32_t>(screen), option, std::move(callback), submenu, std::string(before), beforePreference,
            episodes, gap, std::move(visible) });
        return true;
    }

    // The number next to MENU_DISPLAY_VALUE_SLIDERBAR rows: the game prints its own for a few graphics options,
    // FF's sliders replace it with their value
    static void SetValueText(uintptr_t rowOffset, uintptr_t buffer)
    {
        auto screen = *currentScreen;
        if (screen < 0 || screen >= 73)
            return;
        auto& options = screens[screen].options;
        if (!options.data || rowOffset / sizeof(SettingsTables::Option) >= options.count)
            return;
        auto& option = *reinterpret_cast<SettingsTables::Option*>(reinterpret_cast<uint8_t*>(options.data) + rowOffset);
        if (auto it = valueTexts.find(option.preference); it != valueTexts.end())
            wcsncpy_s(reinterpret_cast<wchar_t*>(buffer), 64, it->second().c_str(), _TRUNCATE);
    }

public:
    int32_t Get(int32_t prefID)
    {
        if (prefID >= firstCustomID)
        {
            auto it = mFusionPrefs.find(prefID);
            return it != mFusionPrefs.end() ? it->second.GetValue() : 0;
        }
        else
        {
            if (!mPrefs || prefID < 0)
                return 0;
            return mPrefs[prefID];
        }
    }
    auto Set(int32_t prefID, int32_t value)
    {
        if (prefID >= firstCustomID)
        {
            if (auto it = mFusionPrefs.find(prefID); it != mFusionPrefs.end())
                it->second.SetValue(value);
        }
        else
        {
            if (!mPrefs || prefID < 0)
                return;
            mPrefs[prefID] = value;
        }
    }
    int32_t Get(std::string_view name)
    {
        auto prefID = GetPrefIDByName(name);
        if (prefID) { return Get(*prefID); }
        return 0;
    }
    auto Set(std::string_view name, int32_t value)
    {
        auto prefID = GetPrefIDByName(name);
        if (prefID) return Set(*prefID, value);
    }
    auto isSame(int32_t id, std::string_view name)
    {
        auto prefID = GetPrefIDByName(name);
        if (prefID && *prefID == id)
            return true;
        return false;
    }
    std::optional<std::reference_wrapper<int32_t>> GetRef(std::string_view name)
    {
        auto prefID = GetPrefIDByName(name);
        if (prefID)
        {
            if (*prefID >= firstCustomID)
                return std::ref(mFusionPrefs.at(*prefID).value);
            else
            {
                if (!mPrefs)
                {
                    MessageBoxW(0, L"Can't GetRef of original PREF", 0, 0);
                    return std::nullopt;
                }
                return std::ref(mPrefs[*prefID]);
            }
        }
        return std::nullopt;
    }
    void SetCallback(std::string_view name, std::function<void(int32_t)>&& cb)
    {
        const auto prefID = GetPrefIDByName(name);
        if (prefID && mFusionPrefs.contains(*prefID)) mFusionPrefs.at(*prefID).callback = std::move(cb);
    }
    void RemoveCallback(std::string_view name)
    {
        const auto prefID = GetPrefIDByName(name);
        if (prefID && mFusionPrefs.contains(*prefID)) mFusionPrefs.at(*prefID).callback = nullptr;
    }
    // Values the predicate rejects can't be used now (ASI Settings, Availability): the menu skips them, the value in
    // effect falls back to the default or the first usable value, and the cfg keeps the player's choice, which comes
    // back once it can be used. The name is the preference (PREF_...) or the setting (SECTION.Key).
    void SetAvailability(std::string_view name, std::function<bool(int32_t)>&& available)
    {
        auto id = SettingId(name);
        if (id.empty())
        {
            const auto prefID = GetPrefIDByName(name);
            if (prefID && mFusionPrefs.contains(*prefID)) mFusionPrefs.at(*prefID).available = std::move(available);
            return;
        }
        SettingsPlugin().Availability(id, [available = std::move(available)](std::string_view value)
        {
            int number = 0;
            return !asi_settings::detail::ParseInt(value, number) || available(number);
        });
    }
    // Asks the availability again after what it depends on may have changed: the value in effect and the menu follow
    void RefreshAvailability(std::string_view name)
    {
        if (auto id = SettingId(name); !id.empty())
            SettingsPlugin().Refresh(id);
    }
    void ForEachPref(std::function<void(int32_t id, int32_t idStart, int32_t idEnd)>&& cb)
    {
        for (auto& it : mFusionPrefs)
        {
            cb(it.first, it.second.idStart, it.second.idEnd);
        }
    }
    auto operator()(int32_t i) { return Get(i); }
    auto operator()(std::string_view name) { return Get(name); }

    // The language chosen in the menu, -1 for the language of the installation
    void SaveLanguagePref(int32_t value)
    {
        SettingsPlugin().Set("LANGUAGEOVERRIDE.Language", std::to_string(value));
    }

    int32_t LoadLanguagePref()
    {
        return SettingsPlugin().Int("LANGUAGEOVERRIDE.Language");
    }

public:
    struct
    {
        enum eFpsCaps { eOFF, eCustom, e30, e40, e50, e60, e75, e100, e120, e144, e165, e200, e240 };
        const std::vector<int32_t> data = { 0, 0, 30, 40, 50, 60, 75, 100, 120, 144, 165, 200, 240 };
    } FpsCaps;

    struct
    {
        enum eTimecycText { eMO_DEF, eOFF, eIV, eTLAD, eTBOGT };
        const std::vector<const char*> data = { "MO_DEF", "OFF", "IV", "TLAD", "TBOGT" };
    } TimecycText;

    struct
    {
        enum eShadowFilterText
        {
            eSharp, eSoft, eCHSS
        };
        const std::vector<const char*> data = { "Sharp", "Soft", "CHSS" };
    } ShadowFilterText;

    struct
    {
        enum eDofText
        {
            eOff, eCutscenesOnly, eLow, eMedium, eHigh, eVeryHigh
        };
        const std::vector<const char*> data = { "Off", "Cutscenes Only", "Low", "Medium", "High", "Very High" };
    } DofText;

    struct
    {
        enum eTreeFxText
        {
            ePC, ePCPlus, eConsole
        };
        const std::vector<const char*> data = { "PC", "PC+", "Console" };
    } TreeFxText;

    struct
    {
        enum eTreeAlphaText { ePC, eConsole };
        const std::vector<const char*> data = { "PC", "Console" };
    } TreeAlphaText;

    struct
    {
        enum eButtonsText { eXbox360, eXboxOne, ePlaystation3, ePlaystation4, ePlaystation5, eNintendoSwitch, eSteamDeck, eSteamController };
        const std::vector<const char*> data = { "Xbox 360", "Xbox One", "Playstation 3", "Playstation 4", "Playstation 5", "Nintendo Switch", "Steam Deck", "Steam Controller" };
    } ButtonsText;

    struct
    {
        enum eAntialiasingText { eMO_OFF, eFXAA, eSMAA, eTAA, eDLAA, eFSR };
        const std::vector<const char*> data = { "MO_OFF", "FXAA", "SMAA", "TAA", "DLAA", "FSR" };
    } AntialiasingText;

    struct
    {
        enum eExtraNightShadowsText { eOff, eLampposts, eLampostsHeadl, eLampHeadlVNS };
        const std::vector<const char*> data = { "MO_OFF", "Lampposts", "LampostsHeadl", "LampHeadlVNS" };
    } ExtraNightShadowsText;

} FusionFixSettings;

export bool shouldModifyMapMenuBackground(int curMenuTab = *pMenuTab)
{
    static auto bTransparentMapMenu = FusionFixSettings.GetRef("PREF_TRANSPARENTMAPMENU");
    return bTransparentMapMenu->get() && fMenuBlur && curMenuTab == 3;
}

injector::hook_back<decltype(&Natives::GetNumberOfInstancesOfStreamedScript)> hbGET_NUMBER_OF_INSTANCES_OF_STREAMED_SCRIPT;
int32_t __cdecl NATIVE_GET_NUMBER_OF_INSTANCES_OF_STREAMED_SCRIPT(char* name)
{
    auto ret = hbGET_NUMBER_OF_INSTANCES_OF_STREAMED_SCRIPT.fun(name);

    if (!ret)
    {
        if (std::string_view(name).starts_with("PREF_"))
        {
            return FusionFixSettings.Get(name);
        }
    }

    return ret;
}

class Settings
{
public:
    Settings()
    {
        // The next pause menu starts at the screens' own options, not at a page or another category
        FusionFix::onMenuExitEvent() += []()
        {
            CSettings::ResetCustomScreens();
            CSettings::OnMenuClosed();
        };

        // Changes of settings made on other threads, and the rows of other plugins, follow on the game's thread
        FusionFix::onGameProcessEvent() += []()
        {
            CSettings::ProcessChanges();
        };
        FusionFix::onMenuDrawingEvent() += []()
        {
            CSettings::ProcessChanges();
        };

        FusionFix::onInitEventAsync() += []()
        {
            auto stationslimit = GetModulePath(GetModuleHandleW(NULL)).parent_path() / "pc" / "audio" / "Config" / "stationslimit.txt";

            std::ifstream is(stationslimit, std::ios::in);
            if (is)
            {
                int limit = -1;
                is >> limit;

                if (limit >= 0 && limit <= 23)
                {
                    auto pattern = hook::pattern("0F B6 35 ? ? ? ? 85 F6");
                    if (!pattern.empty())
                    {
                        static int stationsLimit = limit;
                        injector::WriteMemory(pattern.get_first(3), &stationsLimit, true);
                    }
                }
            }
        };

        FusionFix::onInitEventAsync() += []()
        {
            // Runtime preferences stay in the game's original array. Route the
            // shared frontend accesses through Get/Set for custom IDs, including
            // the extra reads used by wraparound and mouse/slider rendering.
            auto pattern = find_pattern("8B 1C 95 ? ? ? ? 89 54 24 14", "8B 1C 8D ? ? ? ? 89 4C 24 18");
            static auto readReg = *pattern.get_first<uint8_t>(2);
            struct IniReader
            {
                void operator()(injector::reg_pack& regs)
                {
                    regs.ebx = FusionFixSettings.Get(readReg == 0x95 ? regs.edx : regs.ecx);
                }
            }; injector::MakeInline<IniReader>(pattern.get_first(), pattern.get_first(7));

            pattern = find_pattern("8B 14 85 ? ? ? ? 66 83 F9 31", "8B 0C 8D ? ? ? ? 75 12");
            static auto displayReadReg = *pattern.get_first<uint8_t>(2);
            struct DisplayReader
            {
                void operator()(injector::reg_pack& regs)
                {
                    if (displayReadReg == 0x85)
                        regs.edx = FusionFixSettings.Get(regs.eax);
                    else
                        regs.ecx = FusionFixSettings.Get(regs.ecx);
                }
            }; injector::MakeInline<DisplayReader>(pattern.get_first(), pattern.get_first(7));

            // Replace CMP/JL together. Both continuations start by setting flags
            // themselves (TEST EBX or CMP ESI/EDI), so no flags need to be synthesized.
            static auto wrapRead = find_pattern("83 3C 85 ? ? ? ? 00 7C ? 85 DB 74", "83 3C 95 ? ? ? ? 00 7C ? 85 DB 74").get_first<uint8_t>();
            static auto wrapReadReg = wrapRead[2];
            static auto wrapNegative = wrapRead + 10 + *reinterpret_cast<int8_t*>(wrapRead + 9);
            static auto wrapReader = safetyhook::create_mid(wrapRead, [](SafetyHookContext& regs)
            {
                auto id = wrapReadReg == 0x85 ? regs.eax : regs.edx;
                regs.eip = reinterpret_cast<uintptr_t>(FusionFixSettings.Get(id) < 0 ? wrapNegative : wrapRead + 10);
            });

            // CMP/JE guards the existing mouse writer below. Skip the store for
            // an unchanged position, just as the native path does. Its next flag
            // consumer is preceded by TEST CL, CL (TEST AL, AL in 1.1.2.0).
            static auto mouseRead = find_pattern("F3 0F 2C C0 39 04 9D ? ? ? ? 74 ? 89 04 9D", "F3 0F 2C D0 39 14 BD ? ? ? ? 74 ? 0F BF 79 12").get_first<uint8_t>(4);
            static auto mouseReadReg = mouseRead[2];
            static auto mouseUnchanged = mouseRead + 9 + *reinterpret_cast<int8_t*>(mouseRead + 8);
            static auto mouseReader = safetyhook::create_mid(mouseRead, [](SafetyHookContext& regs)
            {
                auto id = mouseReadReg == 0x9D ? regs.ebx : regs.edi;
                auto value = mouseReadReg == 0x9D ? regs.eax : regs.edx;
                regs.eip = reinterpret_cast<uintptr_t>(FusionFixSettings.Get(id) == static_cast<int32_t>(value) ? mouseUnchanged : mouseRead + 9);
            });

            static auto sliderRead = find_pattern("66 0F 6E 04 85 ? ? ? ? 0F 5B C0 F3 0F 59 C8", "F3 0F 2A 0C 85 ? ? ? ? F3 0F 59 C1").get_first<uint8_t>();
            static auto sliderReadOpcode = sliderRead[0];
            static auto sliderReader = safetyhook::create_mid(sliderRead, [](SafetyHookContext& regs)
            {
                if (sliderReadOpcode == 0x66)
                {
                    // MOVD clears the upper 96 bits, before CVTDQ2PS.
                    regs.xmm0 = {};
                    regs.xmm0.u32[0] = FusionFixSettings.Get(regs.eax);
                }
                else
                {
                    // 1.1.2.0 uses CVTSI2SS directly, preserving the upper lanes.
                    regs.xmm1.f32[0] = static_cast<float>(FusionFixSettings.Get(regs.eax));
                }
                regs.eip = reinterpret_cast<uintptr_t>(sliderRead + 9);
            });

            pattern = hook::pattern("89 1C ? ? ? ? ? E8 ? ? ? ? A1");
            static auto reg = *pattern.get_first<uint8_t>(2);
            struct IniWriter
            {
                void operator()(injector::reg_pack& regs)
                {
                    auto id = regs.edx;
                    auto value = regs.ebx;

                    if (reg == 0x8D)
                    {
                        id = regs.ecx;
                        value = regs.ebx;
                    }

                    FusionFixSettings.Set(id, value);

                    // custom handler for language switch
                    if (FusionFixSettings.isSame(id, "PREF_CURRENT_LANGUAGE"))
                    {
                        FusionFixSettings.SaveLanguagePref(value);
                    }

                }
            }; injector::MakeInline<IniWriter>(pattern.get_first(0), pattern.get_first(7));

            pattern = find_pattern("89 04 9D ? ? ? ? 0F BF 44 16", "89 14 BD ? ? ? ? 0F BF 49 12");
            static auto reg4 = *pattern.get_first<uint8_t>(2);
            struct IniWriterMouse
            {
                void operator()(injector::reg_pack& regs)
                {
                    auto id = regs.ebx;
                    auto value = regs.eax;

                    if (reg4 == 0xBD)
                    {
                        id = regs.edi;
                        value = regs.edx;
                    }

                    FusionFixSettings.Set(id, value);
                }
            }; injector::MakeInline<IniWriterMouse>(pattern.get_first(0), pattern.get_first(7));

            // show game in display menu
            pattern = find_pattern("75 1F FF 35 ? ? ? ? E8 ? ? ? ? 8B 4C 24 18", "75 10 57 E8 ? ? ? ? 83 C4 04 83 F8 03");
            injector::MakeNOP(pattern.get_first(), 2);

            pattern = hook::pattern("83 F8 03 0F 44 CE");
            if (!pattern.empty())
            {
                struct MenuHook
                {
                    void operator()(injector::reg_pack& regs)
                    {
                        regs.ecx = 1;
                    }
                }; injector::MakeInline<MenuHook>(pattern.get_first(0), pattern.get_first(6));
            }
            else
            {
                pattern = hook::pattern("75 02 B3 01 57 E8");
                if (!pattern.empty())
                    injector::MakeNOP(pattern.get_first(), 2);
            }

            pattern = hook::pattern("7E 4E 8A 1D ? ? ? ? 8B 35");
            if (!pattern.empty())
                injector::WriteMemory<uint8_t>(pattern.get_first(0), 0xEB, true);
            else
            {
                pattern = hook::pattern("83 F8 03 7F 06");
                if (!pattern.empty())
                    injector::MakeNOP(pattern.get_first(3), 2);
            }

            // Same but for the camera options of the Controls tab. The Graphics tab shows the game anyway (Extra Night
            // Shadows).
            static auto shouldModifyMenuBackground = [](int curMenuTab = *pMenuTab) -> bool
            {
                static auto centeredCamera = CSettings::GetPreferenceID("PREF_CENTEREDCAMERA");
                static auto centeredCameraFoot = CSettings::GetPreferenceID("PREF_CENTEREDCAMERAFOOT");
                auto selected = CSettings::GetSelectedPreference();
                return (curMenuTab == 8) ||  // Everything in Display Tab
                    (curMenuTab == 5 && (selected == centeredCamera || selected == centeredCameraFoot));    // Controls Tab
            };

            pattern = hook::pattern("83 FE ? 75 ? FF 35 ? ? ? ? E8 ? ? ? ? 83 C4 ? 85 C0 79");
            if (!pattern.empty())
            {
                static auto loc_5C27AD = resolve_displacement(pattern.get_first(3)).value();
                struct MenuBackgroundHook1
                {
                    void operator()(injector::reg_pack& regs)
                    {
                        if (regs.esi != 49 && !shouldModifyMenuBackground(regs.esi))
                        {
                            return_to(loc_5C27AD);
                        }
                    }
                }; injector::MakeInline<MenuBackgroundHook1>(pattern.get_first(0));
            }
            else
            {
                pattern = hook::pattern("83 3D ? ? ? ? ? 75 13 8B 0D ? ? ? ? 51 E8 ? ? ? ? 83 C4 04 85 C0 7D 21");
                static auto loc_5C27AD = resolve_displacement(pattern.get_first(7)).value();
                static auto dword_10FBF24 = *pattern.get_first<uint32_t>(2);
                struct MenuBackgroundHook1
                {
                    void operator()(injector::reg_pack& regs)
                    {
                        if (dword_10FBF24 == 49 && !shouldModifyMenuBackground(dword_10FBF24))
                        {
                            return_to(loc_5C27AD);
                        }
                    }
                }; injector::MakeInline<MenuBackgroundHook1>(pattern.get_first(0), pattern.get_first(9));
            }

            pattern = find_pattern("83 F8 ? 0F 84 ? ? ? ? 80 3D ? ? ? ? ? 0F 85 ? ? ? ? 83 F8", "83 F8 03 0F 84 ? ? ? ? 80 3D ? ? ? ? ? 0F 85 ? ? ? ? 83 F8 31");
            static auto loc_5A9815 = resolve_displacement(pattern.get_first(3)).value();
            struct MenuBackgroundHook2
            {
                void operator()(injector::reg_pack& regs)
                {
                    if (regs.eax == 3 || shouldModifyMenuBackground(regs.eax))
                    {
                        return_to(loc_5A9815);
                    }
                }
            }; injector::MakeInline<MenuBackgroundHook2>(pattern.get_first(0), pattern.get_first(9));

            // And for map tab
            pattern = hook::pattern("83 3D ? ? ? ? ? 75 ? 83 FE ? 74 ? C6 05 ? ? ? ? ? E8 ? ? ? ? 83 3D");
            if (!pattern.empty())
            {
                static auto loc_5A8557 = resolve_displacement(pattern.get_first(7)).value();
                struct MenuBackgroundHook3
                {
                    void operator()(injector::reg_pack& regs)
                    {
                        if (pMenuTab && (*pMenuTab != 49 && !shouldModifyMapMenuBackground(*pMenuTab)))
                        {
                            return_to(loc_5A8557);
                        }
                    }
                }; injector::MakeInline<MenuBackgroundHook3>(pattern.get_first(0), pattern.get_first(9));
            }
            else
            {
                pattern = hook::pattern("39 05 ? ? ? ? 75 12 39 44 24 14 74 2B C6 05 ? ? ? ? ? E8 ? ? ? ? B8 ? ? ? ? 39 05 ? ? ? ? 75 12");
                static auto loc_5A8557 = resolve_displacement(pattern.get_first(6)).value();
                struct MenuBackgroundHook3
                {
                    void operator()(injector::reg_pack& regs)
                    {
                        if (pMenuTab && (*pMenuTab != 49 && !shouldModifyMapMenuBackground(*pMenuTab)))
                        {
                            return_to(loc_5A8557);
                        }
                    }
                }; injector::MakeInline<MenuBackgroundHook3>(pattern.get_first(0), pattern.get_first(8));
            }

            pattern = find_pattern("83 F8 ? 74 ? 83 F8 ? 75 ? 33 C9 8D 64 24 ? 8B 81 ? ? ? ? 3B 81 ? ? ? ? 0F 85", "83 F8 31 74 05 83 F8 3E 75 6F 33 C0 8B FF");
            static auto loc_5AC19A = resolve_displacement(pattern.get_first(3)).value();
            struct MenuBackgroundHook4
            {
                void operator()(injector::reg_pack& regs)
                {
                    CSettings::OnTabTransition(regs.ebx, regs.ebp);

                    if (regs.eax == 49 || shouldModifyMapMenuBackground(regs.eax))
                    {
                        return_to(loc_5AC19A);
                    }
                }
            }; injector::MakeInline<MenuBackgroundHook4>(pattern.get_first(0));

            // TLAD
            pattern = hook::pattern("8D 83 ? ? ? ? 50 8D 84 24 ? ? ? ? EB ? 8D 83 ? ? ? ? 50 8D 84 24 ? ? ? ? EB ? 8D 83 ? ? ? ? 50 8D 84 24 ? ? ? ? EB ? 8D 83");
            if (!pattern.empty())
            {
                struct MenuBackgroundHook5
                {
                    void operator()(injector::reg_pack& regs)
                    {
                        regs.eax = regs.ebx + 0x112;

                        if (shouldModifyMenuBackground())
                            regs.eax = regs.ebx + 0x114;
                    }
                }; injector::MakeInline<MenuBackgroundHook5>(pattern.get_first(0), pattern.get_first(6));
            }
            else
            {
                pattern = hook::pattern("8D 8D ? ? ? ? 51 52 E8 ? ? ? ? 8B 08 89 4C 24 18");
                struct MenuBackgroundHook5
                {
                    void operator()(injector::reg_pack& regs)
                    {
                        regs.ecx = regs.ebp + 0x112;

                        if (shouldModifyMenuBackground())
                            regs.ecx = regs.ebp + 0x114;
                    }
                }; injector::MakeInline<MenuBackgroundHook5>(pattern.get_first(0), pattern.get_first(6));
            }

            pattern = find_pattern("83 3D ? ? ? ? ? 74 0C 38 05 ? ? ? ? 0F 84 ? ? ? ? 8D 84 24 ? ? ? ? 68 ? ? ? ? 50", "83 3D ? ? ? ? ? 74 0C 38 1D ? ? ? ? 0F 84 ? ? ? ? 8D 84 24");
            static auto loc_5AF8EE = resolve_displacement(pattern.get_first(0)).value();
            struct MenuBackgroundHook6
            {
                void operator()(injector::reg_pack& regs)
                {
                    // Custom pages shown on the Display screen use the regular background
                    if (*pMenuTab == 8 && !CSettings::IsCustomScreenActive() && !shouldModifyMenuBackground())
                    {
                        return_to(loc_5AF8EE);
                    }
                }
            }; injector::MakeInline<MenuBackgroundHook6>(pattern.get_first(0), pattern.get_first(9));

            //menu scrolling
            pattern = find_pattern("83 F8 10 7E 37 6A 00 E8 ? ? ? ? 83 C4 04 8D 70 F8 E8 ? ? ? ? D9 5C 24 30", "83 F8 10 7E 2A 6A 00 E8 ? ? ? ? 83 E8 08 89 44 24 14");
            injector::WriteMemory<uint8_t>(pattern.get_first(2), 0x10 * 2, true);
            pattern = hook::pattern("8D 70 F8 E8 ? ? ? ? D9 5C 24 30");
            if (!pattern.empty())
                injector::WriteMemory<uint8_t>(pattern.get_first(2), 0xF0, true);
            else
            {
                pattern = hook::pattern("83 E8 08 89 44 24 14");
                if (!pattern.empty())
                    injector::WriteMemory<uint8_t>(pattern.get_first(2), 0x10, true);
            }
            pattern = find_pattern("83 FE 10 7F 08", "83 FF 10 7F 0C");
            injector::WriteMemory<uint8_t>(pattern.get_first(2), 0x10 * 2, true);
            pattern = find_pattern("83 F8 10 7E 37 6A 00 E8 ? ? ? ? 83 C4 04 8D 70 F8", "83 F8 ? 7E ? 6A 00 E8 ? ? ? ? 83 E8 08 89 44 24 24");
            injector::WriteMemory<uint8_t>(pattern.get_first(2), 0x10 * 2, true);
            pattern = hook::pattern("8D 70 F8 E8 ? ? ? ? D9 5C 24 38");
            if (!pattern.empty())
                injector::WriteMemory<uint8_t>(pattern.get_first(2), 0xF0, true);
            else
            {
                pattern = hook::pattern("83 E8 08 89 44 24 24");
                if (!pattern.empty())
                    injector::WriteMemory<uint8_t>(pattern.get_first(2), 0x10, true);
            }
            pattern = find_pattern("8D 46 F0 66 0F 6E C0", "83 C7 F0 89 7C");
            injector::WriteMemory<uint8_t>(pattern.get_first(2), 0xE0, true);

            //Text
            CText::Hook();

            // FOG
            pattern = find_pattern("F3 0F 10 05 ? ? ? ? F3 0F 5C C1 F3 0F 59 C2 F3 0F 58 C1 F3 0F 11 05 ? ? ? ? F3 0F 10 05");
            if (!pattern.empty())
            {
                static float* farClipMultiplier = *pattern.get_first<float*>(4);
                injector::MakeNOP(pattern.get_first(), 8);
                static auto farClipMultiplierHook = safetyhook::create_mid(pattern.get_first(), [](SafetyHookContext& regs)
                {
                    static auto fog = FusionFixSettings.GetRef("PREF_VOLUMETRICFOG");
                    if (fog->get())
                        regs.xmm0.f32[0] = 1.0f;
                    else
                        regs.xmm0.f32[0] = *farClipMultiplier;

                    if (bIsQUB3D)
                    {
                        regs.xmm0.f32[0] = 0.1f;
                        bIsQUB3D = false;
                    }
                });
            }
            else
            {
                pattern = find_pattern("F3 0F 10 15 ? ? ? ? F3 0F 5C D1 F3 0F 59 D0 F3 0F 58 D1 F3 0F 11 15 ? ? ? ? F3 0F 10 15");
                static float* farClipMultiplier = *pattern.get_first<float*>(4);
                injector::MakeNOP(pattern.get_first(), 8);
                static auto farClipMultiplierHook = safetyhook::create_mid(pattern.get_first(), [](SafetyHookContext& regs)
                {
                    static auto fog = FusionFixSettings.GetRef("PREF_VOLUMETRICFOG");
                    if (fog->get())
                        regs.xmm2.f32[0] = 1.0f;
                    else
                        regs.xmm2.f32[0] = *farClipMultiplier;

                    if (bIsQUB3D)
                    {
                        regs.xmm2.f32[0] = 0.1f;
                        bIsQUB3D = false;
                    }
                });
            }

            pattern = find_pattern("F3 0F 10 05 ? ? ? ? F3 0F 5C C1 F3 0F 59 C2 F3 0F 58 C1 F3 0F 11 05 ? ? ? ? 8B E5");
            if (!pattern.empty())
            {
                static float* nearFogMultiplier = *pattern.get_first<float*>(4);
                injector::MakeNOP(pattern.get_first(), 8);
                static auto nearFogMultiplierHook = safetyhook::create_mid(pattern.get_first(), [](SafetyHookContext& regs)
                {
                    static auto fog = FusionFixSettings.GetRef("PREF_VOLUMETRICFOG");
                    if (fog->get())
                        regs.xmm0.f32[0] = 1.0f;
                    else
                        regs.xmm0.f32[0] = *nearFogMultiplier;
                });
            }
            else
            {
                pattern = find_pattern("F3 0F 10 15 ? ? ? ? F3 0F 5C D1 F3 0F 59 D0 F3 0F 58 D1 F3 0F 11 15 ? ? ? ? 8B E5");
                static float* nearFogMultiplier = *pattern.get_first<float*>(4);
                injector::MakeNOP(pattern.get_first(), 8);
                static auto nearFogMultiplierHook = safetyhook::create_mid(pattern.get_first(), [](SafetyHookContext& regs)
                {
                    static auto fog = FusionFixSettings.GetRef("PREF_VOLUMETRICFOG");
                    if (fog->get())
                        regs.xmm2.f32[0] = 1.0f;
                    else
                        regs.xmm2.f32[0] = *nearFogMultiplier;
                });
            }

            {
                static SafetyHookInline shGetUserLanguage{};
                auto GetUserLanguage = []() -> int
                {
                    auto l = FusionFixSettings.LoadLanguagePref();
                    if (l >= 0)
                        return l;
                    return shGetUserLanguage.call<int>();
                };

                auto pattern = hook::pattern("83 EC ? A1 ? ? ? ? 33 C4 89 44 24 ? A1 ? ? ? ? 8B 0D ? ? ? ? 53 55 56 33 ED 57 33 FF 85 C0 0F 45 E8");
                if (!pattern.empty())
                    shGetUserLanguage = safetyhook::create_inline(pattern.get_first(0), static_cast<int(*)()>(GetUserLanguage));
            }

            // Shadows setting
            {
                FusionFixSettings.SetCallback("PREF_EXTRANIGHTSHADOWS", [](int32_t value)
                {
                    if (value)
                    {
                        bExtraNightShadows = true;
                        bHeadlightShadows = value >= FusionFixSettings.ExtraNightShadowsText.eLampostsHeadl;
                        bVehicleNightShadows = value != FusionFixSettings.ExtraNightShadowsText.eLampostsHeadl;
                    }
                    else
                    {
                        bExtraNightShadows = false;
                        bHeadlightShadows = false;
                        bVehicleNightShadows = true;
                    }
                });

                if (FusionFixSettings("PREF_EXTRANIGHTSHADOWS"))
                {
                    bExtraNightShadows = true;
                    bHeadlightShadows = FusionFixSettings("PREF_EXTRANIGHTSHADOWS") >= FusionFixSettings.ExtraNightShadowsText.eLampostsHeadl;
                    bVehicleNightShadows = FusionFixSettings("PREF_EXTRANIGHTSHADOWS") != FusionFixSettings.ExtraNightShadowsText.eLampostsHeadl;
                }
            }
        };

        // FPS Counter
        if (GetD3DX9_43DLL())
        {
            SettingsReader reader;

            // [FOG]
            static bool bExtendedTimecycEditing = reader.ReadInteger("FOG", "ExtendedTimecycEditing", 0) != 0;

            // [EXPERIMENTAL]
            static bool bDisplayMemoryStats = reader.ReadInteger("EXPERIMENTAL", "DisplayMemoryStats", 0) != 0;

            static ID3DXFont* pFPSFont = nullptr;

            FusionFix::onBeforeReset() += []()
            {
                if (pFPSFont)
                    pFPSFont->Release();
                pFPSFont = nullptr;
            };

            FusionFix::onEndScene() += []()
            {
                static auto fpsc = FusionFixSettings.GetRef("PREF_FPSCOUNTER");
                static auto fpsCounterID = CSettings::GetPreferenceID("PREF_FPSCOUNTER");
                // Display and Graphics and their pages, not the pages of other tabs shown on the Display screen
                auto root = CSettings::CustomScreenRoot();
                auto graphicsTab = pMenuTab && ((*pMenuTab == 8 && (root == -1 || root == 8 || root == 49)) || *pMenuTab == 49);
                if (graphicsTab || (pMenuTab && CSettings::GetSelectedPreference() == fpsCounterID) || fpsc->get())
                {
                    static std::list<int> m_times;
                    static int fontSize = 0;

                    auto pDevice = *RageDirect3DDevice9::m_pRealDevice;

                    LARGE_INTEGER frequency;
                    LARGE_INTEGER time;
                    QueryPerformanceFrequency(&frequency);
                    QueryPerformanceCounter(&time);

                    if (m_times.size() == 50)
                        m_times.pop_front();
                    m_times.push_back(static_cast<int>(time.QuadPart));

                    uint32_t fps = 0;
                    if (m_times.size() >= 2)
                        fps = static_cast<uint32_t>(0.5f + (static_cast<double>(m_times.size() - 1) * static_cast<double>(frequency.QuadPart)) / static_cast<double>(m_times.back() - m_times.front()));

                    if (!pFPSFont)
                    {
                        D3DDEVICE_CREATION_PARAMETERS cparams;
                        RECT rect;
                        pDevice->GetCreationParameters(&cparams);
                        GetClientRect(cparams.hFocusWindow, &rect);

                        fontSize = rect.bottom / 20;

                        D3DXFONT_DESC fps_font;
                        ZeroMemory(&fps_font, sizeof(D3DXFONT_DESC));
                        fps_font.Height = fontSize;
                        fps_font.Width = 0;
                        fps_font.Weight = 400;
                        fps_font.MipLevels = 0;
                        fps_font.Italic = 0;
                        fps_font.CharSet = DEFAULT_CHARSET;
                        fps_font.OutputPrecision = OUT_DEFAULT_PRECIS;
                        fps_font.Quality = ANTIALIASED_QUALITY;
                        fps_font.PitchAndFamily = DEFAULT_PITCH | FF_DONTCARE;
                        wchar_t FaceName[] = L"Arial";
                        memcpy(&fps_font.FaceName, &FaceName, sizeof(FaceName));

                        if (D3DXCreateFontIndirectW(pDevice, &fps_font, &pFPSFont) != D3D_OK)
                            return;
                    }
                    else
                    {
                        auto DrawTextOutline = [](ID3DXFont* pFont, FLOAT X, FLOAT Y, D3DXCOLOR dColor, CONST PCHAR cString, ...)
                        {
                            const D3DXCOLOR BLACK(D3DCOLOR_XRGB(0, 0, 0));
                            CHAR cBuffer[101] = "";

                            va_list oArgs;
                            va_start(oArgs, cString);
                            _vsnprintf((cBuffer + strlen(cBuffer)), (sizeof(cBuffer) - strlen(cBuffer)), cString, oArgs);
                            va_end(oArgs);

                            RECT Rect[5] =
                            {
                                { LONG(X - 1), LONG(Y), LONG(X + 500.0f), LONG(Y + 50.0f) },
                                { LONG(X), LONG(Y - 1), LONG(X + 500.0f), LONG(Y + 50.0f) },
                                { LONG(X + 1), LONG(Y), LONG(X + 500.0f), LONG(Y + 50.0f) },
                                { LONG(X), LONG(Y + 1), LONG(X + 500.0f), LONG(Y + 50.0f) },
                                { LONG(X), LONG(Y), LONG(X + 500.0f), LONG(Y + 50.0f)},
                            };

                            if (dColor != BLACK)
                            {
                                for (auto i = 0; i < 4; i++)
                                    pFont->DrawTextA(NULL, cBuffer, -1, &Rect[i], DT_NOCLIP, BLACK);
                            }

                            pFont->DrawTextA(NULL, cBuffer, -1, &Rect[4], DT_NOCLIP, dColor);
                        };
                        auto curEp = _dwCurrentEpisode ? *_dwCurrentEpisode : 0;
                        static char str_format_fps[] = "%02d";
                        static const D3DXCOLOR TBOGT(D3DCOLOR_XRGB(0xD7, 0x11, 0x6E));
                        static const D3DXCOLOR TLAD(D3DCOLOR_XRGB(0x6F, 0x0D, 0x0F));
                        static const D3DXCOLOR IV(CText::hasViceCityStrings() ? D3DCOLOR_XRGB(0xF5, 0x8F, 0xBE) : D3DCOLOR_XRGB(0xF0, 0xA0, 0x00));

                        DrawTextOutline(pFPSFont, 10, 10, (curEp == 2) ? TBOGT : ((curEp == 1) ? TLAD : IV), str_format_fps, fps);

                        if (bExtendedTimecycEditing)
                        {
                            auto i = 0;

                            static char sVolFogDensity[] = "VolFogDensity: %f";
                            DrawTextOutline(pFPSFont, 10, FLOAT(fontSize * ++i), (curEp == 2) ? TBOGT : ((curEp == 1) ? TLAD : IV), sVolFogDensity, CTimeCycleExt::GetVolFogDensity());

                            static char sVolFogHeightFalloff[] = "VolFogHeightFalloff: %f";
                            DrawTextOutline(pFPSFont, 10, FLOAT(fontSize * ++i), (curEp == 2) ? TBOGT : ((curEp == 1) ? TLAD : IV), sVolFogHeightFalloff, CTimeCycleExt::GetVolFogHeightFalloff());

                            static char sVolFogAltitudeTweak[] = "VolFogAltitudeTweak: %f";
                            DrawTextOutline(pFPSFont, 10, FLOAT(fontSize * ++i), (curEp == 2) ? TBOGT : ((curEp == 1) ? TLAD : IV), sVolFogAltitudeTweak, CTimeCycleExt::GetVolFogAltitudeTweak());

                            static char sVolFogPower[] = "VolFogPower: %f";
                            DrawTextOutline(pFPSFont, 10, FLOAT(fontSize * ++i), (curEp == 2) ? TBOGT : ((curEp == 1) ? TLAD : IV), sVolFogPower, CTimeCycleExt::GetVolFogPower());

                            static char sSSIntensity[] = "SSIntensity: %f";
                            DrawTextOutline(pFPSFont, 10, FLOAT(fontSize * ++i), (curEp == 2) ? TBOGT : ((curEp == 1) ? TLAD : IV), sSSIntensity, CTimeCycleExt::GetSSIntensity());

                            static std::string_view modNames[] = {
                                "noambient", "NoAmbientmult", "qwnomoon", "qw2nomoon", "Brook_S2_TC", "MH_NOMOON", "KsS1nomoon1", "KsS1nomoon2", "KsS1nomoon3", "Brook_N_gden", "Buildsite_MH1",
                                "MH3carpark", "bkn2_Nomoon1", "bkn2_Nomoon2", "bks3norain", "MHNoMoon", "erosware", "QM_Nomoon", "NJ2nomoon", "bxwnomoon", "star_junc", "raytest",
                                "raytest2", "NJ02TUNNEL", "Internaldim", "SoosTunnel", "Nikwarehouse", "clam", "vlads", "generic", "jamcafe", "ten_str", "playboyx", "browner",
                                "limo", "STEVETUNNEL", "SUBWAY", "SUBWAY_STATION", "CARPARK", "RomansFl", "rscafe", "bernies", "Factorytest", "Factory", "bens2", "Hospital",
                                "Museum2", "Bada", "Badamine", "DrugDen", "Bank3", "Chase", "sexshop", "Diner", "hospitallobby", "Trespass", "ritz", "ritzf3", "ritzpen",
                                "intcafe", "firedept", "deal", "korrest", "korbar", "korkitch", "apart", "parktoilet", "HarlemProjects", "HarlemDrug", "JerSave", "burgershot",
                                "burgershotold", "HarlemTopFloor", "Irishbar", "boatcabs", "corplobby", "binco", "gazwarehouse", "bruciechopshop", "playboyxlobby", "statuestair",
                                "waste", "Bowl", "GunShop", "chinagun", "harlem_ten", "sw_har_decor", "sw_har_psh", "ten_standard", "ten_ornate", "ten_modern", "cluckinbell",
                                "Casino", "limooffice", "DrugDenStair", "project", "DwayneApart", "stair1", "stair2", "MH8_carpark", "MH8_Savehouse", "MH8_Showroom", "STUDIO_APART01",
                                "projectStair", "lightning", "playersettings", "playersettings2", "sniper", "sniper_ini", "binocular", "injured", "fast", "death", "death2", "death3",
                                "train_int", "busted", "cabaret", "lobby2office", "lobby2", "Police", "SUBWAYSERV", "SUBWAYENT", "SUBWAY_N", "SUBWAY_E", "SUBWAY_S", "SUBWAY_W",
                                "NIGHTSHADE", "PIZZAREST", "PIZZAREST2", "BRUCIE_STUDIO", "church", "Faustins", "Faustinsbase", "LittleJacobs", "Prison", "BernieCrane",
                                "McRearyHouse", "CopshopOffice", "Michelles", "sopranos", "Manny", "CIAoffice", "portacabin", "comclub", "elizabetas", "Bada", "fau3_a",
                                "imbhst", "em_4b", "g_1", "g_2", "g_3", "em_4", "df_2", "df_3", "lilj1_a", "imfau6", "imfau2", "wedint", "gm_2", "br_1", "br_4", "px_2",
                                "pxdf", "rb_4b", "vla1_a", "vla2_a", "vla4_a", "rom8_b", "pm_3", "em_1", "em_2", "em_3", "em_5", "em_7", "fau4_a", "show_1", "show_2",
                                "show_3", "show_4", "show_5", "show_6", "show_7", "show_8", "rb_4", "j_1", "rp_13", "rom2_a", "rom3_a", "rom5_a", "rom6_a", "rom8_a",
                                "r_9", "Classic", "Tweaked", "Cinema", "Verte", "Hot", "Steel", "Psyche", "Romantic", "Sepia", "Muddy", "Neon", "Rouge", "Bronze",
                                "Ulraviolet", "Eclipse", "Noire", "colors", "Vintage", "Fire", "Sketch", "em_1", "em_2", "em_5",
                            };

                            static char sModifiers[] = "%s %f";
                            for (const auto& it : currentTimecycleModifiers)
                            {
                                if (it.first >= 0 && it.first < CTimeCycleModifier::ARRAY_SIZE)
                                    DrawTextOutline(pFPSFont, 10, FLOAT(fontSize * ++i), (curEp == 2) ? TBOGT : ((curEp == 1) ? TLAD : IV), sModifiers, modNames[it.first].data(), it.second);
                            }
                        }
                        else if (bDisplayMemoryStats)
                        {
                            auto i = 0;

                            static char sStreamingMemory[] = "Streaming memory: %d / %d MB";
                            static char sVirtualHeapSize[] = "Virtual heap size: %d / %d MB";
                            static char sProcessMemory[] = "Process memory: %d / %d MB";

                            constexpr uint32_t MB = 1024 * 1024;

                            uint32_t nUsedPhysical = (CStreamingEngine::ms_info->m_PhysicalUsed + MB / 2) / MB;
                            uint32_t nMaxPhysical = (CStreamingEngine::ms_info->m_ResourcePhysicalAvailable + MB / 2) / MB;

                            uint32_t nUsedVirtual = (CStreamingEngine::ms_info->m_VirtualUsed + MB / 2) / MB;
                            uint32_t nMaxVirtual = (CStreamingEngine::ms_info->m_ResourceVirtualMax + MB / 2) / MB;

                            uint32_t nProcessMemory = 0;
                            uint32_t nTotalProcessMemory = 0;

                            PROCESS_MEMORY_COUNTERS ProcessMemoryCounter{};
                            if (GetProcessMemoryInfo(GetCurrentProcess(), &ProcessMemoryCounter, sizeof(ProcessMemoryCounter)))
                            {
                                nProcessMemory = ProcessMemoryCounter.WorkingSetSize / (1024 * 1024);
                            }

                            MEMORYSTATUSEX MemoryStatus{};
                            MemoryStatus.dwLength = sizeof(MemoryStatus);

                            if (GlobalMemoryStatusEx(&MemoryStatus))
                            {
                                constexpr uint64_t MaxProcessMemory = 4ull * 1024 * 1024 * 1024; // 4 GB

                                uint64_t TotalSystemMemory = MemoryStatus.ullTotalPhys;
                                if (TotalSystemMemory > MaxProcessMemory)
                                    TotalSystemMemory = MaxProcessMemory;

                                nTotalProcessMemory = static_cast<uint32_t>(TotalSystemMemory / (1024 * 1024));
                            }

                            // Physical memory must be corrected by adding used memory to it, as normally it decreases as used memory increases;
                            // It will still update continuously, because grcResourceCache updates it.
                            // Virtual memory is fixed size, so it does not update dynamically and can be displayed as is.
                            uint32_t CorrectedPhysical = nUsedPhysical + nMaxPhysical;

                            DrawTextOutline(pFPSFont, 10, FLOAT(fontSize * ++i), (curEp == 2) ? TBOGT : ((curEp == 1) ? TLAD : IV), sStreamingMemory, nUsedPhysical, CorrectedPhysical);
                            DrawTextOutline(pFPSFont, 10, FLOAT(fontSize * ++i), (curEp == 2) ? TBOGT : ((curEp == 1) ? TLAD : IV), sVirtualHeapSize, nUsedVirtual, nMaxVirtual);
                            DrawTextOutline(pFPSFont, 10, FLOAT(fontSize* ++i), (curEp == 2) ? TBOGT : ((curEp == 1) ? TLAD : IV), sProcessMemory, nProcessMemory, nTotalProcessMemory);
                        }
                    }
                }
            };

            if (bExtendedTimecycEditing)
            {
                FusionFix::onGameProcessEvent() += []()
                {
                    static auto oldState = IsKeyboardKeyPressed(VK_F3);
                    auto curState = IsKeyboardKeyPressed(VK_F3);
                    if (!oldState && curState)
                    {
                        TimeCycle::Initialise();
                        TimeCycle::InitialiseModifiers();
                    }
                    oldState = curState;
                };
            }

            FusionFix::onShutdownEvent() += []()
            {
                if (pFPSFont)
                    pFPSFont->Release();
                pFPSFont = nullptr;
            };

            FusionFix::onMenuDrawingEvent() += []()
            {
                if (*pMenuTab == 3)
                    fMenuBlur = 1.0f;
                else
                    fMenuBlur = 0.0f;
            };

            FusionFix::onMenuExitEvent() += []()
            {
                fMenuBlur = 0.0f;
            };

            auto pattern = find_pattern("51 56 57 64 8B 3D", "51 53 56 BE ? ? ? ? 33 DB");
            static auto readFrontendMenuHook = safetyhook::create_mid(pattern.get_first(0), [](SafetyHookContext& regs)
            {
                // The game frees the screens' rows before reading the XML again
                CSettings::ResetCustomScreens();

            });

            // RMB as previous in menus
            pattern = find_pattern("F6 C1 01 0F 84 ? ? ? ? EB 1B", "F6 C2 01 0F 84 ? ? ? ? 8B 04 B5");
            injector::WriteMemory<uint8_t>(pattern.get_first(2), 3, true);

            pattern = find_pattern("F6 C1 01 74 ? 68 ? ? ? ? B9", "F6 C1 01 74 ? 68");
            injector::WriteMemory<uint8_t>(pattern.get_first(2), 3, true);

            static bool bRmb = false;
            pattern = find_pattern("F6 C1 01 0F 84 ? ? ? ? 80 BC 16");
            if (!pattern.empty())
            {
                static auto GetRMBClick = safetyhook::create_mid(pattern.get_first(0), [](SafetyHookContext& regs)
                {
                    bRmb = (*(uint8_t*)&regs.ecx & 2) != 0;
                });
            }
            else
            {
                pattern = find_pattern("F6 C2 01 0F 84 ? ? ? ? 80 BC 08");
                static auto GetRMBClick = safetyhook::create_mid(pattern.get_first(0), [](SafetyHookContext& regs)
                {
                    bRmb = (*(uint8_t*)&regs.edx & 2) != 0;
                });
            }

            pattern = find_pattern("83 C4 1C 84 C0 74 ? 68 ? ? ? ? B9 ? ? ? ? E8 ? ? ? ? 8B 04 BD ? ? ? ? C7 80 ? ? ? ? ? ? ? ? 8B 04 BD ? ? ? ? 0F B6 84 30", "83 C4 1C 84 C0 74 ? 68 ? ? ? ? B9 ? ? ? ? E8 ? ? ? ? 8B 0C B5 ? ? ? ? C7 81 ? ? ? ? ? ? ? ? 8B 14 B5");
            static auto CheckRMBClick = safetyhook::create_mid(pattern.get_first(0), [](SafetyHookContext& regs)
            {
                if (*(uint8_t*)&regs.eax == 0)
                {
                    if (bRmb)
                    {
                        regs.eax = 1;
                        bRmb = false;
                    }
                }
            });

            // Make camera changes visible in menus
            {
                pattern = find_pattern("E8 ? ? ? ? 84 C0 74 12 80 3D ? ? ? ? ? 0F B6 DB", "E8 ? ? ? ? 84 C0 74 0A 38 1D");
                CTimer::hbIsUserPaused.fun = injector::MakeCALL(pattern.get_first(0), CTimer::IsCameraBaseProcessingUserPaused).get();

                pattern = hook::pattern("0A 05 ? ? ? ? 0A 05 ? ? ? ? 74 12");
                if (!pattern.empty())
                {
                    injector::MakeNOP(pattern.get_first(0), 6, true);
                    static auto CCam__BaseProcess_Hook = safetyhook::create_mid(pattern.get_first(0), [](SafetyHookContext& regs)
                    {
                        *(bool*)&regs.eax |= *CTimer::ms_bUserPause;

                        if (nCameraUnpauseTimer2 > 0)
                        {
                            *(bool*)&regs.eax = false;

                            nCameraUnpauseTimer2--;
                        }
                    });
                }
                else
                {
                    pattern = hook::pattern("E8 ? ? ? ? 84 C0 74 ? 80 3D ? ? ? ? ? 75 ? 80 3D ? ? ? ? ? 74 ? 84 DB");
                    CTimer::hbIsGamePaused.fun = injector::MakeCALL(pattern.get_first(0), CTimer::IsCameraBaseProcessingGamePaused).get();
                }
            }

            // Make timecycle changes visible in menus
            {
                pattern = hook::pattern("0A 05 ? ? ? ? 0A 05 ? ? ? ? 0F 85 ? ? ? ? E8 ? ? ? ? 84 C0 0F 85 ? ? ? ? F3 0F 10 05 ? ? ? ? F3 0F 11 04 24");
                if (!pattern.empty())
                {
                    injector::MakeNOP(pattern.get_first(0), 6, true);
                    static auto CVisualEffects__Update_Hook = safetyhook::create_mid(pattern.get_first(0), [](SafetyHookContext& regs)
                    {
                        *(bool*)&regs.eax |= *CTimer::ms_bUserPause;

                        if (nTimecycleUnpauseTimer > 0)
                        {
                            *(bool*)&regs.eax = false;

                            nTimecycleUnpauseTimer--;
                        }
                    });
                }
                else
                {
                    pattern = hook::pattern("E8 ? ? ? ? 84 C0 5F 0F 85");
                    CTimer::hbIsGamePaused.fun = injector::MakeCALL(pattern.get_first(0), CTimer::AreVisualEffectUpdatesGamePaused).get();
                }

                pattern = hook::pattern("0A 05 ? ? ? ? 0A 05 ? ? ? ? 74 ? 8B 0D");
                if (!pattern.empty())
                {
                    injector::MakeNOP(pattern.get_first(0), 6, true);
                    static auto TimeCycle__UpdateFinalize_Hook = safetyhook::create_mid(pattern.get_first(0), [](SafetyHookContext& regs)
                    {
                        *(bool*)&regs.eax |= *CTimer::ms_bUserPause;

                        if (nTimecycleUnpauseTimer > 0)
                        {
                            *(bool*)&regs.eax = false;

                            nTimecycleUnpauseTimer--;
                        }
                    });
                }
                else
                {
                    pattern = hook::pattern("E8 ? ? ? ? 84 C0 74 ? A1 ? ? ? ? 69 C0");
                    CTimer::hbIsGamePaused.fun = injector::MakeCALL(pattern.get_first(0), CTimer::AreVisualEffectUpdatesGamePaused).get();
                }
            }

            hbGET_NUMBER_OF_INSTANCES_OF_STREAMED_SCRIPT.fun = NativeOverride::Register(Natives::NativeHashes::GET_NUMBER_OF_INSTANCES_OF_STREAMED_SCRIPT, NATIVE_GET_NUMBER_OF_INSTANCES_OF_STREAMED_SCRIPT, "E8", 30);
        }
    }
} Settings;