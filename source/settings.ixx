module;

#include <common.hxx>
#include <shlobj.h>
#include <d3dx9.h>
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

    SafetyHookInline shGetText{};
    const wchar_t* __fastcall getText(CText* text, void* edx, const char* key)
    {
        auto hash = GetHash(key);
        if (gxtEntries.contains(hash))
            return gxtEntries[hash].c_str();

        return shGetText.fastcall<const wchar_t*>(text, edx, key);
    }

    SafetyHookInline shGetTextByKey{};
    const wchar_t* __fastcall getTextByKey(CText* text, void* edx, uint32_t hash, int a3)
    {
        if (gxtEntries.contains(hash))
            return gxtEntries[hash].c_str();

        return shGetTextByKey.fastcall<const wchar_t*>(text, edx, hash, a3);
    }

    SafetyHookInline shDoesTextLabelExist{};
    char __fastcall doesTextLabelExist(CText* text, void* edx, const char* key)
    {
        if (gxtEntries.contains(GetHash(key)))
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

        auto GetValue() { return value; }
        void SetValue(int32_t v) { value = std::clamp(v, idStart, idEnd); WriteToIni(); if (callback) callback(value); }
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
    };
    static inline std::vector<DynamicOption> dynamicOptions;
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
        if (row >= options.count || !options.data || options.data[row].display != 101 || options.data[row].preference < firstCustomID)
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
        if (!options.data || options.count == 0 || options.count > options.capacity || options.data[options.count - 1].action != 46)
            return;
        for (auto& added : dynamicOptions)
        {
            if (added.screen != screen)
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
            if (options.count >= 50)
                return;
            auto sentinel = options.data[options.count - 1];
            *AppendOption(&options) = sentinel;
            options.data[options.count - 2] = added.option;
        }
    }

    static int32_t __cdecl FillMenu(int32_t menu)
    {
        InjectMenu(*currentScreen);
        if (legacyExecutable)
        {
            fillMenu.ccall<void>(menu);
            return 0;
        }
        return fillMenu.ccall<int32_t>(menu);
    }

    static int32_t FindButton(int32_t row)
    {
        if (*currentScreen < 0 || *currentScreen >= 73 || row < 0)
            return -1;
        auto& options = screens[*currentScreen].options;
        if (row >= options.count || !options.data || options.data[row].action != 127)
            return -1;
        auto index = options.data[row].preference;
        if (index < 0 || static_cast<size_t>(index) >= dynamicOptions.size() || !dynamicOptions[index].selected ||
            dynamicOptions[index].screen != *currentScreen)
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
                auto callback = dynamicOptions[button].selected;
                callback();
                FillMenu(menu);
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
        auto previous = std::exchange(selectedButton, -1);
        auto previousMenu = std::exchange(inputMenu, menu);
        auto result = processMenu.ccall<uint8_t>(menu);
        selectedButton = previous;
        inputMenu = previousMenu;
        return result;
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
            { 0, "PREF_GRAPHICSAPI",            "MAIN",       "GraphicsAPI",                        "MENU_DISPLAY_GRAPHICS_API",                           0, nullptr, 0, 1 },
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

        CIniReader d3d9cfg(d3d9cfgPath);
        auto api = d3d9cfg.ReadInteger("MAIN", "API", 0);
        FusionFixSettings.Set("PREF_GRAPHICSAPI", api);
        InitializeMenuAPI();
    }
public:
    enum class MenuScreen : int32_t
    {
        Game = 0, Controls = 5, Audio = 7, Display = 8, Graphics = 49,
        TitleControls = 59, TitleAudio = 60, TitleDisplay = 61, TitleGraphics = 62,
        KeyboardOptions = 71, ControllerOptions = 72
    };

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

