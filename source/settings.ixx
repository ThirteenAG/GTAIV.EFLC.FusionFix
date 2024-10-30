module;

#include <common.hxx>
#include <shlobj.h>
#include <filesystem>
#include <d3dx9.h>

export module settings;

import common;
import comvars;
import d3dx9_43;
import fusiondxhook;
import gxtloader;

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

    void Hook()
    {
        auto pattern = find_pattern("B9 ? ? ? ? E8 ? ? ? ? 50 8D 84 24 ? ? ? ? 50 E8 ? ? ? ? 83 C4 0C", "B9 ? ? ? ? E8 ? ? ? ? 50 8D 84 24 ? ? ? ? 68 ? ? ? ? 50 BA ? ? ? ? E8 ? ? ? ? 83 C4 14");
        g_text = *pattern.get_first<CText*>(1);

        pattern = find_pattern("E8 ? ? ? ? 50 68 ? ? ? ? 8D 84 24 ? ? ? ? 68 ? ? ? ? 50 E8 ? ? ? ? 83 C4 18", "E8 ? ? ? ? 50 8D 84 24 ? ? ? ? 68 ? ? ? ? 50 BA ? ? ? ? E8 ? ? ? ? 83 C4 14");
        Get = (const wchar_t* (__fastcall*)(void*, void*, const char*))injector::GetBranchDestination(pattern.get_first(0)).as_int();
        //shGetText = safetyhook::create_inline(injector::GetBranchDestination(pattern.get_first()).as_int(), getText);

        pattern = find_pattern("E8 ? ? ? ? 50 8D 86 ? ? ? ? 50 E8 ? ? ? ? 83 C4 0C EB 27", "E8 ? ? ? ? 50 8D 8F ? ? ? ? 51");
        shGetTextByKey = safetyhook::create_inline(injector::GetBranchDestination(pattern.get_first()).as_int(), getTextByKey);

        pattern = find_pattern("51 8B 44 24 08 53 8B D9 C6 44 24", "51 8B 44 24 08 85 C0 53 8B D9");
        shDoesTextLabelExist = safetyhook::create_inline(pattern.get_first(), doesTextLabelExist);
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
        auto SetValue(auto v) { value = v; WriteToIni(); if (callback) callback(value); }
        auto ReadFromIni(auto& iniReader) { return iniReader.ReadInteger(iniSec, iniName, iniDefValInt); }
        auto ReadFromIni() { CIniReader iniReader(cfgPath); return ReadFromIni(iniReader); }
        void WriteToIni(auto& iniWriter) { iniWriter.WriteInteger(iniSec, iniName, value, true); }
        void WriteToIni() { CIniReader iniWriter(cfgPath); iniWriter.WriteInteger(iniSec, iniName, value, true); }
    };

    struct MenuPrefs
    {
        uint32_t prefID;
        char* name;
    };

    static inline std::filesystem::path cfgPath;
    static inline std::vector<MenuPrefs> aMenuPrefs;
    static inline auto firstCustomID = 0;
    static inline std::unordered_map<int32_t, std::pair<std::string, std::string>> slidersList;
private:
    static inline int32_t* mPrefs = nullptr;
    static inline std::unordered_map<uint32_t, CSetting> mFusionPrefs;

    std::optional<std::string> GetPrefNameByID(auto prefID) {
        auto it = std::find_if(std::begin(aMenuPrefs), std::end(aMenuPrefs), [&prefID](auto& it) {
            return it.prefID == prefID;
        });
        if (it != std::end(aMenuPrefs))
            return std::string(it->name);
        return std::nullopt;
    }
    std::optional<int32_t> GetPrefIDByName(auto prefName) {
        auto it = std::find_if(std::begin(aMenuPrefs), std::end(aMenuPrefs), [&prefName](auto& it) {
            return std::string_view(it.name) == prefName;
        });
        if (it != std::end(aMenuPrefs))
            return it->prefID;
        return std::nullopt;
    }
