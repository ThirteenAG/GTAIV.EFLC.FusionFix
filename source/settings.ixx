module;

#include <common.hxx>
#include <shlobj.h>
#include <d3dx9.h>
#include <psapi.h>
#include <span>
#include <stdexcept>

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

namespace CTimer
{
    char IsUserPaused()
    {
        if (nCameraUnpauseTimer1 > 0)
        {
            nCameraUnpauseTimer1--;

            return 0;
        }

        return *CTimer::ms_bUserPause;
    }

    injector::hook_back<int(*)()> hbIsGamePaused;
    int IsGamePaused_1()
    {
        if (nCameraUnpauseTimer2 > 0)
        {
            nCameraUnpauseTimer2--;

            return 0;
        }

        return hbIsGamePaused.fun();
    }

    int IsGamePaused_2()
    {
        if (nTimecycleUnpauseTimer > 0)
        {
            nTimecycleUnpauseTimer--;

            return 0;
        }

        return hbIsGamePaused.fun();
    }
}

export class CSettings
{
private:
    struct CSetting
    {
        int32_t value = 0;
        std::string prefName;
        std::string iniSec;
        std::string iniName;
        std::string strEnum;
        int32_t iniDefValInt = 0;
        std::function<void(int32_t value)> callback;
        int32_t idStart;
        int32_t idEnd;
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
            value = FindAvailable(v, v == previous + 1 || (previous == idEnd && v == idStart));
            if (setter)
                setter(value);
            else
                WriteToIni();
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
        auto ReadFromIni(auto& iniReader) { return iniReader.ReadInteger(iniSec, iniName, iniDefValInt); }
        auto ReadFromIni() { CIniReader iniReader(cfgPath); return ReadFromIni(iniReader); }
        void WriteToIni(auto& iniWriter) { if (!iniName.empty()) iniWriter.WriteInteger(iniSec, iniName, value, true); }
        void WriteToIni() { CIniReader iniWriter(cfgPath); if (!iniName.empty()) iniWriter.WriteInteger(iniSec, iniName, value, true); }
    };

    struct MenuPrefs
    {
        uint32_t prefID;
        char* name;
    };