private:
    static bool AddOption(MenuScreen screen, std::string_view label, int32_t preference, int32_t display, std::function<void()> callback = {})
    {
        switch (screen)
        {
        case MenuScreen::Game: case MenuScreen::Controls: case MenuScreen::Audio: case MenuScreen::Display:
        case MenuScreen::Graphics: case MenuScreen::TitleControls: case MenuScreen::TitleAudio:
        case MenuScreen::TitleDisplay: case MenuScreen::TitleGraphics: case MenuScreen::KeyboardOptions:
        case MenuScreen::ControllerOptions: break;
        default: return false;
        }
        if (label.empty() || label.size() >= sizeof(SettingsTables::Option::label) || label.find('\0') != label.npos)
            return false;
        for (auto& added : dynamicOptions)
            if (added.screen == static_cast<int32_t>(screen) && label == added.option.label)
                return false;
        auto& options = screens[static_cast<int32_t>(screen)].options;
        size_t pending = 1;
        for (auto& added : dynamicOptions)
            if (added.screen == static_cast<int32_t>(screen))
            {
                bool present = false;
                for (size_t i = 0; i < options.count; ++i)
                    present |= std::strcmp(options.data[i].label, added.option.label) == 0;
                pending += !present;
            }
        if (options.count + pending > 50)
            return false;
        SettingsTables::Option option;
        option.action = callback ? 127 : 1; // MENUOPT_ADJUST
        std::memcpy(option.label, label.data(), label.size());
        option.preference = static_cast<int16_t>(preference);
        option.scaler = callback ? 0 : static_cast<uint8_t>(SettingsTables::scalers[preference]);
        option.display = static_cast<uint8_t>(display);
        dynamicOptions.push_back({ static_cast<int32_t>(screen), option, std::move(callback) });
        return true;
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
        enum eAntialiasingText { eMO_OFF, eFXAA, eSMAA };
        const std::vector<const char*> data = { "MO_OFF", "FXAA", "SMAA" };
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
                            if (GetModuleHandleW(L"winevulkan.dll") || GetModuleHandleW(L"vulkan-1.dll"))
                                FusionFixSettings.Set(id, 1);
                            else
                                FusionFixSettings.Set(id, 0);
                        }
                        else
                        {
                            FreeLibrary(vulkan);
                            CIniReader d3d9cfg(CSettings::d3d9cfgPath);
                            d3d9cfg.WriteInteger("MAIN", "API", value, true);
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

            // Same but for Game tab
            static auto shouldModifyMenuBackground = [](int curMenuTab = *pMenuTab) -> bool
            {
                auto selectedItem = CMenu::getSelectedItem();
                return (curMenuTab == 8) ||  // Everything in Display Tab
                    (curMenuTab == 0 && selectedItem == 18) ||  // PREF_EXTRANIGHTSHADOWS in Game Tab
                    (curMenuTab == 5 && selectedItem == 8) ||  // PREF_CENTEREDCAMERA in Controls Tab
                    (curMenuTab == 5 && selectedItem == 9);     // PREF_CENTEREDCAMERAFOOT in Controls Tab
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
                    if (*pMenuTab == 8 && !shouldModifyMenuBackground())
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
            static bool bExtendedTimecycEditing = iniReader.ReadInteger("FOG", "ExtendedTimecycEditing", 0) != 0;

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
                if (pMenuTab && *pMenuTab == 8 || *pMenuTab == 49 || (*pMenuTab == 0 && CMenu::getSelectedItem() == 13) || fpsc->get())
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
                static bool bOnce = false;

                if (!bOnce)
                {
                    bOnce = true;
                    auto api = FusionFixSettings.GetRef("PREF_GRAPHICSAPI")->get();
                    if (api && !GetModuleHandleW(L"winevulkan.dll") && !GetModuleHandleW(L"vulkan-1.dll"))
                        FusionFixSettings.Set("PREF_GRAPHICSAPI", 0);
                    else if (!api && (GetModuleHandleW(L"winevulkan.dll") || GetModuleHandleW(L"vulkan-1.dll")))
                        FusionFixSettings.Set("PREF_GRAPHICSAPI", 1);
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