public:
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

        MenuPrefs* originalPrefs = nullptr;
        MenuPrefs** ppOriginalPrefs = nullptr;

        auto pattern = hook::pattern("8B 04 FD ? ? ? ? 5F 5E C3");
        if (pattern.size() == 4)
        {
            ppOriginalPrefs = pattern.count(4).get(3).get<MenuPrefs*>(3);
            originalPrefs = *ppOriginalPrefs;
        }
        else
        {
            pattern = hook::pattern("8B 04 F5 ? ? ? ? 5E C3 8B 04 F5 ? ? ? ? 5E C3 8B 04 F5 ? ? ? ? 5E C3 8B 04 F5");
            ppOriginalPrefs = pattern.get_first<MenuPrefs*>(3);
            originalPrefs = *ppOriginalPrefs;
        }
        
        auto pOriginalPrefsNum = find_pattern("81 FF ? ? ? ? 7C DF", "81 FE ? ? ? ? 7C E0 33 F6").get_first<uint32_t>(2);

        for (auto i = 0; originalPrefs[i].prefID < *pOriginalPrefsNum; i++)
        {
            aMenuPrefs.emplace_back(originalPrefs[i].prefID, originalPrefs[i].name);
        }

        aMenuPrefs.reserve(aMenuPrefs.size() * 2);
        firstCustomID = aMenuPrefs.back().prefID + 1;

        injector::WriteMemory(ppOriginalPrefs, &aMenuPrefs[0].prefID, true);
        injector::WriteMemory(find_pattern("FF 34 FD ? ? ? ? 56 E8 ? ? ? ? 83 C4 08 85 C0 0F 84 ? ? ? ? 47 81 FF", "8B 04 F5 ? ? ? ? 50 57 E8 ? ? ? ? 83 C4 08 85 C0 74 7C").get_first(3), &aMenuPrefs[0].name, true);

        pattern = find_pattern("89 1C 95 ? ? ? ? E8 ? ? ? ? A1 ? ? ? ? 83 C4 04 8D 04 40", "89 1C 8D ? ? ? ? E8 ? ? ? ? A1 ? ? ? ? 8D 0C 40 8B 14 CD");
        mPrefs = *pattern.get_first<int32_t*>(3);

        CIniReader iniReader(cfgPath);

        // VOLATILE! DO NOT CHANGE THE ORDER OF THESE! ONLY WORKS BY SOME MIRACLE.
        static CSetting arr[] = {
            { 0, "PREF_SKIP_INTRO",        "MAIN",       "SkipIntro",                       "",                           1, nullptr, 0, 1 },
            { 0, "PREF_SKIP_MENU",         "MAIN",       "SkipMenu",                        "",                           1, nullptr, 0, 1 },
            { 0, "PREF_BORDERLESS",        "MAIN",       "BorderlessWindowed",              "",                           1, nullptr, 0, 1 },
            { 0, "PREF_FPS_LIMIT_PRESET",  "FRAMELIMIT", "FpsLimitPreset",                  "MENU_DISPLAY_FRAMELIMIT",    0, nullptr, FpsCaps.eOFF, std::distance(std::begin(FpsCaps.data), std::end(FpsCaps.data)) - 1 },
            { 0, "PREF_BLOOM",             "MAIN",       "Bloom",                           "",                           1, nullptr, 0, 1 },
            { 0, "PREF_CONSOLE_GAMMA",     "MISC",       "ConsoleGamma",                    "",                           0, nullptr, 0, 1 },
            { 0, "PREF_TIMECYC",           "MISC",       "ScreenFilter",                    "MENU_DISPLAY_TIMECYC",       5, nullptr, TimecycText.eMO_DEF, std::distance(std::begin(TimecycText.data), std::end(TimecycText.data)) - 1 },
            { 0, "PREF_WINDOWED",          "MAIN",       "Windowed",                        "",                           0, nullptr, 0, 1 },
            { 0, "PREF_DEFINITION",        "MAIN",       "Definition",                      "",                           1, nullptr, 0, 1 },
            { 0, "PREF_SHADOWFILTER",      "SHADOWS",    "ShadowFilter",                    "MENU_DISPLAY_SHADOWFILTER",  5, nullptr, ShadowFilterText.eSoft, std::distance(std::begin(ShadowFilterText.data), std::end(ShadowFilterText.data)) - 1 },
            { 0, "PREF_TREE_LIGHTING",     "MISC",       "TreeLighting",                    "MENU_DISPLAY_TREE_LIGHTING", 7, nullptr, TreeFxText.ePC, std::distance(std::begin(TreeFxText.data), std::end(TreeFxText.data)) - 1 },
            { 0, "PREF_TCYC_DOF",          "MISC",       "DepthOfField",                    "MENU_DISPLAY_DOF",           7, nullptr, DofText.eOff, std::distance(std::begin(DofText.data), std::end(DofText.data)) - 1 },
            { 0, "PREF_MOTIONBLUR",        "MAIN",       "MotionBlur",                      "",                           0, nullptr, 0, 1 },
            { 0, "PREF_LEDILLUMINATION",   "MISC",       "LightSyncRGB",                    "",                           0, nullptr, 0, 1 },
            { 0, "PREF_TREEALPHA",         "MISC",       "TreeAlpha",                       "MENU_DISPLAY_TREEALPHA",     4, nullptr, TreeAlphaText.ePC, std::distance(std::begin(TreeAlphaText.data), std::end(TreeAlphaText.data)) - 1 },
            { 0, "PREF_SUNSHAFTS",         "MISC",       "SunShafts",                       "",                           1, nullptr, 0, 1 },
            { 0, "PREF_FPSCOUNTER",        "FRAMELIMIT", "DisplayFpsCounter",               "",                           0, nullptr, 0, 1 },
            { 0, "PREF_ALWAYSRUN",         "MISC",       "AlwaysRun",                       "",                           0, nullptr, 0, 1 },
            { 0, "PREF_ALTDIALOGUE",       "MISC",       "AltDialogue",                     "",                           0, nullptr, 0, 1 },
            { 0, "PREF_COVERCENTERING",    "MISC",       "CameraCenteringInCover",          "",                           0, nullptr, 0, 1 },
            { 0, "PREF_KBCAMCENTERDELAY",  "MISC",       "DelayBeforeCenteringCameraKB",    "",                           0, nullptr, 0, 9 },
            { 0, "PREF_PADCAMCENTERDELAY", "MISC",       "DelayBeforeCenteringCameraPad",   "",                           0, nullptr, 0, 9 },
            { 0, "PREF_CUSTOMFOV",         "MISC",       "FieldOfView",                     "",                           0, nullptr, 0, 9 },
            { 0, "PREF_RAWINPUT",          "MISC",       "RawInput",                        "",                           1, nullptr, 0, 1 },
            { 0, "PREF_BUTTONS",           "MISC",       "Buttons",                         "MENU_DISPLAY_BUTTONS",       6, nullptr, ButtonsText.eXbox360, std::distance(std::begin(ButtonsText.data), std::end(ButtonsText.data)) - 1 },
            { 0, "PREF_LETTERBOX",         "MISC",       "Letterbox",                       "",                           1, nullptr, 0, 1 },
            { 0, "PREF_PILLARBOX",         "MISC",       "Pillarbox",                       "",                           1, nullptr, 0, 1 },
            { 0, "PREF_ANTIALIASING",      "MISC",       "Antialiasing",                    "MENU_DISPLAY_ANTIALIASING",  1, nullptr, AntialiasingText.eMO_OFF, std::distance(std::begin(AntialiasingText.data), std::end(AntialiasingText.data)) - 1 },
            { 0, "PREF_UPDATE",            "UPDATE",     "CheckForUpdates",                 "",                           0, nullptr, 0, 1 },
            { 0, "PREF_BLOCKONLOSTFOCUS",  "MAIN",       "BlockOnLostFocus",                "",                           0, nullptr, 0, 1 },
            { 0, "PREF_LAMPPOSTSHADOWS",   "SHADOWS",    "LamppostShadows",                 "",                           0, nullptr, 0, 1 },
            { 0, "PREF_HEADLIGHTSHADOWS",  "SHADOWS",    "HeadlightShadows",                "",                           0, nullptr, 0, 1 },
            // Enums are at capacity, to use more enums, replace multiplayer ones. On/Off toggles should still be possible to add.
        };

        auto i = firstCustomID;
        for (auto& it : arr)
        {
            mFusionPrefs[i] = it;
            mFusionPrefs[i].value = std::clamp(it.ReadFromIni(iniReader), it.idStart, it.idEnd);
            aMenuPrefs.emplace_back(i, mFusionPrefs[i].prefName.data());
            i++;
        }

        injector::WriteMemory(pOriginalPrefsNum, aMenuPrefs.size(), true);

        // MENU_DISPLAY enums hook
        CSettings::MenuPrefs* originalEnums = nullptr;
        static std::vector<MenuPrefs> aMenuEnums;
        pattern = hook::pattern("8B 04 FD ? ? ? ? 5F 5E C3");
        if (pattern.size() == 4)
        {
            ppOriginalPrefs = pattern.count(4).get(1).get<MenuPrefs*>(3);
            originalEnums = *ppOriginalPrefs;
        }
        else
        {
            pattern = hook::pattern("8B 04 F5 ? ? ? ? 5E C3 8B 04 F5");
            ppOriginalPrefs = pattern.count(4).get(2).get<MenuPrefs*>(3);
            originalEnums = *ppOriginalPrefs;
            
        }

        auto pOriginalEnumsNum = find_pattern("83 FF 3C 7C E2", "83 FE 3C 7C E3 33 F6").get_first<uint8_t>(2);
        auto pOriginalEnumsNum2 = find_pattern("83 FE 3C 7C E6", "83 FE 3C 7C E3 33 C0").get_first<uint8_t>(2);

        for (auto i = 0; originalEnums[i].prefID < *pOriginalEnumsNum; i++)
        {
            aMenuEnums.emplace_back(originalEnums[i].prefID, originalEnums[i].name);
        }
        aMenuEnums.reserve(aMenuEnums.size() * 2);
        auto firstEnumCustomID = aMenuEnums.back().prefID + 1;

        for (auto& it : arr)
        {
            if (!it.strEnum.empty()) {
                aMenuEnums.emplace_back(firstEnumCustomID, it.strEnum.data());
                firstEnumCustomID += 1;
            }
        }

        injector::WriteMemory(ppOriginalPrefs, &aMenuEnums[0].prefID, true);
        injector::WriteMemory(find_pattern("FF 34 FD ? ? ? ? 56 E8 ? ? ? ? 83 C4 08 85 C0 0F 84 ? ? ? ? 47 83 FF 3C", "8B 14 F5 ? ? ? ? 52").get_first(3), &aMenuEnums[0].name, true);
        pattern = find_pattern("33 C0 5E C3 8B 04 F5 ? ? ? ? 5F 5E C3 8B 04 F5", "33 C0 5E C3 8B 04 F5 ? ? ? ? 5E C3 8B 04 F5");
        injector::WriteMemory(pattern.get_first(7), &aMenuEnums[0].prefID, true);
        pattern = find_pattern("FF 34 F5 ? ? ? ? 57 E8 ? ? ? ? 83 C4 08 85 C0 74 0B 46 83 FE 3C", "8B 0C F5 ? ? ? ? 51 57 E8 ? ? ? ? 83 C4 08 85 C0 74 15");
        injector::WriteMemory(pattern.get_first(3), &aMenuEnums[0].name, true);

        injector::WriteMemory<uint8_t>(pOriginalEnumsNum, uint8_t(aMenuEnums.size()), true);
        injector::WriteMemory<uint8_t>(pOriginalEnumsNum2, uint8_t(aMenuEnums.size()), true);

        // Sliders
        static std::vector<std::pair<std::string_view, std::string_view>> matchingSettingsList =
        {
            { "PREF_EPISODIC_RACECLASS_RACE_3", "PREF_CUSTOMFOV" },
            { "PREF_EPISODIC_RACECLASS_RACE_4", "PREF_KBCAMCENTERDELAY" },
            { "PREF_EPISODIC_RACECLASS_RACE_5", "PREF_PADCAMCENTERDELAY" },
        };

        for (auto& it : matchingSettingsList)
        {
            slidersList.emplace(*GetPrefIDByName(it.first), std::make_pair(it.first, it.second));
        }

        pattern = find_pattern("C6 05 ? ? ? ? ? 5E 74 1D", "C6 05 ? ? ? ? ? 74 1B 38 1D", "C6 05 ? ? ? ? ? 5E 74 1B");
        static auto slidersHook = safetyhook::create_mid(pattern.get_first(), [](SafetyHookContext& regs)
        {
            for (auto& it : matchingSettingsList)
            {
                FusionFixSettings.Set(it.first, FusionFixSettings.Get(it.second));
            }
        });

        pattern = find_pattern("3D ? ? ? ? 7C DF 83 EC 10", "3D ? ? ? ? 7C E1 B8");
        injector::WriteMemory(pattern.get_first(1), 136 - slidersList.size(), true);
        pattern = find_pattern("3D ? ? ? ? 7E E6 80 3D", "3D ? ? ? ? 7E E8 83 CF FF");
        injector::WriteMemory(pattern.get_first(1), 136 - slidersList.size(), true);

        {
            static std::vector<MenuPrefs> aMenuPrefs2(aMenuPrefs.size());

            hook::pattern patterns[] = {
                hook::pattern("89 14 8D ? ? ? ? 66 8B 0D"), hook::pattern("89 3C AD ? ? ? ? 0F B7 3A 83 C6 01"),
                hook::pattern("89 04 8D ? ? ? ? 8B 74 24 10"), hook::pattern("8B 04 85 ? ? ? ? 85 C0 7F 05")
            };

            for (auto& pattern : patterns)
                if (!pattern.empty())
                    injector::WriteMemory(pattern.get_first(3), &aMenuPrefs2[0].prefID, true);
        }
    }