    static inline std::filesystem::path cfgPath;
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
    };
    static inline std::vector<DynamicOption> dynamicOptions;
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
    };

    // A column of categories left of the options of a screen: the screen's own options, then pages
    struct CategoryScreen
    {
        int32_t screen = -1;
        std::vector<int32_t> pages;     // category 1 and up
        int32_t current = 0;
        std::array<int32_t, 8> selectedRows{};  // the row selected when the category was left
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
    static inline std::array<Bounds, 8> categoryBounds{};
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
    static inline std::array<Page, 16> pages;
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
    }

    // Empty lines added through the API: MENUOPT_NONE rows with a label of their own, shown without text. Like the
    // empty lines of the XML the menu can't select them, it only selects rows with a text.
    static bool IsGap(const SettingsTables::Option& option)
    {
        return option.action == 0 && std::strncmp(option.label, "FFGAP", 5) == 0;
    }

    // After the rows of another category are shown: the selection goes to the row selected there before, else to
    // the first selectable row (CE)
    static void FixSelection(int32_t menu, bool reset)
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
            return row >= 0 && row < count && options.data[row].action != 46 && instance[0x33E4 + row] != 0 && instance[0x3416 + row] != 0 &&
                instance[0x34AC + row] == 0 && *reinterpret_cast<const wchar_t*>(instance + 4 + 120 * row) != 0;
        };
        auto selected = *reinterpret_cast<int32_t*>(instance + 0x361C);
        if (!reset && selectable(selected))
            return;
        auto target = -1;
        if (auto categories = FindCategories(*currentScreen); reset && categories && selectable(categories->selectedRows[categories->current]))
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

    // The categories of the current screen while its column is shown: not in pages opened from its rows
    static CategoryScreen* GetActiveCategories()
    {
        if (!categoriesEnabled || *currentScreen < 0 || *currentScreen >= 73)
            return nullptr;
        auto categories = FindCategories(*currentScreen);
        auto page = FindPage(activePage);
        if (!categories || (page && (!page->category || activeHost != *currentScreen)))
            return nullptr;
        return categories;
    }

    static uint8_t* GetMenu()
    {
        auto handle = menuHandles[0];
        return handle >= 0 ? menuInstances[handle] : nullptr;
    }

    // The rows of the category selected in the column of the current screen replace its own rows
    static void ShowCategory()
    {
        auto categories = FindCategories(*currentScreen);
        if (auto page = FindPage(activePage); !categoriesEnabled || !categories || (page && !page->category))
            return;
        categories->current = std::clamp(categories->current, 0, static_cast<int32_t>(categories->pages.size()));
        if (categories->current > 0)
            ActivatePage(categories->pages[categories->current - 1]);
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
            categories.selectedRows.fill(-1);
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
            sidebarWidth = valueColumnWidth * 0.38f;
        }
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
        auto count = static_cast<int32_t>(categories.pages.size()) + 1;
        SelectCategory(categories, (categories.current + count + direction) % count);
    }

    static int32_t HitTestCategory(const CategoryScreen& categories)
    {
        auto x = mousePosition[0] * mouseScale[0];
        auto y = mousePosition[3] * mouseScale[2];
        for (size_t i = 0; i <= categories.pages.size(); ++i)
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
        if (menu != 0 || !categories || !tabState || tabState[0] != 0 || hasTabModal() != 0)
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
        auto count = std::min(categories.pages.size() + 1, categoryBounds.size());
        std::array<const wchar_t*, std::tuple_size_v<decltype(categoryBounds)>> names{};
        names[0] = CText::getText(screens[categories.screen].header);
        for (size_t i = 1; i < count; ++i)
            names[i] = GetPageName(*FindPage(categories.pages[i - 1]));

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
        auto width = 0.0f;
        for (size_t i = 0; i < count; ++i)
            width = std::max(width, measureTab.ccall<float>(names[i], uint8_t(1)));
        if (width > sidebarWidth * 0.9f)
            setTabScale(scaleX * sidebarWidth * 0.9f / width, scaleY);
        setFontWrap(x - 0.02f, x + sidebarWidth * 0.9f);

        for (size_t i = 0; i < count; ++i)
        {
            categoryBounds[i] = { x, y, x + sidebarWidth * 0.9f, y + step };
            bool selected = static_cast<int32_t>(i) == categories.current;
            bool highlighted = selected && categoryFocus && tabState[1] == 0;
            setTabColor(*reinterpret_cast<uint32_t*>(instance + (highlighted ? 0x3634 : 0x3630)));
            if (selected)
                printTab.ccall<void>(x - 0.015f, y, L">", -1, -1);
            printTab.ccall<void>(x, y, names[i], -1, -1);
            y += step;
        }
        std::memcpy(font, saved.data(), saved.size());
    }

    // Render thread: the layout is left as the main thread set it
    static void __cdecl MenuDraw(int32_t handle)
    {
        menuDraw.ccall<void>(handle);
        if (auto categories = GetActiveCategories(); categories && handle == menuHandles[0] && layoutMenu && layoutMenu == GetMenu())
            DrawCategories(*categories);
    }

    static int32_t __cdecl FillMenu(int32_t menu)
    {
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
            if (GetActiveCategories())
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
        if (CategoryInput(menu))
            return 0;
        auto previous = std::exchange(selectedButton, -1);
        auto previousMenu = std::exchange(inputMenu, menu);
        auto previousSubmenu = std::exchange(openSubmenu, -1);
        auto result = processMenu.ccall<uint8_t>(menu);
        auto submenu = openSubmenu;
        selectedButton = previous;
        inputMenu = previousMenu;
        openSubmenu = previousSubmenu;
        if (menu == 0 && submenu != -1)
            OpenPage(submenu);
        // Back in a page of the title menu, taken over by QueryInput
        if (menu == 0 && std::exchange(pendingTitleBack, false))
            if (auto page = FindPage(activePage); page && page->parent != -1)
                OpenPage(page->parent, page->parentRow);
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
            return found->parent;
        auto root = GetRootScreen(page);
        return IsTitleScreen(root) ? root : DisplayScreen;
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
        if ((current & (current ^ previous) & 1) != 0 && hasTabModal() == 0 &&
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
            // Back from a page opened from a row returns to it, from a category it leaves the screen
            if (auto page = FindPage(activePage); page && page->parent != -1 && !page->category)
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
            OpenPage(page->parent, page->parentRow);
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
            if (auto button = FindButton(row); button >= 0)
            {
                if (auto page = FindPage(dynamicOptions[button].submenu))
                {
                    // The row's text has room for 59 characters
                    wcsncpy_s(text, 60, GetPageName(*page), _TRUNCATE);
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
            if (auto page = FindPage(activePage); page && page->displayGame && activeHost == DisplayScreen && *frontendActive != 0 &&
                *currentScreen == DisplayScreen)
                *renderGame = 1;
        });
        // mov eax, [current screen]; pages get the menu background unless they show the game, categories look like
        // their screen
        pageBackgroundHook = safetyhook::create_mid(pageBackground, [](SafetyHookContext& regs)
        {
            regs.eax = static_cast<uintptr_t>(*currentScreen);
            if (auto page = FindPage(activePage); page && !page->category && activeHost == DisplayScreen && *currentScreen == DisplayScreen)
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
    static inline std::filesystem::path d3d9cfgPath;

    CSettings()
    {
        TCHAR szPath[MAX_PATH];
        std::vector<std::filesystem::path> cfgPaths;
        auto cfgName = GetThisModuleName().replace_extension(L".cfg");
        cfgPaths.emplace_back(GetThisModulePath() / cfgName);
        cfgPaths.emplace_back(GetExeModulePath() / cfgName);
        if (SUCCEEDED(SHGetFolderPath(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, szPath)))
            cfgPaths.emplace_back(std::filesystem::path(szPath) / L"Rockstar Games\\GTA IV\\" / cfgName);
        if (SUCCEEDED(SHGetFolderPath(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, szPath)))
            cfgPaths.emplace_back(std::filesystem::path(szPath) / GetThisModuleName().stem() / cfgName);
        if (SUCCEEDED(SHGetFolderPath(NULL, CSIDL_MYDOCUMENTS, NULL, 0, szPath)))
            cfgPaths.emplace_back(std::filesystem::path(szPath) / GetThisModuleName().stem() / cfgName);

        cfgPath = cfgPaths.front();
        for (auto& it : cfgPaths)
        {
            auto status = std::filesystem::status(it).permissions();
            if (status == std::filesystem::perms::unknown)
            {
                std::ofstream ofile;
                ofile.open(it, std::ios::binary);
                if (ofile.is_open())
                {
                    cfgPath = it;
                    ofile.close();
                    break;
                }
            }
            else if ((std::filesystem::perms::owner_read & status) == std::filesystem::perms::owner_read &&
            (std::filesystem::perms::owner_write & status) == std::filesystem::perms::owner_write)
            {
                cfgPath = it;
                break;
            }
        }

        {
            WCHAR szPath[MAX_PATH];
            std::vector<std::filesystem::path> d3d9cfgPaths;
            auto cfgName = L"d3d9.cfg";
            d3d9cfgPaths.emplace_back(GetExeModulePath() / cfgName);
            if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, szPath)))
                d3d9cfgPaths.emplace_back(std::filesystem::path(szPath) / L"Rockstar Games\\GTA IV\\" / cfgName);
            if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, szPath)))
                d3d9cfgPaths.emplace_back(std::filesystem::path(szPath) / cfgName);
            if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_MYDOCUMENTS, NULL, 0, szPath)))
                d3d9cfgPaths.emplace_back(std::filesystem::path(szPath) / cfgName);

            // First, try to find an existing readable file
            for (auto& it : d3d9cfgPaths)
            {
                std::error_code ec;
                if (std::filesystem::exists(it, ec) && !ec)
                {
                    auto status = std::filesystem::status(it, ec);
                    if (!ec && status.type() == std::filesystem::file_type::regular)
                    {
                        // Check if we can read from this file
                        std::ifstream testFile(it);
                        if (testFile.good())
                        {
                            d3d9cfgPath = it;
                            testFile.close();
                            break;
                        }
                    }
                }
            }

            // If no existing readable file found, find a writable location for new file creation
            if (d3d9cfgPath.empty())
            {
                for (auto& it : d3d9cfgPaths)
                {
                    std::error_code ec;
                    auto status = std::filesystem::status(it, ec);

                    if (ec && ec.value() == 2) // File doesn't exist, check if directory is writable
                    {
                        std::ofstream ofile;
                        ofile.open(it, std::ios::binary);
                        if (ofile.is_open())
                        {
                            d3d9cfgPath = it;
                            ofile.close();
                            break;
                        }
                    }
                    else if (!ec && ((std::filesystem::perms::owner_read & status.permissions()) == std::filesystem::perms::owner_read &&
                             (std::filesystem::perms::owner_write & status.permissions()) == std::filesystem::perms::owner_write))
                    {
                        d3d9cfgPath = it;
                        break;
                    }
                }
            }

            // Fallback to first path if nothing else worked
            if (d3d9cfgPath.empty())
            {
                d3d9cfgPath = d3d9cfgPaths.front();
            }
        }

        InitializeTables();

        CIniReader iniReader(cfgPath);

        // IDs and XML display records are independent of registration order.
        static CSetting arr[] = {
            { 0, "PREF_SKIP_INTRO",             "MAIN",       "SkipIntro",                          "",                           1, nullptr, 0, 1 },
            { 0, "PREF_SKIP_MENU",              "MAIN",       "SkipMenu",                           "",                           1, nullptr, 0, 1 },
            { 0, "PREF_BORDERLESS",             "MAIN",       "BorderlessWindowed",                 "",                           1, nullptr, 0, 1 },
            { 0, "PREF_FPS_LIMIT_PRESET",       "FRAMELIMIT", "FpsLimitPreset",                     "MENU_DISPLAY_FRAMELIMIT",    0, nullptr, (int32_t)FpsCaps.eOFF, std::distance(std::begin(FpsCaps.data), std::end(FpsCaps.data)) - 1 },
            { 0, "PREF_BLOOM",                  "MAIN",       "Bloom",                              "",                           1, nullptr, 0, 1 },
            { 0, "PREF_CONSOLE_GAMMA",          "MISC",       "ConsoleGamma",                       "MENU_DISPLAY_CONSOLE_GAMMA",                           1, nullptr, 0, 2 },
            { 0, "PREF_TIMECYC",                "MISC",       "ScreenFilter",                       "MENU_DISPLAY_TIMECYC",       0, nullptr, (int32_t)TimecycText.eMO_DEF, std::distance(std::begin(TimecycText.data), std::end(TimecycText.data)) - 1 },
            { 0, "PREF_WINDOWED",               "MAIN",       "Windowed",                           "",                           0, nullptr, 0, 1 },
            { 0, "PREF_DEFINITION",             "MAIN",       "Definition",                         "",                           1, nullptr, 0, 1 },
            { 0, "PREF_SHADOWFILTER",           "SHADOWS",    "ShadowFilter",                       "MENU_DISPLAY_SHADOWFILTER",  0, nullptr, (int32_t)ShadowFilterText.eSharp, std::distance(std::begin(ShadowFilterText.data), std::end(ShadowFilterText.data)) - 1 },
            { 0, "PREF_TREE_LIGHTING",          "MISC",       "TreeLighting",                       "MENU_DISPLAY_TREE_LIGHTING", 1, nullptr, (int32_t)TreeFxText.ePC, std::distance(std::begin(TreeFxText.data), std::end(TreeFxText.data)) - 1 },
            { 0, "PREF_TCYC_DOF",               "MISC",       "DepthOfField",                       "MENU_DISPLAY_DOF",           1, nullptr, (int32_t)DofText.eOff, std::distance(std::begin(DofText.data), std::end(DofText.data)) - 1 },
            { 0, "PREF_MOTIONBLUR",             "MAIN",       "MotionBlur",                         "",                           2, nullptr, 0, 4 },
            { 0, "PREF_LEDILLUMINATION",        "MISC",       "LightSyncRGB",                       "",                           0, nullptr, 0, 1 },
            { 0, "PREF_TREEALPHA",              "MISC",       "TreeAlpha",                          "MENU_DISPLAY_TREE_LIGHTING", 0, nullptr, (int32_t)TreeFxText.ePC, std::distance(std::begin(TreeFxText.data), std::end(TreeFxText.data)) - 1 },
            { 0, "PREF_SUNSHAFTS",              "MISC",       "SunShafts",                          "",                           0, nullptr, 0, 1 },
            { 0, "PREF_FPSCOUNTER",             "FRAMELIMIT", "DisplayFpsCounter",                  "",                           0, nullptr, 0, 1 },
            { 0, "PREF_ALWAYSRUN",              "MISC",       "AlwaysRun",                          "",                           0, nullptr, 0, 1 },
            { 0, "PREF_ALTDIALOGUE",            "MISC",       "AltDialogue",                        "",                           0, nullptr, 0, 1 },
            { 0, "PREF_COVERCENTERING",         "MISC",       "CameraCenteringInCover",             "",                           0, nullptr, 0, 1 },
            { 0, "PREF_KBCAMCENTERDELAY",       "MISC",       "DelayBeforeCenteringCameraKB",       "",                           4, nullptr, 0, 9 },
            { 0, "PREF_PADCAMCENTERDELAY",      "MISC",       "DelayBeforeCenteringCameraPad",      "",                           0, nullptr, 0, 9 },
            { 0, "PREF_CUSTOMFOV",              "MISC",       "FieldOfView",                        "",                           0, nullptr, 0, 9 },
            { 0, "PREF_RAWINPUT",               "MISC",       "RawInput",                           "",                           1, nullptr, 0, 1 },
            { 0, "PREF_BUTTONS",                "MISC",       "Buttons",                            "MENU_DISPLAY_BUTTONS",       1, nullptr, (int32_t)ButtonsText.eXbox360, std::distance(std::begin(ButtonsText.data), std::end(ButtonsText.data)) - 1 },
            { 0, "PREF_LETTERBOX",              "MISC",       "Letterbox",                          "",                           1, nullptr, 0, 1 },
            { 0, "PREF_PILLARBOX",              "MISC",       "Pillarbox",                          "",                           1, nullptr, 0, 1 },
            { 0, "PREF_ANTIALIASING",           "MISC",       "Antialiasing",                       "MENU_DISPLAY_ANTIALIASING",  2, nullptr, (int32_t)AntialiasingText.eMO_OFF, std::distance(std::begin(AntialiasingText.data), std::end(AntialiasingText.data)) - 1 },
            { 0, "PREF_UPDATE",                 "UPDATE",     "CheckForUpdates",                    "",                           0, nullptr, 0, 1 },
            { 0, "PREF_BLOCKONLOSTFOCUS",       "MAIN",       "BlockOnLostFocus",                   "",                           0, nullptr, 0, 1 },
            { 0, "PREF_TRANSPARENTMAPMENU",     "MISC",       "TransparentMapMenu",                 "",                           0, nullptr, 0, 1 },
            { 0, "PREF_EXTENDEDSNIPERCONTROLS", "MISC",       "ExtendedSniperControls",             "",                           0, nullptr, 0, 1 },
            { 0, "PREF_TIMEDEVENTS",            "MISC",       "TimedEvents",                        "",                           1, nullptr, 0, 1 },
            { 0, "PREF_VOLUMETRICFOG",          "FOG",        "VolumetricFog",                      "",                           0, nullptr, 0, 1 },
            { 0, "PREF_TONEMAPPING",            "MISC",       "ToneMapping",                        "",                           0, nullptr, 0, 1 },
            { 0, "PREF_ZOOMEDMOVEMENT",         "MISC",       "ZoomedMovement",                     "",                           1, nullptr, 0, 1 },
            { 0, "PREF_UNCLAMPLIGHTING",        "MISC",       "UnclampLighting",                    "",                           0, nullptr, 0, 1 },
            { 0, "PREF_DISTANTLIGHTS",          "MISC",       "DistantLights",                      "MENU_DISPLAY_DISTANT_LIGHTS",                           0, nullptr, 0, 1 },
            { 0, "PREF_CENTEREDCAMERA",         "MISC",       "CenteredVehCam",                     "",                           0, nullptr, 0, 1 },
            { 0, "PREF_CENTEREDCAMERAFOOT",     "MISC",       "CenteredFootCam",                    "",                           0, nullptr, 0, 1 },
            { 0, "PREF_CAMERASHAKE",            "MAIN",       "CameraShake",                        "",                           1, nullptr, 0, 1 },
            { 0, "PREF_CUTSCENEAUDIOSYNC",      "MAIN",       "CutsceneAudioSync",                  "MENU_DISPLAY_AUDIO_SYNC",                           0, nullptr, 0, 2 },
            { 0, "PREF_TURNINDICATORS",         "MISC",       "TurnIndicators",                     "",                           0, nullptr, 0, 1 },
            { 0, "PREF_EXTRANIGHTSHADOWS",      "SHADOWS",    "ExtraNightShadows",                  "MENU_DISPLAY_EXTRA_NIGHT_SHADOWS",                           0, nullptr, 0, 3 },
            { 0, "PREF_GRAPHICSAPI",            "MAIN",       "GraphicsAPI",                        "MENU_DISPLAY_GRAPHICS_API",                           0, nullptr, 0, 2 },
            { 0, "PREF_BULLETTRACES",           "MISC",       "AlwaysShowBulletTraces",             "",                           0, nullptr, 0, 1 },
            { 0, "PREF_AUTOEXPOSURE",           "MISC",       "ConsoleAutoExposure",                "",                           1, nullptr, 0, 1 },
            { 0, "PREF_KBCAMCENTERDELAYVEH",    "MISC",       "DelayBeforeCenteringCameraKBInCar",  "",                           0, nullptr, 0, 9 },
            { 0, "PREF_PADCAMCENTERDELAYVEH",   "MISC",       "DelayBeforeCenteringCameraPadInCar", "",                           0, nullptr, 0, 9 },
            { 0, "PREF_KBCAMTURNSPEEDVEH",      "MISC",       "CameraTurnSpeedKBInCar",             "",                           3, nullptr, 0, 7 },
            { 0, "PREF_PADCAMTURNSPEEDVEH",     "MISC",       "CameraTurnSpeedPadInCar",            "",                           0, nullptr, 0, 7 },
            { 0, "PREF_PADLOOKSENSITIVITY",     "MISC",       "PadLookSensitivity",                 "",                           10, nullptr, 0, 20 },
            { 0, "PREF_PADAIMSENSITIVITY",      "MISC",       "PadAimSensitivity",                  "",                           10, nullptr, 0, 20 },
            { 0, "PREF_MOUSEAIMSENSITIVITY",    "MISC",       "MouseAimSensitivity",                "",                           10, nullptr, 0, 20 },
            { 0, "PREF_NOWARDROBEFADING",       "MISC",       "DisableWardrobeTransition",          "",                           0, nullptr, 0, 1 },
            { 0, "PREF_STOPTAXI",               "MISC",       "InstantStopTaxi",                    "",                           0, nullptr, 0, 1 },
            { 0, "PREF_SAO",                    "MISC",       "AmbientOcclusion",                   "",                           0, nullptr, 0, 1 },
            { 0, "PREF_AUTOCLIMBLADDERS",       "MISC",       "AutoClimbLadders",                   "",                           0, nullptr, 0, 1 },
            { 0, "PREF_STUNTJUMPCAM",           "MISC",       "StuntJumpCamera",                    "",                           1, nullptr, 0, 1 },
            { 0, "PREF_ACTIONCAM",              "MISC",       "ActionCamera",                       "",                           1, nullptr, 0, 1 },
        };

        for (auto& setting : arr)
        {
            auto id = static_cast<int32_t>(aMenuPrefs.size());
            auto& stored = mFusionPrefs.emplace(id, std::move(setting)).first->second;
            stored.value = std::clamp(stored.ReadFromIni(iniReader), stored.idStart, stored.idEnd);
            SettingsTables::scalers[id] = stored.idEnd + 1;
            aMenuPrefs.emplace_back(id, stored.prefName.data());
            if (!stored.strEnum.empty() && !displayIDs.contains(stored.strEnum))
                displayIDs.emplace(stored.strEnum, nextDisplayID++);
        }

        // DirectX 12 (D3D9on12) is only enabled with API=2 in d3d9.cfg. The menu shows it while the game runs on
        // it, but only switches between DirectX 9 and Vulkan.
        FusionFixSettings.SetAvailability("PREF_GRAPHICSAPI", [](int32_t value) -> bool
        {
            return value != 2;
        });

        CIniReader d3d9cfg(d3d9cfgPath);
        ShowGraphicsAPI(std::clamp(d3d9cfg.ReadInteger("MAIN", "API", 0), 0, 2));
        InitializeMenuAPI();
        RegisterMenuRows();
    }

    // The options of Fusion Fix in the menus, the frontend XMLs are the original ones. In Display and Graphics they
    // are a category of their own, else they're where they used to be in the XMLs.
    void RegisterMenuRows()
    {
        DefineDisplay("MENU_DISPLAY_TIMECYC", { "MO_DEF", "MO_OFF", "IV", "TLAD", "TBOGT" });
        DefineDisplay("MENU_DISPLAY_FRAMELIMIT", { "MO_OFF", "Custom", "30", "40", "50", "60", "75", "100", "120", "144", "165", "200", "240" });
        DefineDisplay("MENU_DISPLAY_SHADOWFILTER", { "Sharp", "Soft", "CHSS" });
        DefineDisplay("MENU_DISPLAY_DOF", { "MO_OFF", "Cutscenes Only", "MO_LOW", "MO_MED", "MO_HIGH", "MO_VHIGH" });
        DefineDisplay("MENU_DISPLAY_TREE_LIGHTING", { "PC", "PC+", "Console" });
        DefineDisplay("MENU_DISPLAY_BUTTONS", { "Xbox 360", "Xbox One", "PlayStation 3", "PlayStation 4", "PlayStation 5", "Nintendo Switch", "Steam Deck", "FE_STEAMPAD" });
        DefineDisplay("MENU_DISPLAY_ANTIALIASING", { "MO_OFF", "FXAA", "SMAA", "TAA", "DLAA", "FSR" });
        DefineDisplay("MENU_DISPLAY_CONSOLE_GAMMA", { "MO_OFF", "Xbox 360", "PlayStation 3" });
        DefineDisplay("MENU_DISPLAY_AUDIO_SYNC", { "MO_OFF", "MO_ALT", "MO_ON" });
        DefineDisplay("MENU_DISPLAY_EXTRA_NIGHT_SHADOWS", { "MO_OFF", "Lampposts", "LampostsHeadl", "LampHeadlVNS" });
        DefineDisplay("MENU_DISPLAY_GRAPHICS_API", { "DirectX 9", "Vulkan", "DirectX 12" });
        DefineDisplay("MENU_DISPLAY_DISTANT_LIGHTS", { "MO_DEF", "Project2DFX" });
        DefineDisplay("MENU_DISPLAY_CUTSCENE_BARS", { "MO_OFF", "CutscBars1", "CutscBars2", "CutscBars3" });
        DefineDisplay("MENU_DISPLAY_WINDOW_MODE", { "MO_OFF", "MO_ON", "FF_BORDERLESS" });

        constexpr auto toggle = "MENU_DISPLAY_ON_OFF";
        constexpr auto slider = "MENU_DISPLAY_SLIDERBAR";
        constexpr uint8_t NotTLAD = 1 | 4;

        // Choices of two toggles each: off, pillarbox, letterbox and both in cutscenes; windowed off, on and borderless
        auto letterbox = *GetPrefIDByName("PREF_LETTERBOX");
        auto pillarbox = *GetPrefIDByName("PREF_PILLARBOX");
        auto cutsceneBars = RegisterCallbackPreference(3, [this, letterbox, pillarbox]
        {
            return (Get(letterbox) ? 2 : 0) + (Get(pillarbox) ? 1 : 0);
        }, [this, letterbox, pillarbox](int32_t value)
        {
            if (Get(pillarbox) != (value & 1))
                Set(pillarbox, value & 1);
            if (Get(letterbox) != (value >> 1))
                Set(letterbox, value >> 1);
        });
        auto windowed = *GetPrefIDByName("PREF_WINDOWED");
        auto borderless = *GetPrefIDByName("PREF_BORDERLESS");
        auto windowMode = RegisterCallbackPreference(2, [this, windowed, borderless]
        {
            return Get(windowed) ? (Get(borderless) ? 2 : 1) : 0;
        }, [this, windowed, borderless](int32_t value)
        {
            // The style first: the window has it once the game switches to windowed
            if (value != 0 && Get(borderless) != (value == 2 ? 1 : 0))
                Set(borderless, value == 2 ? 1 : 0);
            if (Get(windowed) != (value != 0 ? 1 : 0))
                Set(windowed, value != 0 ? 1 : 0);
        });

        // CE hides these rows every time the menu is filled, after the menu has shown them: they flicker. Marketplace
        // (TLAD, TBoGT) and voice output.
        if (!legacyExecutable)
        {
            RemoveOption(MenuScreen::Game, "MO_MARKETPLACE");
            for (auto screen : { MenuScreen::Audio, MenuScreen::TitleAudio })
                RemoveOption(screen, "MO_VOUT", "PREF_VOICE_OUTPUT");
        }

        // Game: the pause menu before Exit Game, the title menu at the end
        for (auto [screen, before] : { std::pair{ MenuScreen::Game, "MO_EXITGAM" }, std::pair{ MenuScreen::TitleGame, "" } })
        {
            if (screen == MenuScreen::TitleGame)
                AddEmptyLine(screen, before);
            AddRow(screen, "Skip Intro", "PREF_SKIP_INTRO", 2, toggle, before);
            AddRow(screen, "Skip Menu", "PREF_SKIP_MENU", 2, toggle, before);
            AddEmptyLine(screen, before);
            AddOption(screen, "CutscBars", cutsceneBars, FindDisplay("MENU_DISPLAY_CUTSCENE_BARS"), {}, -1, before, 4);
            AddRow(screen, "TranspMapMenu", "PREF_TRANSPARENTMAPMENU", 2, toggle, before);
            AddRow(screen, "FPS Counter", "PREF_FPSCOUNTER", 2, toggle, before);
            AddEmptyLine(screen, before);
            AddOption(screen, "Windowed", windowMode, FindDisplay("MENU_DISPLAY_WINDOW_MODE"), {}, -1, before, 3);
            AddRow(screen, "Focus Loss", "PREF_BLOCKONLOSTFOCUS", 2, toggle, before);
            AddEmptyLine(screen, before);
            AddRow(screen, "LightSyncRGB", "PREF_LEDILLUMINATION", 2, toggle, before);
            AddRow(screen, "Timed Events", "PREF_TIMEDEVENTS", 2, toggle, before);
            AddEmptyLine(screen, before);
            AddRow(screen, "Check Updates", "PREF_UPDATE", 2, toggle, before);
            if (screen == MenuScreen::Game)
                AddEmptyLine(screen, before);
        }

        // Controls, at the end. TLAD has no wardrobe.
        for (auto screen : { MenuScreen::Controls, MenuScreen::TitleControls })
        {
            AddEmptyLine(screen);
            AddRow(screen, "Always Run", "PREF_ALWAYSRUN", 2, toggle);
            AddRow(screen, "Zoomed Movement", "PREF_ZOOMEDMOVEMENT", 2, toggle);
            AddRow(screen, "Extended Sniper", "PREF_EXTENDEDSNIPERCONTROLS", 2, toggle);
            AddEmptyLine(screen);
            AddRow(screen, "Camera Shake", "PREF_CAMERASHAKE", 2, toggle);
            AddRow(screen, "CenteredVehCam", "PREF_CENTEREDCAMERA", 2, toggle);
            AddRow(screen, "CenteredFootCam", "PREF_CENTEREDCAMERAFOOT", 2, toggle);
            AddRow(screen, "Stunt Jump Cam", "PREF_STUNTJUMPCAM", 2, toggle);
            AddRow(screen, "Action Cam", "PREF_ACTIONCAM", 2, toggle);
            AddEmptyLine(screen);
            AddRow(screen, "Turn Indicators", "PREF_TURNINDICATORS", 2, toggle);
            AddRow(screen, "Bullet Traces", "PREF_BULLETTRACES", 2, toggle);
            AddRow(screen, "Wardrobe Fading", "PREF_NOWARDROBEFADING", 2, toggle, {}, NotTLAD);
            AddRow(screen, "Stop Taxi", "PREF_STOPTAXI", 2, toggle);
            AddRow(screen, "Ladders Climb", "PREF_AUTOCLIMBLADDERS", 2, toggle);
        }

        // Keyboard options: 21 steps of mouse sensitivity, the camera settings before Remap Keyboard
        SetOptionScaler(MenuScreen::KeyboardOptions, "MO_MOUSE", 21);
        AddRow(MenuScreen::KeyboardOptions, "MouseAimSens", "PREF_MOUSEAIMSENSITIVITY", 21, slider, "MO_MOWHEEL");
        AddRow(MenuScreen::KeyboardOptions, "CenterDelayFoot", "PREF_KBCAMCENTERDELAY", 10, slider, "MO_KBMAP");
        AddRow(MenuScreen::KeyboardOptions, "CenterDelayVeh", "PREF_KBCAMCENTERDELAYVEH", 10, slider, "MO_KBMAP");
        AddRow(MenuScreen::KeyboardOptions, "CamTurnSpeedVeh", "PREF_KBCAMTURNSPEEDVEH", 8, slider, "MO_KBMAP");
        AddRow(MenuScreen::KeyboardOptions, "Raw Input", "PREF_RAWINPUT", 2, toggle, "MO_KBMAP");
        AddEmptyLine(MenuScreen::KeyboardOptions, "MO_KBMAP");

        // Controller options: look and aim sensitivity sliders instead of the three sensitivity steps
        for (auto screen : { MenuScreen::ControllerOptions, MenuScreen::NetworkControls })
        {
            constexpr auto sensitivity = "PREF_CONTROLLER_SENSITIVITY";
            AddRow(screen, "PadLookSens", "PREF_PADLOOKSENSITIVITY", 21, slider, sensitivity);
            AddRow(screen, "MO_SENS", "PREF_PADAIMSENSITIVITY", 21, slider, sensitivity);
            if (screen == MenuScreen::ControllerOptions)
            {
                AddEmptyLine(screen, sensitivity);
                AddRow(screen, "CenterDelayFoot", "PREF_PADCAMCENTERDELAY", 10, slider, sensitivity);
                AddRow(screen, "CenterDelayVeh", "PREF_PADCAMCENTERDELAYVEH", 10, slider, sensitivity);
                AddRow(screen, "CamTurnSpeedVeh", "PREF_PADCAMTURNSPEEDVEH", 8, slider, sensitivity);
                AddRow(screen, "Gamepad Icons", "PREF_BUTTONS", 8, "MENU_DISPLAY_BUTTONS", sensitivity);
            }
            RemoveOption(screen, "MO_SENS", sensitivity);
        }

        // Audio: no microphone, the options at the end
        for (auto screen : { MenuScreen::Audio, MenuScreen::TitleAudio })
        {
            RemoveOption(screen, "MO_VMIC");
            AddEmptyLine(screen);
            AddRow(screen, "Alt. Dialogues", "PREF_ALTDIALOGUE", 2, toggle);
            AddRow(screen, "CutscAudioSync", "PREF_CUTSCENEAUDIOSYNC", 3, "MENU_DISPLAY_AUDIO_SYNC");
        }

        // Display and Graphics: an Advanced category, next to the original options or a submenu before Restore
        // Defaults and Graphics Analyzer
        for (auto screen : { MenuScreen::Display, MenuScreen::TitleDisplay })
        {
            auto category = AddCategory(screen, "FF_ADVANCED", "MO_DEF");
            if (category == MenuScreen::Invalid)
                continue;
            advancedDisplay.push_back(category);
            AddRow(category, "MO_FOV", "PREF_CUSTOMFOV", 10, slider);
            AddEmptyLine(category);
            AddRow(category, "MO_DOF", "PREF_DEFINITION", 2, toggle);
            AddRow(category, "Console Gamma", "PREF_CONSOLE_GAMMA", 3, "MENU_DISPLAY_CONSOLE_GAMMA");
            AddRow(category, "Auto Exposure", "PREF_AUTOEXPOSURE", 2, toggle);
            AddRow(category, "Motion Blur", "PREF_MOTIONBLUR", 5, "MENU_DISPLAY_REFLECTION_QUALITY");
            AddRow(category, "Depth of Field", "PREF_TCYC_DOF", 6, "MENU_DISPLAY_DOF");
            AddRow(category, "TreeFX", "PREF_TREE_LIGHTING", 3, "MENU_DISPLAY_TREE_LIGHTING");
            AddRow(category, "Tree Alpha", "PREF_TREEALPHA", 3, "MENU_DISPLAY_TREE_LIGHTING");
            AddEmptyLine(category);
            AddRow(category, "Bloom", "PREF_BLOOM", 2, toggle);
            AddRow(category, "Screen Filter", "PREF_TIMECYC", 5, "MENU_DISPLAY_TIMECYC");
            AddRow(category, "Distant Lights", "PREF_DISTANTLIGHTS", 2, "MENU_DISPLAY_DISTANT_LIGHTS");
        }

        for (auto screen : { MenuScreen::Graphics, MenuScreen::TitleGraphics })
        {
            // Definition is in Display
            RemoveOption(screen, "MO_DOF", "PREF_DOF");
            auto category = AddCategory(screen, "FF_ADVANCED", "MO_ANALYZER");
            if (category == MenuScreen::Invalid)
                continue;
            advancedGraphics.push_back(category);
            AddRow(category, "FPS Limiter", "PREF_FPS_LIMIT_PRESET", 13, "MENU_DISPLAY_FRAMELIMIT");
            AddRow(category, "Antialiasing", "PREF_ANTIALIASING", 6, "MENU_DISPLAY_ANTIALIASING");
            AddEmptyLine(category);
            AddRow(category, "Volumetric Fog", "PREF_VOLUMETRICFOG", 2, toggle);
            AddRow(category, "Sun Shafts", "PREF_SUNSHAFTS", 2, toggle);
            AddRow(category, "UnclampLighting", "PREF_UNCLAMPLIGHTING", 2, toggle);
            AddRow(category, "Tone Mapping", "PREF_TONEMAPPING", 2, toggle);
            AddRow(category, "AO", "PREF_SAO", 2, toggle);
            AddRow(category, "Shadow Filter", "PREF_SHADOWFILTER", 3, "MENU_DISPLAY_SHADOWFILTER");
            // Shown on the game, with a warning below the menu while it's on
            AddRow(category, "ExtraNightShad", "PREF_EXTRANIGHTSHADOWS", 4, "MENU_DISPLAY_EXTRA_NIGHT_SHADOWS");
            AddRow(category, "Graphics API", "PREF_GRAPHICSAPI", 3, "MENU_DISPLAY_GRAPHICS_API");
        }
    }

    // The graphics API the game runs on: d3d9.dll of Fusion Fix loads DXVK (vulkan.dll) for API=1 in d3d9.cfg
    // and enables D3D9on12 for API=2. Only the system d3d9.dll loads d3d9on12.dll, so it's checked first.
    static int32_t GetRunningGraphicsAPI()
    {
        if (GetModuleHandleW(L"d3d9on12.dll"))
            return 2;
        if (GetModuleHandleW(L"winevulkan.dll") || GetModuleHandleW(L"vulkan-1.dll"))
            return 1;
        return 0;
    }

    // Sets the value the menu shows without the availability check, which keeps DirectX 12 out of the menu's choices
    static void ShowGraphicsAPI(int32_t api)
    {
        FusionFixSettings.GetRef("PREF_GRAPHICSAPI")->get() = api;
    }
public:
    // The cfg the menu's preferences are saved in
    static const std::filesystem::path& GetConfigPath() { return cfgPath; }

    // A custom screen is shown in place of the Display screen
    static bool IsCustomScreenActive() { return activePage != -1; }
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
        if (pageCount == pages.size() || customTabCount == 4)
            return MenuScreen::Invalid;
        auto afterId = static_cast<int32_t>(after);
        if (after != MenuScreen::Controls && after != MenuScreen::Audio && after != MenuScreen::Display && after != MenuScreen::Graphics && !FindPage(afterId))
            return MenuScreen::Invalid;
        auto position = std::find_if(tabs.begin(), tabs.end(), [afterId](const Tab& tab) { return tab.id == afterId; });
        auto text = ToWide(label, 16);
        if (position == tabs.end() || text.empty())
            return MenuScreen::Invalid;

        auto screen = FirstCustomScreen + static_cast<int32_t>(pageCount);
        auto& page = pages[pageCount++];
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
    MenuScreen AddSubmenu(MenuScreen parent, std::string_view label, bool displayGame = false, std::string_view before = {})
    {
        auto parentId = static_cast<int32_t>(parent);
        if (!tabsInitialized || label.empty() || label.size() >= sizeof(Page::label) || label.find('\0') != label.npos ||
            (!FindPage(parentId) && parent != MenuScreen::Controls && parent != MenuScreen::Audio && parent != MenuScreen::Display &&
                parent != MenuScreen::Graphics && !IsTitleScreen(parentId)))
            return MenuScreen::Invalid;
        for (size_t i = 0; i < pageCount; ++i)
            if (pages[i].parent == parentId && !pages[i].category && label == pages[i].label)
                return static_cast<MenuScreen>(FirstCustomScreen + i);
        auto text = ToWide(label, 60);
        if (text.empty() || pageCount == pages.size())
            return MenuScreen::Invalid;

        auto screen = FirstCustomScreen + static_cast<int32_t>(pageCount);
        char key[16]{};
        std::snprintf(key, sizeof(key), "NY_SUB_%02u", static_cast<uint32_t>(pageCount));
        if (!AddOption(parent, key, static_cast<int32_t>(dynamicOptions.size()), 100, {}, screen, before))
            return MenuScreen::Invalid;

        auto& page = pages[pageCount++];
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

    // The Advanced category of Display or Graphics, in the pause menu and in the title menu (a submenu where the
    // column of categories isn't available). Options for both menus are added to each screen.
    const std::vector<MenuScreen>& GetAdvancedScreens(bool graphics) const
    {
        return graphics ? advancedGraphics : advancedDisplay;
    }

    // Call registration APIs on the game thread, or during startup before the
    // frontend is loaded. Values are zero-based positions, also for sliders.
    // Empty iniName creates an in-memory preference. Callback runs on Set().
    int32_t RegisterPreference(std::string name, int32_t maximum, int32_t defaultValue = 0,
        std::string iniSection = {}, std::string iniName = {}, std::function<void(int32_t)> callback = {})
    {
        if (!name.starts_with("PREF_") || name.size() == 5 || maximum < 1 || maximum > 254 ||
            GetPrefIDByName(name) || aMenuPrefs.size() >= SettingsTables::PreferenceCapacity ||
            (!iniName.empty() && iniSection.empty()))
            return -1;
        auto id = static_cast<int32_t>(aMenuPrefs.size());
        CSetting setting{ 0, std::move(name), std::move(iniSection), std::move(iniName), {},
            defaultValue, std::move(callback), 0, maximum };
        setting.value = std::clamp(setting.iniName.empty() ? defaultValue : setting.ReadFromIni(), 0, maximum);
        auto& stored = mFusionPrefs.emplace(id, std::move(setting)).first->second;
        aMenuPrefs.emplace_back(id, stored.prefName.data());
        SettingsTables::scalers[id] = maximum + 1;
        return id;
    }

    // Text entries are GXT keys or literal labels, using the native 15-byte
    // field. Register before XML loading when the name will be used in XML.
    int32_t RegisterEnum(std::string name, std::span<const std::string_view> labels)
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
    bool AddEmptyLine(MenuScreen screen, std::string_view before = {}, uint8_t episodes = 0xFF)
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

    // A category of options of Display or Graphics in the pause menu: a column of categories left of the options,
    // the screen's own options are the first one (CE). Elsewhere the category is a submenu opened from a row
    // before the row labelled 'before'. The name is a key of FF's texts or the text itself. Options of the category
    // are added to the returned screen.
    MenuScreen AddCategory(MenuScreen screen, std::string_view name, std::string_view before = {})
    {
        auto id = static_cast<int32_t>(screen);
        if (!categoriesEnabled || (screen != MenuScreen::Display && screen != MenuScreen::Graphics))
            return AddSubmenu(screen, name, true, before);
        if (name.empty() || name.size() >= sizeof(Page::label) || name.find('\0') != name.npos)
            return MenuScreen::Invalid;
        for (size_t i = 0; i < pageCount; ++i)
            if (pages[i].category && pages[i].parent == id && name == pages[i].label)
                return static_cast<MenuScreen>(FirstCustomScreen + i);
        if (pageCount == pages.size())
            return MenuScreen::Invalid;
        auto text = ToWide(name, 60);
        if (text.empty())
            return MenuScreen::Invalid;
        auto categories = FindCategories(id);
        if (!categories)
        {
            categories = &categoryScreens.emplace_back();
            categories->screen = id;
            categories->selectedRows.fill(-1);
        }
        if (categories->pages.size() + 1 >= categories->selectedRows.size())
            return MenuScreen::Invalid;

        auto page = FirstCustomScreen + static_cast<int32_t>(pageCount);
        auto& entry = pages[pageCount++];
        std::memcpy(entry.label, name.data(), name.size());
        entry.text = std::move(text);
        entry.parent = id;
        entry.category = true;
        entry.displayGame = screen == MenuScreen::Display;
        entry.rows[0].action = 46;
        categories->pages.push_back(page);
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

    // -1 for an unknown name
    static int32_t GetPreferenceID(std::string_view name)
    {
        return GetPrefIDByName(name).value_or(-1);
    }

private:
    static inline int32_t gapCount = 0;

    // A row like the <options> of the frontend XML: MENUOPT_ADJUST with a preference, a scaler and a display
    bool AddRow(MenuScreen screen, std::string_view label, std::string_view preference, int32_t scaler, std::string_view display,
        std::string_view before = {}, uint8_t episodes = 0xFF)
    {
        auto id = GetPrefIDByName(preference);
        auto displayId = FindDisplay(display);
        if (!id || displayId < 0)
            return false;
        return AddOption(screen, label, *id, displayId, {}, -1, before, scaler, episodes);
    }

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
    void DefineDisplay(std::string name, std::initializer_list<std::string_view> labels)
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
        return wide;
    }

    // An in-memory preference whose value is read and written through the callbacks
    int32_t RegisterCallbackPreference(int32_t maximum, std::function<int32_t()> getter, std::function<void(int32_t)> setter)
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
        int32_t submenu = -1, std::string_view before = {}, int32_t scaler = -1, uint8_t episodes = 0xFF, bool gap = false)
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
            episodes, gap });
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

    static inline std::vector<MenuScreen> advancedDisplay;
    static inline std::vector<MenuScreen> advancedGraphics;

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
    // Values the predicate rejects are skipped when the setting is changed in the menu
    void SetAvailability(std::string_view name, std::function<bool(int32_t)>&& available)
    {
        const auto prefID = GetPrefIDByName(name);
        if (prefID && mFusionPrefs.contains(*prefID)) mFusionPrefs.at(*prefID).available = std::move(available);
    }
    // Moves the current value to an available one after availability changed. The ini keeps the
    // user's choice, so a feature that is missing for one session comes back once it is available.
    void RefreshAvailability(std::string_view name)
    {
        const auto prefID = GetPrefIDByName(name);
        if (!prefID || !mFusionPrefs.contains(*prefID))
            return;
        auto& setting = mFusionPrefs.at(*prefID);
        auto saved = setting.iniName.empty() ? setting.value : std::clamp(setting.ReadFromIni(), setting.idStart, setting.idEnd);
        auto value = setting.FindAvailable(saved, false);
        if (value != setting.value)
        {
            setting.value = value;
            if (setting.callback)
                setting.callback(value);
        }
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

    void SaveLanguagePref(int32_t value)
    {
        CIniReader iniWriter(cfgPath);
        iniWriter.WriteInteger("LANGUAGEOVERRIDE", "Language", value, true);
    }

    int32_t LoadLanguagePref()
    {
        CIniReader iniReader(cfgPath);
        return iniReader.ReadInteger("LANGUAGEOVERRIDE", "Language", -1);
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

                    // custom handler for graphics api switch
                    if (FusionFixSettings.isSame(id, "PREF_GRAPHICSAPI"))
                    {
                        auto vulkan = LoadLibraryExW(L"vulkan.dll", NULL, LOAD_LIBRARY_AS_DATAFILE);
                        auto FusionFixGraphicsApiSwitch = GetProcAddress(GetModuleHandleW(L"d3d9.dll"), "FusionFixGraphicsApiSwitch");

                        if (vulkan == NULL || !FusionFixGraphicsApiSwitch)
                        {
                            CSettings::ShowGraphicsAPI(CSettings::GetRunningGraphicsAPI());
                        }
                        else
                        {
                            FreeLibrary(vulkan);
                            // Unavailable values were skipped, the setting holds the value that was picked
                            CIniReader d3d9cfg(CSettings::d3d9cfgPath);
                            d3d9cfg.WriteInteger("MAIN", "API", FusionFixSettings.Get(id), true);
                        }
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
            CIniReader iniReader("");

            // [FOG]
            static bool bExtendedTimecycEditing = iniReader.ReadInteger("FOG", "ExtendedTimecycEditing", 0) != 0;

            // [EXPERIMENTAL]
            static bool bDisplayMemoryStats = iniReader.ReadInteger("EXPERIMENTAL", "DisplayMemoryStats", 0) != 0;

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
                if (pMenuTab && *pMenuTab == 8 || *pMenuTab == 49 || (*pMenuTab == 0 && CSettings::GetSelectedPreference() == fpsCounterID) || fpsc->get())
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

                static bool bOnce = false;

                if (!bOnce)
                {
                    bOnce = true;
                    CSettings::ShowGraphicsAPI(CSettings::GetRunningGraphicsAPI());
                }
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
                injector::MakeCALL(pattern.get_first(0), CTimer::IsUserPaused);

                pattern = hook::pattern("0A 05 ? ? ? ? 0A 05 ? ? ? ? 74 12");
                if (!pattern.empty())
                {
                    injector::MakeNOP(pattern.get_first(0), 6, true);
                    static auto CCam__BaseProcess_Hook = safetyhook::create_mid(pattern.get_first(0), [](SafetyHookContext& regs)
                    {
                        *(uint8_t*)&regs.eax |= *CTimer::ms_bUserPause;

                        if (nCameraUnpauseTimer2 > 0)
                        {
                            *(uint8_t*)&regs.eax = 0;

                            nCameraUnpauseTimer2--;
                        }
                    });
                }
                else
                {
                    pattern = hook::pattern("E8 ? ? ? ? 84 C0 74 ? 80 3D ? ? ? ? ? 75 ? 80 3D ? ? ? ? ? 74 ? 84 DB");
                    CTimer::hbIsGamePaused.fun = injector::MakeCALL(pattern.get_first(0), CTimer::IsGamePaused_1).get();
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
                        *(uint8_t*)&regs.eax |= *CTimer::ms_bUserPause;

                        if (nTimecycleUnpauseTimer > 0)
                        {
                            *(uint8_t*)&regs.eax = 0;

                            nTimecycleUnpauseTimer--;
                        }
                    });
                }
                else
                {
                    pattern = hook::pattern("E8 ? ? ? ? 84 C0 5F 0F 85");
                    CTimer::hbIsGamePaused.fun = injector::MakeCALL(pattern.get_first(0), CTimer::IsGamePaused_2).get();
                }

                pattern = hook::pattern("0A 05 ? ? ? ? 0A 05 ? ? ? ? 74 ? 8B 0D");
                if (!pattern.empty())
                {
                    injector::MakeNOP(pattern.get_first(0), 6, true);
                    static auto TimeCycle__UpdateFinalize_Hook = safetyhook::create_mid(pattern.get_first(0), [](SafetyHookContext& regs)
                    {
                        *(uint8_t*)&regs.eax |= *CTimer::ms_bUserPause;

                        if (nTimecycleUnpauseTimer > 0)
                        {
                            *(uint8_t*)&regs.eax = 0;

                            nTimecycleUnpauseTimer--;
                        }
                    });
                }
                else
                {
                    pattern = hook::pattern("E8 ? ? ? ? 84 C0 74 ? A1 ? ? ? ? 69 C0");
                    CTimer::hbIsGamePaused.fun = injector::MakeCALL(pattern.get_first(0), CTimer::IsGamePaused_2).get();
                }
            }

            hbGET_NUMBER_OF_INSTANCES_OF_STREAMED_SCRIPT.fun = NativeOverride::Register(Natives::NativeHashes::GET_NUMBER_OF_INSTANCES_OF_STREAMED_SCRIPT, NATIVE_GET_NUMBER_OF_INSTANCES_OF_STREAMED_SCRIPT, "E8", 30);
        }
    }
} Settings;