public:
    int32_t Get(int32_t prefID)
    {
        if (prefID >= firstCustomID)
            return mFusionPrefs[prefID].GetValue();
        else
        {
            if (!mPrefs)
                return 0;
            DWORD tmp;
            injector::UnprotectMemory(&mPrefs[prefID], sizeof(int32_t), tmp);
            return mPrefs[prefID];
        }
    }
    auto Set(int32_t prefID, int32_t value) {
        if (prefID >= firstCustomID) {
            mFusionPrefs[prefID].SetValue(value);
        }
        else
        {
            if (!mPrefs)
                return;
            DWORD tmp;
            injector::UnprotectMemory(&mPrefs[prefID], sizeof(int32_t), tmp);
            mPrefs[prefID] = value;

            if (slidersList.contains(prefID)) {
                auto id = GetPrefIDByName(slidersList[prefID].second);
                if (id) mFusionPrefs[*id].SetValue(value);
            }
        }
    }
    int32_t Get(std::string_view name)
    {
        auto prefID = GetPrefIDByName(name);
        if (prefID) { return Get(*prefID); }
        return 0;
    }
    auto Set(std::string_view name, int32_t value) {
        auto prefID = GetPrefIDByName(name);
        if (prefID) return Set(*prefID, value);
    }
    auto isSame(int32_t id, std::string_view name) {
        auto prefID = GetPrefIDByName(name);
        if (prefID && *prefID == id)
            return true;
        return false;
    }
    std::optional<std::reference_wrapper<int32_t>> GetRef(std::string_view name)
    {
        auto prefID = GetPrefIDByName(name);
        if (prefID) {
            if (prefID >= firstCustomID)
                return std::ref(mFusionPrefs[*prefID].value);
            else {
                if (!mPrefs) {
                    MessageBoxW(0, L"Can't GetRef of original PREF", 0, 0);
                    return std::nullopt;
                }
                DWORD tmp;
                injector::UnprotectMemory(&mPrefs[*prefID], sizeof(int32_t), tmp);
                return std::ref(mPrefs[*prefID]);
            }
        }
        return std::nullopt;
    }
    void SetCallback(std::string_view name, std::function<void(int32_t)>&& cb)
    {
        auto prefID = GetPrefIDByName(name);
        if (prefID) mFusionPrefs[*prefID].callback = cb;
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

public:
    struct
    {
        enum eFpsCaps { eTOGGLE, eHOLD, eOFF, eCustom, e30 = 30, e40 = 40, e50 = 50, e60 = 60, e75 = 75, e100 = 100, e120 = 120, e144 = 144, e165 = 165, e240 = 240, e360 = 360 };
        std::vector<int32_t> data = { eTOGGLE, eHOLD, eOFF, eCustom, e30, e40, e50, e60, e75, e100, e120, e144, e165, e240, e360 };
    } FpsCaps;

    struct
    {
        enum eTimecycText { eLow, eMedium, eHigh, eVeryHigh, eHighest, eMO_DEF, eOFF, eIV, eTLAD, eTBOGT };
        std::vector<const char*> data = { "Low", "Medium", "High", "Very High", "Highest", "MO_DEF", "OFF", "IV", "TLAD", "TBOGT" };
    } TimecycText;

    struct
    {
        enum eShadowFilterText {
            eRadio, eSequential, eShuffle, eSoft, eSharp, eCHSS
        };
        std::vector<const char*> data = { "Radio", "Sequential", "Shuffle", "Soft", "Sharp", "CHSS",};
    } ShadowFilterText;

    struct
    {
        enum eDofText {
            eAuto, e43, e54, e159, e169, eOff, eCutscenesOnly, eLow, eMedium, eHigh, eVeryHigh
        };
        std::vector<const char*> data = { "Auto", "4:3", "5:4", "15:9", "16:9", "Off", "Cutscenes Only", "Low", "Medium", "High", "Very High"};
    } DofText;

    struct
    {
        enum eTreeFxText {
            eAuto, e43, e54, e159, e169, e1610, ePC, ePCPlus, eConsole
        };
        std::vector<const char*> data = { "Auto", "4:3", "5:4", "15:9", "16:9", "16:10", "PC", "PC+", "Console" };
    } TreeFxText;

    struct
    {
        enum eTreeAlphaText { eLow, eMedium, eHigh, eVeryHigh, ePC, eConsole };
        std::vector<const char*> data = { "Low", "Medium", "High", "Very High", "PC", "Console" };
    } TreeAlphaText;

    struct
    {
        enum eButtonsText { eOff, eLow, eMedium, eHigh, eVeryHigh, eXbox360, eXboxOne, ePlaystation3, ePlaystation4, ePlaystation5, eNintendoSwitch, eSteamDeck, eSteamController };
        std::vector<const char*> data = { "Off", "Low", "Medium", "High", "Very High", "Xbox 360", "Xbox One", "Playstation 3", "Playstation 4", "Playstation 5", "Nintendo Switch", "Steam Deck", "Steam Controller" };
    } ButtonsText;

    struct
    {
        enum eAntialiasingText { eLow, eMedium, eHigh, eVeryHigh, eHighest, eMO_OFF, eFXAA, eSMAA };
        std::vector<const char*> data = { "Low", "Medium", "High", "Very High", "Highest", "MO_OFF", "FXAA", "SMAA" };
    } AntialiasingText;

} FusionFixSettings;

class Settings
{
public:
    Settings()
    {
        FusionFix::onInitEventAsync() += []()
        {
            // runtime settings
            auto pattern = hook::pattern("89 1C ? ? ? ? ? E8 ? ? ? ? A1");
            static auto reg = *pattern.get_first<uint8_t>(2);
            struct IniWriter
            {
                void operator()(injector::reg_pack& regs)
                {
                    auto id = regs.edx;
                    auto value = regs.ebx;

                    if (reg == 0x8D) {
                        id = regs.ecx;
                        value = regs.ebx;
                    }

                    auto old = FusionFixSettings(id);

                    FusionFixSettings.ForEachPref([&](int32_t prefID, int32_t idStart, int32_t idEnd) {
                        if (prefID == id) {
                            if (int32_t(value) <= idStart) {
                                if (old > idStart)
                                    value = idStart;
                                else
                                    value = idEnd;
                            }
                        }
                    });

                    FusionFixSettings.Set(id, value);
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

                    if (reg4 == 0xBD) {
                        id = regs.edi;
                        value = regs.edx;
                    }

                    auto old = FusionFixSettings(id);

                    FusionFixSettings.ForEachPref([&](int32_t prefID, int32_t idStart, int32_t idEnd) {
                        if (prefID == id) {
                            if (int32_t(value) <= idStart) {
                                if (old > idStart)
                                    value = idStart;
                                else
                                    value = idEnd;
                            }
                        }
                    });

                    FusionFixSettings.Set(id, value);
                }
            }; injector::MakeInline<IniWriterMouse>(pattern.get_first(0), pattern.get_first(7));

            pattern = find_pattern("8B 1C 95 ? ? ? ? 89 54 24 14", "8B 1C 8D ? ? ? ? 89 4C 24 18");
            static auto reg2 = *pattern.get_first<uint8_t>(2);
            struct MenuTogglesHook1
            {
                void operator()(injector::reg_pack& regs)
                {
                    if (reg2 == 0x8D) {
                        regs.ebx = FusionFixSettings.Get(regs.ecx);
                        return;
                    }
                    regs.ebx = FusionFixSettings.Get(regs.edx);
                }
            }; injector::MakeInline<MenuTogglesHook1>(pattern.get_first(0), pattern.get_first(7));

            pattern = find_pattern("8B 14 85 ? ? ? ? 66 83 F9 31", "8B 0C 8D ? ? ? ? 75 12");
            static auto reg3 = *pattern.get_first<uint8_t>(2);
            struct MenuTogglesHook2
            {
                void operator()(injector::reg_pack& regs)
                {
                    if (reg3 == 0x8D) {
                        regs.ecx = FusionFixSettings.Get(regs.ecx);
                        return;
                    }
                    regs.edx = FusionFixSettings.Get(regs.eax);
                }
            }; injector::MakeInline<MenuTogglesHook2>(pattern.get_first(0), pattern.get_first(7));

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
        };

        // FPS Counter
        if (GetD3DX9_43DLL())
        {
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
                if (pMenuTab && *pMenuTab == 8 || *pMenuTab == 49 || fpsc->get())
                {
                    static std::list<int> m_times;
            
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
            
                        D3DXFONT_DESC fps_font;
                        ZeroMemory(&fps_font, sizeof(D3DXFONT_DESC));
                        fps_font.Height = rect.bottom / 20;
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
                        static const D3DXCOLOR IV(D3DCOLOR_XRGB(0xF0, 0xA0, 0x00));
                        DrawTextOutline(pFPSFont, 10, 10, (curEp == 2) ? TBOGT : ((curEp == 1) ? TLAD : IV), str_format_fps, fps);
                    }
                }
            };
            
            FusionFix::onShutdownEvent() += []()
            {
                if (pFPSFont)
                    pFPSFont->Release();
                pFPSFont = nullptr;
            };
        }
    }
} Settings;