#include <misc.h>
#include <mmsystem.h>
#pragma comment(lib, "winmm.lib") // needed for timeBeginPeriod()/timeEndPeriod()
#include <filesystem>
#include <map>
#include <d3d9.h>
#include <bitset>

class CSettings
{
private:
    struct CSetting
    {
        int32_t value = 0;
        std::string prefName;
        std::string iniSec;
        std::string iniName;
        int32_t iniDefValInt = 0;
        std::function<void(int32_t value)> callback;

        auto GetValue() { return value; }
        auto SetValue(auto v) { value = v; WriteToIni(); if (callback) callback(value); }
        auto ReadFromIni(auto& iniReader) { return iniReader.ReadInteger(iniSec, iniName, iniDefValInt); }
        auto ReadFromIni() { CIniReader iniReader(""); return ReadFromIni(iniReader); }
        void WriteToIni(auto& iniWriter) { iniWriter.WriteInteger(iniSec, iniName, value); }
        void WriteToIni() { CIniReader iniWriter(""); iniWriter.WriteInteger(iniSec, iniName, value); }
    };

    struct MenuPrefs
    {
        uint32_t prefID;
        char* name;
    };

    static inline std::vector<MenuPrefs> aMenuPrefs;
    static inline auto firstCustomID = 0;
private:
    static inline int32_t* mPrefs;
    static inline std::map<uint32_t, CSetting> mFusionPrefs;

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
        auto pattern = hook::pattern("8B 04 FD ? ? ? ? 5F 5E C3");
        auto pattern2 = hook::pattern("FF 34 FD ? ? ? ? 56 E8 ? ? ? ? 83 C4 08 85 C0 0F 84 ? ? ? ? 47 81 FF");
        auto originalPrefs = *pattern.count(4).get(3).get<MenuPrefs*>(3);
        auto pOriginalPrefsNum = pattern2.get_first<uint32_t>(27);

        for (auto i = 0; originalPrefs[i].prefID < *pOriginalPrefsNum; i++)
        {
            aMenuPrefs.emplace_back(originalPrefs[i].prefID, originalPrefs[i].name);
        }
        
        aMenuPrefs.reserve(aMenuPrefs.size() * 2);
        firstCustomID = aMenuPrefs.back().prefID + 1;

        injector::WriteMemory(pattern.count(4).get(3).get<void*>(3), &aMenuPrefs[0].prefID, true);
        injector::WriteMemory(pattern2.get_first(3), &aMenuPrefs[0].name, true);

        pattern = hook::pattern("89 1C 95 ? ? ? ? E8 ? ? ? ? A1 ? ? ? ? 83 C4 04");
        mPrefs = *pattern.get_first<int32_t*>(3);

        CIniReader iniReader("");

        CSetting arr[] = {
            { 0, "PREF_SKIP_INTRO",       "MAIN",       "SkipIntro",          1, nullptr },
            { 0, "PREF_SKIP_MENU",        "MAIN",       "SkipMenu",           1, nullptr },
            { 0, "PREF_BORDERLESS",       "MAIN",       "BorderlessWindowed", 1, nullptr },
            { 0, "PREF_FPS_LIMIT_PRESET", "FRAMELIMIT", "FpsLimitPreset",     0, nullptr },
            { 0, "PREF_FXAA",             "MISC",       "FXAA",               1, nullptr },
            { 0, "PREF_CONSOLE_GAMMA",    "MISC",       "ConsoleGamma",       0, nullptr },
            { 0, "PREF_TIMECYC",          "MISC",       "ScreenFilter",       5, nullptr },
        };

        auto i = firstCustomID;
        for (auto& it : arr)
        {
            mFusionPrefs[i] = it;
            mFusionPrefs[i].SetValue(it.ReadFromIni(iniReader));
            aMenuPrefs.emplace_back(i, mFusionPrefs[i].prefName.data());
            i++;
        }

        injector::WriteMemory(pOriginalPrefsNum, aMenuPrefs.size(), true);

        // MENU_DISPLAY enums hook
        static std::vector<MenuPrefs> aMenuEnums;
        pattern = hook::pattern("8B 04 FD ? ? ? ? 5F 5E C3");
        pattern2 = hook::pattern("FF 34 FD ? ? ? ? 56 E8 ? ? ? ? 83 C4 08 85 C0 0F 84 ? ? ? ? 47 83 FF 3C");
        auto pattern3 = hook::pattern("83 FE 3C 7C E6");
        auto originalEnums = *pattern.count(4).get(1).get<MenuPrefs*>(3);
        auto pOriginalEnumsNum = pattern2.get_first<uint8_t>(27);
        auto pOriginalEnumsNum2 = pattern3.get_first<uint8_t>(2);

        for (auto i = 0; originalEnums[i].prefID < *pOriginalEnumsNum; i++)
        {
            aMenuEnums.emplace_back(originalEnums[i].prefID, originalEnums[i].name);
        }
        aMenuEnums.reserve(aMenuEnums.size() * 2);
        auto firstEnumCustomID = aMenuEnums.back().prefID + 1;

        injector::WriteMemory(pattern.count(4).get(1).get<void*>(3), &aMenuEnums[0].prefID, true);
        injector::WriteMemory(pattern2.get_first(3), &aMenuEnums[0].name, true);
        pattern = hook::pattern("8B 04 F5 ? ? ? ? 5F 5E C3 8B 04 F5");
        injector::WriteMemory(pattern.get_first(3), &aMenuEnums[0].prefID, true);
        pattern = hook::pattern("FF 34 F5 ? ? ? ? 57 E8 ? ? ? ? 83 C4 08 85 C0 74 0B 46 83 FE 3C");
        injector::WriteMemory(pattern.get_first(3), &aMenuEnums[0].name, true);

        static auto MENU_DISPLAY_FRAMELIMIT = "MENU_DISPLAY_FRAMELIMIT";
        static auto MENU_DISPLAY_TIMECYC = "MENU_DISPLAY_TIMECYC";
        static auto eMENU_DISPLAY_ON_OFF = 0;

        aMenuEnums.emplace_back(eMENU_DISPLAY_ON_OFF, (char*)MENU_DISPLAY_FRAMELIMIT);
        firstEnumCustomID += 1;
        aMenuEnums.emplace_back(firstEnumCustomID, (char*)MENU_DISPLAY_TIMECYC);
        firstEnumCustomID += 1;

        injector::WriteMemory<uint8_t>(pOriginalEnumsNum, aMenuEnums.size(), true);
        injector::WriteMemory<uint8_t>(pOriginalEnumsNum2, aMenuEnums.size(), true);
    }
public:
    int32_t Get(int32_t prefID)
    {
        if (prefID >= firstCustomID)
            return mFusionPrefs[prefID].GetValue();
        else
            return mPrefs[prefID];
    }
    auto Set(int32_t prefID, int32_t value) {
        if (prefID >= firstCustomID) {
            mFusionPrefs[prefID].SetValue(value);
        }
        else
            mPrefs[prefID] = value;
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
            else
                return std::ref(mPrefs[*prefID]);
        }
        return std::nullopt;
    }
    void SetCallback(std::string_view name, std::function<void(int32_t)>&& cb)
    {
        auto prefID = GetPrefIDByName(name);
        if (prefID) mFusionPrefs[*prefID].callback = cb;
    }
    auto operator()(int32_t i) { return Get(i); }
    auto operator()(std::string_view name) { return Get(name); }
} FusionFixSettings;

int32_t nFrameLimitType;
float fFpsLimit;
float fCutsceneFpsLimit;
float fScriptCutsceneFpsLimit;
float fScriptCutsceneFovLimit;
std::vector<int32_t> fpsCaps = { 0, 1, 2, 30, 40, 50, 60, 75, 100, 120, 144, 165, 240 };

class FrameLimiter
{
public:
    enum FPSLimitMode { FPS_NONE, FPS_REALTIME, FPS_ACCURATE };
    FPSLimitMode mFPSLimitMode = FPS_NONE;
private:
    double TIME_Frequency = 0.0;
    double TIME_Ticks = 0.0;
    double TIME_Frametime = 0.0;
    float  fFPSLimit = 0.0f;
public:
    void Init(FPSLimitMode mode, float fps_limit)
    {
        mFPSLimitMode = mode;
        fFPSLimit = fps_limit;

        LARGE_INTEGER frequency;
        QueryPerformanceFrequency(&frequency);
        static constexpr auto TICKS_PER_FRAME = 1;
        auto TICKS_PER_SECOND = (TICKS_PER_FRAME * fFPSLimit);
        if (mFPSLimitMode == FPS_ACCURATE)
        {
            TIME_Frametime = 1000.0 / (double)fFPSLimit;
            TIME_Frequency = (double)frequency.QuadPart / 1000.0; // ticks are milliseconds
        }
        else // FPS_REALTIME
        {
            TIME_Frequency = (double)frequency.QuadPart / (double)TICKS_PER_SECOND; // ticks are 1/n frames (n = fFPSLimit)
        }
        Ticks();
    }
    DWORD Sync_RT()
    {
        DWORD lastTicks, currentTicks;
        LARGE_INTEGER counter;

        QueryPerformanceCounter(&counter);
        lastTicks = (DWORD)TIME_Ticks;
        TIME_Ticks = (double)counter.QuadPart / TIME_Frequency;
        currentTicks = (DWORD)TIME_Ticks;

        return (currentTicks > lastTicks) ? currentTicks - lastTicks : 0;
    }
    DWORD Sync_SLP()
    {
        LARGE_INTEGER counter;
        QueryPerformanceCounter(&counter);
        double millis_current = (double)counter.QuadPart / TIME_Frequency;
        double millis_delta = millis_current - TIME_Ticks;
        if (TIME_Frametime <= millis_delta)
        {
            TIME_Ticks = millis_current;
            return 1;
        }
        else if (TIME_Frametime - millis_delta > 2.0) // > 2ms
            Sleep(1); // Sleep for ~1ms
        else
            Sleep(0); // yield thread's time-slice (does not actually sleep)

        return 0;
    }
    void Sync()
    {
        if (mFPSLimitMode == FPS_REALTIME)
            while (!Sync_RT());
        else if (mFPSLimitMode == FPS_ACCURATE)
            while (!Sync_SLP());
    }
private:
    void Ticks()
    {
        LARGE_INTEGER counter;
        QueryPerformanceCounter(&counter);
        TIME_Ticks = (double)counter.QuadPart / TIME_Frequency;
    }
};

FrameLimiter FpsLimiter;
FrameLimiter CutsceneFpsLimiter;
FrameLimiter ScriptCutsceneFpsLimiter;
bool(*CCutscenes__hasCutsceneFinished)();
bool(*CCamera__isWidescreenBordersActive)();
void __cdecl sub_855640()
{
    static auto preset = FusionFixSettings.GetRef("PREF_FPS_LIMIT_PRESET");

    if (preset && *preset >= 2) {
        if (fFpsLimit > 0.0f || (*preset > 2 && *preset < fpsCaps.size()))
            FpsLimiter.Sync();
    }

    if (CCamera__isWidescreenBordersActive())
    {
        if (CCutscenes__hasCutsceneFinished())
            if (fCutsceneFpsLimit)
                CutsceneFpsLimiter.Sync();
            else if (fScriptCutsceneFpsLimit)
                ScriptCutsceneFpsLimiter.Sync();
    }
}

bool bSkipIntroNotNeeded = false;
injector::hook_back<void(__cdecl*)(int)> hbsub_7870A0;
void __cdecl sub_7870A0(int a1)
{
    static bool bOnce = false;
    if (!bOnce)
    {
        bSkipIntroNotNeeded = true;
        if (a1 == 0)
        {
            bool bNoLoad = (GetAsyncKeyState(VK_SHIFT) & 0xF000) != 0;
            if (!bNoLoad && FusionFixSettings("PREF_SKIP_MENU"))
                a1 = 6;

            bOnce = true;
        }
    }
    return hbsub_7870A0.fun(a1);
}

HWND gWnd;
RECT gRect;
BOOL WINAPI SetRect_Hook(LPRECT lprc, int xLeft, int yTop, int xRight, int yBottom)
{
    gRect = { xLeft, yTop, xRight, yBottom };
    return SetRect(lprc, xLeft, yTop, xRight, yBottom);
}

BOOL WINAPI MoveWindow_Hook(HWND hWnd, int X, int Y, int nWidth, int nHeight, BOOL bRepaint)
{
    RECT rect = { X, Y, nWidth, nHeight };
    HMONITOR monitor = MonitorFromWindow(hWnd, MONITOR_DEFAULTTONEAREST);
    MONITORINFO info = {};
    info.cbSize = sizeof(MONITORINFO);
    GetMonitorInfo(monitor, &info);
    int32_t DesktopResW = info.rcMonitor.right - info.rcMonitor.left;
    int32_t DesktopResH = info.rcMonitor.bottom - info.rcMonitor.top;
    if ((rect.right - rect.left >= DesktopResW) || (rect.bottom - rect.top >= DesktopResH))
        rect = gRect;
    rect.left = (LONG)(((float)DesktopResW / 2.0f) - ((float)rect.right / 2.0f));
    rect.top = (LONG)(((float)DesktopResH / 2.0f) - ((float)rect.bottom / 2.0f));
    return MoveWindow(hWnd, rect.left, rect.top, rect.right, rect.bottom, bRepaint);
}

void SwitchWindowStyle()
{
    if (gWnd)
    {
        RECT rect = gRect;
        LONG lStyle = GetWindowLong(gWnd, GWL_STYLE);
        if (FusionFixSettings("PREF_BORDERLESS"))
        {
            lStyle &= ~(WS_CAPTION | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_SYSMENU);
        }
        else
        {
            GetWindowRect(gWnd, &rect);
            lStyle |= (WS_CAPTION | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_SYSMENU);
        }
        AdjustWindowRect(&rect, lStyle, FALSE);
        SetWindowLong(gWnd, GWL_STYLE, lStyle);
        MoveWindow_Hook(gWnd, 0, 0, rect.right - rect.left, rect.bottom - rect.top, TRUE);
    }
}

HWND WINAPI CreateWindowExA_Hook(DWORD dwExStyle, LPCSTR lpClassName, LPCSTR lpWindowName, DWORD dwStyle, int X, int Y, int nWidth, int nHeight, HWND hWndParent, HMENU hMenu, HINSTANCE hInstance, LPVOID lpParam)
{
    gWnd = CreateWindowExA(dwExStyle, lpClassName, lpWindowName, 0, X, Y, nWidth, nHeight, hWndParent, hMenu, hInstance, lpParam);
    SwitchWindowStyle();
    return gWnd;
}

BOOL WINAPI AdjustWindowRect_Hook(LPRECT lpRect, DWORD dwStyle, BOOL bMenu)
{
    if (FusionFixSettings("PREF_BORDERLESS"))
        dwStyle &= ~(WS_CAPTION | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_SYSMENU);
    else
        dwStyle |= (WS_CAPTION | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_SYSMENU);

    return AdjustWindowRect(lpRect, dwStyle, bMenu);
}

uint32_t dword_F43AD8;
uint32_t dword_1826D48;
uint32_t dword_1826D34;
uint32_t dword_1826D4C;
uint32_t dword_1826D6C;
double __cdecl MouseFix(void*, int32_t axis)
{
    auto address = *(int32_t*)(dword_F43AD8 + 0xC);
    if (*(BYTE*)(address + 0x1C4) & 8)
    {
        *(int32_t*)(dword_1826D48) = 0;
        *(int32_t*)(dword_1826D34) = 0;
        *(int32_t*)(dword_1826D4C) = 0;
        *(int32_t*)(dword_1826D6C) = 0;
    }
    if (axis == 0)
    {
        return (double)(*(int32_t*)(dword_1826D48)) * 0.0078125;
    }
    else if (axis == 1)
    {
        return (double)(*(int32_t*)(dword_1826D34)) * 0.0078125;
    }

    return 0.0;
}

template<class T>
T GetModulePath(HMODULE hModule)
{
    static constexpr auto INITIAL_BUFFER_SIZE = MAX_PATH;
    static constexpr auto MAX_ITERATIONS = 7;
    T ret;
    auto bufferSize = INITIAL_BUFFER_SIZE;
    for (size_t iterations = 0; iterations < MAX_ITERATIONS; ++iterations)
    {
        ret.resize(bufferSize);
        size_t charsReturned = 0;
        if constexpr (std::is_same_v<T, std::string>)
            charsReturned = GetModuleFileNameA(hModule, &ret[0], bufferSize);
        else
            charsReturned = GetModuleFileNameW(hModule, &ret[0], bufferSize);
        if (charsReturned < ret.length())
        {
            ret.resize(charsReturned);
            return ret;
        }
        else
        {
            bufferSize *= 2;
        }
    }
    return T();
}

template<class T>
T GetExeModulePath()
{
    T r = GetModulePath<T>(NULL);
    if constexpr (std::is_same_v<T, std::string>)
        r = r.substr(0, r.find_last_of("/\\") + 1);
    else
        r = r.substr(0, r.find_last_of(L"/\\") + 1);
    return r;
}

template <typename T, typename V>
bool iequals(const T& s1, const V& s2)
{
    T str1(s1); T str2(s2);
    std::transform(str1.begin(), str1.end(), str1.begin(), ::tolower);
    std::transform(str2.begin(), str2.end(), str2.begin(), ::tolower);
    return (str1 == str2);
}

void* fnAE3DE0 = nullptr;
void* fnAE3310 = nullptr;
int __cdecl sub_AE3DE0(int a1, int a2)
{
    injector::cstd<void(int, int, int, int, int)>::call(fnAE3310, a1, 0, 0, 0, a2);
    return injector::cstd<int(int, int)>::call(fnAE3DE0, a1, a2);
}

void* CFileMgrOpenFile = nullptr;
void* __cdecl sub_8C4CF0(char* a1, char* a2)
{
    if (FusionFixSettings("PREF_TIMECYC"))
    {
        static constexpr auto timecyc = "timecyc.dat";
        auto full_path = std::string_view(a1);
        if (full_path.ends_with(timecyc))
        {
            enum
            {
                eLow,
                eMedium,
                eHigh,
                eVeryHigh,
                eHighest,
                eMO_DEF,
                eOFF,
                eIV,
                eTLAD,
                eTBOGT,
            };
            auto path = full_path.substr(0, full_path.find(timecyc));
            switch (FusionFixSettings("PREF_TIMECYC"))
            {
            case eOFF:
            {
                auto new_path = std::string(path) + std::string("timecycOFF.dat");
                auto opened = injector::cstd<void* (const char*, const char*)>::call(CFileMgrOpenFile, new_path.c_str(), a2);
                if (opened) return opened;
            }
            break;
            case eIV:
            {
                auto new_path = std::string(path) + std::string("timecycIV.dat");
                auto opened = injector::cstd<void* (const char*, const char*)>::call(CFileMgrOpenFile, new_path.c_str(), a2);
                if (opened) return opened;
            }
            break;
            case eTLAD:
            {
                auto new_path = std::string(path) + std::string("timecycTLAD.dat");
                auto opened = injector::cstd<void* (const char*, const char*)>::call(CFileMgrOpenFile, new_path.c_str(), a2);
                if (opened) return opened;
            }
            break;
            case eTBOGT:
            {
                auto new_path = std::string(path) + std::string("timecycTBOGT.dat");
                auto opened = injector::cstd<void* (const char*, const char*)>::call(CFileMgrOpenFile, new_path.c_str(), a2);
                if (opened) return opened;
            }
            break;
            default:
                break;
            }
        }
    }
    return injector::cstd<void* (char*, char*)>::call(CFileMgrOpenFile, a1, a2);
}

int32_t bExtraDynamicShadows;
bool bDynamicShadowForTrees;
std::string curModelName;
injector::memory_pointer_raw CModelInfoStore__allocateBaseModel = nullptr;
void* __cdecl CModelInfoStore__allocateBaseModelHook(char* modelName)
{
    curModelName = modelName;
    std::transform(curModelName.begin(), curModelName.end(), curModelName.begin(), [](unsigned char c) { return std::tolower(c); });
    return injector::cstd<void* (char*)>::call(CModelInfoStore__allocateBaseModel, modelName);
}
injector::memory_pointer_raw CModelInfoStore__allocateInstanceModel = nullptr;
void* __cdecl CModelInfoStore__allocateInstanceModelHook(char* modelName)
{
    curModelName = modelName;
    std::transform(curModelName.begin(), curModelName.end(), curModelName.begin(), [](unsigned char c) { return std::tolower(c); });

    if (bDynamicShadowForTrees) {
        static std::vector<std::string> treeNames = {
            "ag_bigandbushy", "ag_bigandbushygrn", "ag_tree00", "ag_tree06", "azalea_md_ingame",
            "azalea_md_ingame_05", "azalea_md_ingame_06", "azalea_md_ingame_2", "azalea_md_ingame_3",
            "azalea_md_ingame_4", "bholly_md_ingame", "bholly_md_ingame_2", "bholly_md_s_ingame",
            "bholly_md_s_ingame_2", "beech_md_ingame_2", "c_apple_md_ingame", "c_apple_md_ingame01",
            "c_apple_md_ingame_2", "c_fern_md_ingame", "c_fern_md_ingame_2", "c_fern_md_ingame_3",
            "elm_md_ingame", "elm_md_ingame_2", "h_c_md_f_ingame", "h_c_md_f_ingame_2", "l_p_sap_ingame_2",
            "liveoak_md_ingame", "liveoak_md_ingame_2", "londonp_md_ingame", "londonp_md_ingame_2",
            "mglory_c_md_ingame", "mglory_c_md_ingame_2", "pinoak_md_ingame", "pinoak_md_ingame_2",
            "scotchpine", "tree_beech1", "tree_beech2", "w_birch_md_ingame", "w_birch_md_ingame2",
            "w_birch_md_ingame_2", "w_r_cedar_md_ingame", "w_r_cedar_md_ing_2", "scotchpine2", "scotchpine4",
            "tree_redcedar", "tree_redcedar2"
        };
        if (std::any_of(std::begin(treeNames), std::end(treeNames), [](auto& i) { return i == curModelName; }))
            return injector::cstd<void* (char*)>::call(CModelInfoStore__allocateBaseModel, modelName);
    }

    return injector::cstd<void* (char*)>::call(CModelInfoStore__allocateInstanceModel, modelName);
}

std::vector<std::string> modelNames = {"track", "fence", "rail", "pillar", "post"};
injector::memory_pointer_raw CBaseModelInfo__setFlags = nullptr;
void __cdecl CBaseModelInfo__setFlagsHook(void* pModel, int dwFlags, int a3)
{
    if (bExtraDynamicShadows)
    {
        enum
        {
            flag0, flag1, alpha, flag3, flag4, trees, flag6, instance, flag8,
            enable_bone_anim, enable_uv_animation, model_hidden_shadow_casting,
            flag12, no_shadow, flag14, flag15, flag16, dynamic, flag18, flag19,
            flag20, no_backface_cull, static_shadow_1, static_shadow_2,
            flag24, flag25, enable_specialattribute, flag27, flag28,
            flag29, flag30, flag31
        };

        auto bitFlags = std::bitset<32>(dwFlags);
        if (bitFlags.test(no_shadow))
        {
            if (bExtraDynamicShadows >= 3 || std::any_of(std::begin(modelNames), std::end(modelNames), [](auto& i) { return curModelName.contains(i); }))
            {
                bitFlags.reset(no_shadow);
                bitFlags.reset(static_shadow_1);
                bitFlags.reset(static_shadow_2);
                dwFlags = static_cast<int>(bitFlags.to_ulong());
            }
        }
    }

    return injector::cstd<void(void*, int, int)>::call(CBaseModelInfo__setFlags, pModel, dwFlags, a3);
}

void Init()
{
    CIniReader iniReader("");
    //[MAIN]
    bool bSkipIntro = FusionFixSettings("PREF_SKIP_INTRO") != 0;
    bool bSkipMenu = FusionFixSettings("PREF_SKIP_MENU") != 0;
    bool bBorderlessWindowed = FusionFixSettings("PREF_BORDERLESS") != 0;
    bool bRecoilFix = iniReader.ReadInteger("MAIN", "RecoilFix", 1) != 0;
    bool bDefinitionFix = iniReader.ReadInteger("MAIN", "DefinitionFix", 1) != 0;
    bool bEmissiveShaderFix = iniReader.ReadInteger("MAIN", "EmissiveShaderFix", 1) != 0;
    bool bHandbrakeCamFix = iniReader.ReadInteger("MAIN", "HandbrakeCamFix", 0) != 0;
    int32_t nAimingZoomFix = iniReader.ReadInteger("MAIN", "AimingZoomFix", 1);
    bool bFlickeringShadowsFix = iniReader.ReadInteger("SHADOWS", "FlickeringShadowsFix", 1) != 0;
    bExtraDynamicShadows = iniReader.ReadInteger("SHADOWS", "ExtraDynamicShadows", 1);
    bDynamicShadowForTrees = iniReader.ReadInteger("SHADOWS", "DynamicShadowForTrees", 0) != 0;

    //[FRAMELIMIT]
    nFrameLimitType = iniReader.ReadInteger("FRAMELIMIT", "FrameLimitType", 2);
    fFpsLimit = static_cast<float>(iniReader.ReadInteger("FRAMELIMIT", "FpsLimit", 0));
    fCutsceneFpsLimit = static_cast<float>(iniReader.ReadInteger("FRAMELIMIT", "CutsceneFpsLimit", 0));
    fScriptCutsceneFpsLimit = static_cast<float>(iniReader.ReadInteger("FRAMELIMIT", "ScriptCutsceneFpsLimit", 0));
    fScriptCutsceneFovLimit = static_cast<float>(iniReader.ReadInteger("FRAMELIMIT", "ScriptCutsceneFovLimit", 0));

    //[MISC]
    bool bDefaultCameraAngleInTLAD = iniReader.ReadInteger("MISC", "DefaultCameraAngleInTLAD", 0) != 0;
    bool bPedDeathAnimFixFromTBOGT = iniReader.ReadInteger("MISC", "PedDeathAnimFixFromTBOGT", 1) != 0;
    bool bDisableCameraCenteringInCover = iniReader.ReadInteger("MISC", "DisableCameraCenteringInCover", 1) != 0;
    bool bMouseFix = iniReader.ReadInteger("MISC", "MouseFix", 1) != 0;

    //[BudgetedIV]
    uint32_t nVehicleBudget = iniReader.ReadInteger("BudgetedIV", "VehicleBudget", 0);
    uint32_t nPedBudget = iniReader.ReadInteger("BudgetedIV", "PedBudget", 0);

    static float& fTimeStep = **hook::get_pattern<float*>("F3 0F 10 05 ? ? ? ? F3 0F 59 05 ? ? ? ? 8B 43 20 53", 4);

    // reverse lights fix
    {
        auto pattern = hook::pattern("8B 40 64 FF D0 F3 0F 10 40 ? 8D 44 24 40 50");
        injector::WriteMemory<uint8_t>(pattern.get_first(2), 0x60, true);
    }

    // animation fix for phone interaction on bikes
    {
        auto pattern = hook::pattern("83 3D ? ? ? ? 01 0F 8C 18 01 00 00");
        injector::MakeNOP(pattern.get(0).get<int>(0), 13, true);
    }

    //fix for lods appearing inside normal models, unless the graphics menu was opened once (draw distances aren't set properly?)
    {
        auto pattern = hook::pattern("E8 ? ? ? ? 8D 4C 24 10 F3 0F 11 05 ? ? ? ? E8 ? ? ? ? 8B F0 E8 ? ? ? ? DF 2D");
        auto sub_477300 = injector::GetBranchDestination(pattern.get_first(0));
        pattern = hook::pattern("E8 ? ? ? ? E8 ? ? ? ? E8 ? ? ? ? E8 ? ? ? ? E8 ? ? ? ? E8 ? ? ? ? 83 C4 2C C3");
        injector::MakeCALL(pattern.get_first(0), sub_477300, true);
    }

    if (bSkipIntro)
    {
        auto pattern = hook::pattern("8B 0D ? ? ? ? 8B 44 24 18 83 C4 0C 69 C9 ? ? ? ? 89 81");
        static auto dword_15A6F0C = *pattern.count(2).get(1).get<int32_t*>(2);
        struct Loadsc
        {
            void operator()(injector::reg_pack& regs)
            {
                *(int32_t*)&regs.ecx = *dword_15A6F0C;
                *(int32_t*)&regs.eax = *(int32_t*)(regs.esp + 0x18);
                if (!bSkipIntroNotNeeded && FusionFixSettings("PREF_SKIP_INTRO") && *(int32_t*)&regs.eax < 8000)
                {
                    regs.eax = 0;
                }
            }
        }; injector::MakeInline<Loadsc>(pattern.count(2).get(1).get<void*>(0), pattern.count(2).get(1).get<void*>(10));
    }

    //if (bSkipMenu)
    {
        auto pattern = hook::pattern("E8 ? ? ? ? 83 C4 04 8B 8C 24 ? ? ? ? 5F 5E 5D 5B 33 CC E8 ? ? ? ? 81 C4 ? ? ? ? C3");
        hbsub_7870A0.fun = injector::MakeCALL(pattern.count(5).get(1).get<void*>(0), sub_7870A0).get();
    }

    //if (bBorderlessWindowed)
    {
        auto pattern = hook::pattern("FF 15 ? ? ? ? 8B F0 6A 01");
        injector::MakeNOP(pattern.get_first(0), 6, true);
        injector::MakeCALL(pattern.get_first(0), CreateWindowExA_Hook, true);
        pattern = hook::pattern("FF 15 ? ? ? ? 8B 44 24 34");
        injector::MakeNOP(pattern.get_first(0), 6, true);
        injector::MakeCALL(pattern.get_first(0), MoveWindow_Hook, true);
        pattern = hook::pattern("FF 15 ? ? ? ? 8B 54 24 24 8B 74 24 20");
        injector::MakeNOP(pattern.get_first(0), 6, true);
        injector::MakeCALL(pattern.get_first(0), AdjustWindowRect_Hook, true);
        pattern = hook::pattern("FF 15 ? ? ? ? 8B 44 24 10 85 C0");
        injector::MakeNOP(pattern.get_first(0), 6, true);
        injector::MakeCALL(pattern.get_first(0), AdjustWindowRect_Hook, true);
        pattern = hook::pattern("FF 15 ? ? ? ? 8B 74 24 1C 8B 44 24 24");
        injector::MakeNOP(pattern.get_first(0), 6, true);
        injector::MakeCALL(pattern.get_first(0), AdjustWindowRect_Hook, true);
        pattern = hook::pattern("FF 15 ? ? ? ? 80 3D ? ? ? ? ? 74 12");
        injector::MakeNOP(pattern.get_first(0), 6, true);
        injector::MakeCALL(pattern.get_first(0), SetRect_Hook, true);
        pattern = hook::pattern("FF 15 ? ? ? ? 6A 00 FF 35");
        injector::MakeNOP(pattern.get_first(0), 6, true);
        injector::MakeCALL(pattern.get_first(0), SetRect_Hook, true);
        pattern = hook::pattern("FF 15 ? ? ? ? 6A 00 68 ? ? ? ? 8D 44 24 20");
        injector::MakeNOP(pattern.get_first(0), 6, true);
        injector::MakeCALL(pattern.get_first(0), SetRect_Hook, true);

        FusionFixSettings.SetCallback("PREF_BORDERLESS", [](int32_t value) {
            SwitchWindowStyle();
        });
    }

    //fix for zoom flag in tbogt
    if (nAimingZoomFix)
    {
        auto pattern = hook::pattern("8A C1 32 05 ? ? ? ? 24 01 32 C1");
        static auto& byte_F47AB1 = **pattern.get_first<uint8_t*>(4);
        if (nAimingZoomFix > 1)
            injector::MakeNOP(pattern.get_first(-2), 2, true);
        else if (nAimingZoomFix < 0)
            injector::WriteMemory<uint8_t>(pattern.get_first(-2), 0xEB, true);

        pattern = hook::pattern("80 8E ? ? ? ? ? EB 43");
        struct AimZoomHook1
        {
            void operator()(injector::reg_pack& regs)
            {
                *(uint8_t*)(regs.esi + 0x200) |= 1;
                byte_F47AB1 = 1;
            }
        }; injector::MakeInline<AimZoomHook1>(pattern.get_first(0), pattern.get_first(7));

        pattern = hook::pattern("76 09 80 A6 ? ? ? ? ? EB 26");
        struct AimZoomHook2
        {
            void operator()(injector::reg_pack& regs)
            {
                *(uint8_t*)(regs.esi + 0x200) &= 0xFE;
                byte_F47AB1 = 0;
            }
        }; injector::MakeInline<AimZoomHook2>(pattern.get_first(2), pattern.get_first(9));

        //let's default to 0 as well
        pattern = hook::pattern("C6 05 ? ? ? ? ? 74 12 83 3D");
        injector::WriteMemory<uint8_t>(pattern.get_first(6), 0, true);
    }

    if (bFlickeringShadowsFix)
    {
        auto pattern = hook::pattern("C3 68 ? ? ? ? 6A 02 6A 00 E8 ? ? ? ? 83 C4 40 8B E5 5D C3");
        injector::WriteMemory(pattern.count(2).get(1).get<void*>(2), 0x100, true);
    }

    if (bRecoilFix)
    {
        static float fRecMult = 0.65f;
        auto pattern = hook::pattern("F3 0F 10 44 24 ? F3 0F 59 05 ? ? ? ? EB 1E");
        injector::WriteMemory(pattern.get_first(10), &fRecMult, true);
    }

    if (bDefinitionFix)
    {
        //disable forced definition-off in cutscenes
        auto pattern = hook::pattern("E8 ? ? ? ? 0F B6 0D ? ? ? ? 33 D2 84 C0 0F 45 CA 8A C1 C3");
        injector::MakeNOP(pattern.count(2).get(1).get<void*>(12), 7, true);
    }

    if (bEmissiveShaderFix)
    {
        // workaround for gta_emissivestrong.fxc lights on patch 6+,
        //"A0 01 00 00 02 00 00 08" replaced in shader files with "A1 01 00 00 02 00 00 08" (5 occurrences)
        auto pattern = hook::pattern("C1 E7 04 C1 E0 04 8B F7 8D 80 ? ? ? ? 89 4C 24 10 89 44 24 0C");
        struct ShaderTest
        {
            void operator()(injector::reg_pack& regs)
            {
                if (
                    *(uint32_t*)(regs.ebx + 0x00) == 0x39D1B717 &&
                    *(uint32_t*)(regs.ebx + 0x10) == 0x41000000 && //traffic lights and lamps
                    *(uint32_t*)(regs.ebx - 0x10) == 0x3B03126F &&
                    *(uint32_t*)(regs.ebx + 0x20) == 0x3E59999A &&
                    *(uint32_t*)(regs.ebx + 0x24) == 0x3F372474 &&
                    *(uint32_t*)(regs.ebx + 0x28) == 0x3D93A92A
                )
                {
                    auto f_ptr = (uint32_t*)(regs.ebx - 0x158);
                    if (f_ptr)
                    {
                        //auto f_00 = f_ptr[3];
                        //auto f_01 = f_ptr[7];
                        auto f_02 = f_ptr[18];
                        auto f_04 = f_ptr[10];
                        auto f_05 = f_ptr[13];
                        //auto f_06 = f_ptr[12];
                        //auto f_07 = f_ptr[17];
                        auto f_08 = f_ptr[19];
                        //auto f_09 = f_ptr[22];
                        //auto f_10 = f_ptr[23];
                        //auto f_11 = f_ptr[24];

                        if ((f_05 != 0x7f800001 && f_05 != 0xcdcdcdcd && f_04 == 0x00000000 && f_08 != 0xba8bfc22 && f_02 != 0x3f800000)
                                || (f_02 == 0x3f800000 && f_04 == 0x42480000 && f_05 == 0x00000000 && f_08 == 0x00000000))
                        {
                            *(float*)(regs.ebx + 0x00) = -*(float*)(regs.ebx + 0x00);
                        }
                    }
                }

                regs.edi = regs.edi << 4;
                regs.eax = regs.eax << 4;
            }
        }; injector::MakeInline<ShaderTest>(pattern.count(2).get(1).get<void>(0), pattern.count(2).get(1).get<void>(6));
    }

    if (bHandbrakeCamFix)
    {
        static auto unk_117E700 = hook::get_pattern("68 ? ? ? ? B9 ? ? ? ? E8 ? ? ? ? E8 ? ? ? ? F6 D8 1A C0", 1);
        static auto GET_POSITION_OF_ANALOGUE_STICKS = hook::get_pattern("6A 00 E8 ? ? ? ? 83 C4 04 80 B8 ? ? ? ? ? 74 78", 0);
        auto pattern = hook::pattern("F3 0F 10 44 24 ? F3 0F 11 04 24 8D 44 24 34 50 8D 44 24 28 50 53 8B CF");
        struct HandbrakeCam
        {
            void operator()(injector::reg_pack& regs)
            {
                int pLeftX = 0;
                int pLeftY = 0;
                int pRightX = 0;
                int pRightY = 0;
                injector::cstd<void(int, int*, int*, int*, int*)>::call(GET_POSITION_OF_ANALOGUE_STICKS, 0, &pLeftX, &pLeftY, &pRightX, &pRightY);
                if (pRightX == 0 && pRightY == 0)
                    *(float*)(regs.esp + 0x2C) *= (1.0f / 30.0f) / fTimeStep;
                //if (!*(uint8_t*)(*(uint32_t*)unk_117E700 + 0x328C))
                //    *(float*)(regs.esp + 0x2C) *= (1.0f / 30.0f) / fTimeStep;
                float f = *(float*)(regs.esp + 0x2C);
                _asm movss xmm0, dword ptr[f]
            }
        }; injector::MakeInline<HandbrakeCam>(pattern.get_first(0), pattern.get_first(6));
    }

    //camera position in TLAD
    if (bDefaultCameraAngleInTLAD)
    {
        static uint32_t episode_id = 0;
        auto pattern = hook::pattern("83 3D ? ? ? ? ? 8B 01 0F 44 C2 89 01 B0 01 C2 08 00");
        injector::WriteMemory(pattern.count(2).get(0).get<void>(2), &episode_id, true);
    }

    if (bPedDeathAnimFixFromTBOGT)
    {
        auto pattern = hook::pattern("8B D9 75 2E");
        injector::MakeNOP(pattern.get_first(2), 2, true);
    }

    if (bDisableCameraCenteringInCover)
    {
        static constexpr float xmm_0 = FLT_MAX / 2.0f;
        auto pattern = hook::pattern("F3 0F 10 05 ? ? ? ? F3 0F 58 47 ? F3 0F 11 47 ? 8B D1 89 54 24 10");
        injector::WriteMemory(pattern.get_first(4), &xmm_0, true);
    }

    if (bMouseFix)
    {
        auto pattern = hook::pattern("B9 ? ? ? ? E8 ? ? ? ? 85 C0 0F 84 ? ? ? ? F3 0F 10 9C 24 ? ? ? ? F3 0F 10 A4 24 ? ? ? ? F3 0F 10 AC 24");
        dword_F43AD8 = *pattern.get_first<uint32_t>(1);
        pattern = hook::pattern("74 3D C7 05");
        dword_1826D48 = *pattern.get_first<int32_t>(4);
        dword_1826D34 = *pattern.get_first<int32_t>(14);
        dword_1826D4C = *pattern.get_first<int32_t>(24);
        dword_1826D6C = *pattern.get_first<int32_t>(34);
        pattern = hook::pattern("51 8B 54 24 0C C7 04 24 ? ? ? ? 85 D2 75 0D 39 15 ? ? ? ? 75 17 D9 04 24 59 C3");
        injector::MakeJMP(pattern.get_first(0), MouseFix, true);

        //ped cam behaves the same way as aiming cam
        static constexpr float f255 = 255.0f;
        pattern = hook::pattern("F3 0F 59 05 ? ? ? ? F3 0F 2C C0 83 C4 0C");
        injector::WriteMemory(pattern.get_first(4), &f255, true);

        //sniper scope is slow
        static constexpr float f01 = 0.1f;
        pattern = hook::pattern("F3 0F 59 15 ? ? ? ? F3 0F 58 97");
        injector::WriteMemory(pattern.get_first(4), &f01, true);
        injector::WriteMemory(pattern.get_first(28), &f01, true);

        //first person vehicle view is slow
        pattern = hook::pattern("F3 0F 59 15 ? ? ? ? F3 0F 58 96");
        injector::WriteMemory(pattern.get_first(4), &f01, true);
        injector::WriteMemory(pattern.get_first(28), &f01, true);
    }

    //if (fFpsLimit || fCutsceneFpsLimit || fScriptCutsceneFpsLimit)
    {
        auto mode = (nFrameLimitType == 2) ? FrameLimiter::FPSLimitMode::FPS_ACCURATE : FrameLimiter::FPSLimitMode::FPS_REALTIME;
        if (mode == FrameLimiter::FPSLimitMode::FPS_ACCURATE)
            timeBeginPeriod(1);

        auto preset = FusionFixSettings("PREF_FPS_LIMIT_PRESET");
        if (preset > 2 && preset < fpsCaps.size())
            FpsLimiter.Init(mode, (float)fpsCaps[preset]);
        else
        {
            if (preset >= fpsCaps.size())
                FusionFixSettings.Set("PREF_FPS_LIMIT_PRESET", 0);
            FpsLimiter.Init(mode, fFpsLimit);
        }
        CutsceneFpsLimiter.Init(mode, fCutsceneFpsLimit);
        ScriptCutsceneFpsLimiter.Init(mode, fScriptCutsceneFpsLimit);

        auto pattern = hook::pattern("E8 ? ? ? ? 84 C0 75 89");
        CCutscenes__hasCutsceneFinished = (bool(*)()) injector::GetBranchDestination(pattern.get_first(0)).get();
        pattern = hook::pattern("E8 ? ? ? ? 84 C0 75 44 38 05 ? ? ? ? 74 26");
        CCamera__isWidescreenBordersActive = (bool(*)()) injector::GetBranchDestination(pattern.get_first(0)).get();

        pattern = hook::pattern("8B 35 ? ? ? ? 8B 0D ? ? ? ? 8B 15 ? ? ? ? A1");
        injector::WriteMemory(pattern.get_first(0), 0x901CC483, true); //nop + add esp,1C
        injector::MakeJMP(pattern.get_first(4), sub_855640, true); // + jmp

        FusionFixSettings.SetCallback("PREF_FPS_LIMIT_PRESET", [](int32_t value) {
            auto mode = (nFrameLimitType == 2) ? FrameLimiter::FPSLimitMode::FPS_ACCURATE : FrameLimiter::FPSLimitMode::FPS_REALTIME;
            if (value > 2 && value < fpsCaps.size())
                FpsLimiter.Init(mode, (float)fpsCaps[value]);
            else
                FpsLimiter.Init(mode, fFpsLimit);
        });
    }

    if (fScriptCutsceneFovLimit)
    {
        auto pattern = hook::pattern("8B 4E 0C 89 48 60 5E C3");
        struct SetFOVHook
        {
            void operator()(injector::reg_pack& regs)
            {
                float f = *(float*)(regs.esi + 0x0C);
                *(float*)(regs.eax + 0x60) = f > fScriptCutsceneFovLimit ? f : fScriptCutsceneFovLimit;
            }
        }; injector::MakeInline<SetFOVHook>(pattern.get_first(0), pattern.get_first(6));
    }

    if (nVehicleBudget)
    {
        auto pattern = hook::pattern("F7 2D ? ? ? ? 8B CA C1 E9 1F 03 CA B8 ? ? ? ? F7 2D ? ? ? ? 8B C2 C1 E8 1F 03 C2 89 0D ? ? ? ? A3 ? ? ? ? 83 C4 10 C3");
        injector::WriteMemory(*pattern.get_first<void*>(2), nVehicleBudget, true);
    }

    if (nPedBudget)
    {
        auto pattern = hook::pattern("F7 2D ? ? ? ? 8B C2 C1 E8 1F 03 C2 89 0D ? ? ? ? A3 ? ? ? ? 83 C4 10 C3");
        injector::WriteMemory(*pattern.get_first<void*>(2), nPedBudget, true);
    }

    {
        //IMG Loader
        auto pattern = hook::pattern("E8 ? ? ? ? 6A 00 E8 ? ? ? ? 83 C4 14 6A 00");
        static auto CImgManager__addImgFile = (void(__cdecl*)(const char*, char, int)) injector::GetBranchDestination(pattern.get_first(0)).get();
        static auto sub_A95980 = (void(__cdecl*)(char)) injector::GetBranchDestination(pattern.get_first(7)).get();

        pattern = hook::pattern("83 3D ? ? ? ? ? 75 0F 6A 02");
        static auto _dwCurrentEpisode = *pattern.get_first<int*>(2);

        pattern = hook::pattern("89 44 24 44 8B 44 24 4C 53");
        struct ImgListHook
        {
            void operator()(injector::reg_pack& regs)
            {
                *(uint32_t*)(regs.esp + 0x44) = regs.eax;
                regs.eax = *(uint32_t*)(regs.esp + 0x4C);

                auto exePath = std::filesystem::path(GetExeModulePath<std::wstring>());
                auto updatePath = exePath.remove_filename() / L"update";

                if (std::filesystem::exists(updatePath))
                {
                    for (const auto& file : std::filesystem::recursive_directory_iterator(updatePath, std::filesystem::directory_options::skip_permission_denied))
                    {
                        static auto gamePath = std::filesystem::path(GetExeModulePath<std::wstring>()).remove_filename();
                        auto filePath = std::filesystem::path(file.path());
                        auto relativePath = std::filesystem::relative(filePath, gamePath);

                        if (!std::filesystem::is_directory(file) && file.is_regular_file() && iequals(filePath.extension().wstring(), L".img"))
                        {
                            static std::vector<std::filesystem::path> episodicPaths = {
                                std::filesystem::path("/IV/").make_preferred(),
                                std::filesystem::path("/TLAD/").make_preferred(),
                                std::filesystem::path("/TBoGT/").make_preferred(),
                            };

                            auto is_subpath = [](const std::filesystem::path& path, const std::filesystem::path& base) -> bool {
                                std::wstring str1(path.wstring()); std::wstring str2(base.wstring());
                                std::transform(str1.begin(), str1.end(), str1.begin(), ::tolower);
                                std::transform(str2.begin(), str2.end(), str2.begin(), ::tolower);
                                return str1.contains(str2);
                            };
                            
                            auto imgPath = std::filesystem::relative(filePath, exePath).native();
                            std::replace(std::begin(imgPath), std::end(imgPath), L'\\', L'/');
                            auto pos = imgPath.find(std::filesystem::path("/").native());

                            if (pos != imgPath.npos)
                            {
                                imgPath.replace(pos, 1, std::filesystem::path(":/").native());
                                if (std::any_of(std::begin(episodicPaths), std::end(episodicPaths), [&](auto& it) { return is_subpath(relativePath, it); }))
                                {
                                    if (*_dwCurrentEpisode < episodicPaths.size() && is_subpath(relativePath, episodicPaths[*_dwCurrentEpisode]))
                                    {
                                        sub_A95980(255);
                                        CImgManager__addImgFile(std::filesystem::path(imgPath).string().c_str(), 1, -1);
                                        sub_A95980(0);
                                    }
                                }
                                else
                                {
                                    sub_A95980(255);
                                    CImgManager__addImgFile(std::filesystem::path(imgPath).string().c_str(), 1, -1);
                                    sub_A95980(0);
                                }
                            }
                        }
                    }
                }
            }
        }; injector::MakeInline<ImgListHook>(pattern.get_first(0), pattern.get_first(8));
    }

    if (false)
    {
        auto pattern = hook::pattern("E8 ? ? ? ? 83 C4 08 57 56");
        fnAE3DE0 = injector::GetBranchDestination(pattern.get_first()).get();
        injector::MakeCALL(pattern.get_first(), sub_AE3DE0, true);

        pattern = hook::pattern("55 8B EC 83 E4 F0 83 EC 28 80 3D ? ? ? ? ? 56 57 74 27");
        fnAE3310 = pattern.get_first();

        pattern = hook::pattern("75 14 F6 86 ? ? ? ? ? 74 0B");
        injector::WriteMemory<uint8_t>(pattern.get_first(0), 0xEB, true);
        pattern = hook::pattern("75 12 8B 86 ? ? ? ? C1 E8 0B");
        injector::WriteMemory<uint8_t>(pattern.get_first(0), 0xEB, true);
    }

    //timecycb
    static const char* timecycText[] = { "Low", "Medium", "High", "Very High", "Highest", "MO_DEF", "OFF", "IV", "TLAD", "TBOGT" };
    {
        if (FusionFixSettings("PREF_TIMECYC") < 5 || FusionFixSettings("PREF_TIMECYC") >= std::size(timecycText))
            FusionFixSettings.Set("PREF_TIMECYC", 5);

        auto pattern = hook::pattern("E8 ? ? ? ? 8B F0 83 C4 08 85 F6 0F 84 ? ? ? ? BB");
        CFileMgrOpenFile = injector::GetBranchDestination(pattern.get_first(0)).get();
        injector::MakeCALL(pattern.get_first(0), sub_8C4CF0, true);

        pattern = hook::pattern("55 8B EC 83 E4 F0 81 EC ? ? ? ? 8B 0D ? ? ? ? 53 0F B7 41 04");
        static auto CTimeCycleInitialise = pattern.get_first(0);

        static int bTimecycUpdated = 0;
        FusionFixSettings.SetCallback("PREF_TIMECYC", [](int32_t value) {
            injector::fastcall<void()>::call(CTimeCycleInitialise);
            bTimecycUpdated = 200;
        });

        // make timecyc changes visible in menu
        pattern = hook::pattern("0A 05 ? ? ? ? 0A 05 ? ? ? ? 0F 85 ? ? ? ? E8 ? ? ? ? 84 C0 0F 85 ? ? ? ? F3 0F 10 05 ? ? ? ? F3 0F 11 04 24");
        static auto byte_1173590 = *pattern.get_first<uint8_t*>(2);
        struct MenuTimecycHook
        {
            void operator()(injector::reg_pack& regs)
            {
                *(uint8_t*)&regs.eax |= *byte_1173590;
                if (bTimecycUpdated > 0) {
                    *(uint8_t*)&regs.eax = 0;
                    bTimecycUpdated--;
                }
            }
        }; injector::MakeInline<MenuTimecycHook>(pattern.get_first(0), pattern.get_first(6));
        pattern = hook::pattern("0A 05 ? ? ? ? 0A 05 ? ? ? ? 74 20");
        injector::MakeInline<MenuTimecycHook>(pattern.get_first(0), pattern.get_first(6));
    }

    // runtime settings
    {
        auto pattern = hook::pattern("89 1C 95 ? ? ? ? E8 ? ? ? ? A1 ? ? ? ? 83 C4 04");
        struct IniWriter
        {
            void operator()(injector::reg_pack& regs)
            {
                auto id = regs.edx;
                auto value = regs.ebx;
                if (FusionFixSettings.isSame(id, "PREF_FPS_LIMIT_PRESET")) {
                    if (regs.ebx == 1) {
                        auto old = FusionFixSettings(id);
                        if (old >= 2)
                            value = 0;
                        else
                            value = 2;
                    }
                } else if (FusionFixSettings.isSame(id, "PREF_TIMECYC")) {
                    if (value <= 5) {
                        auto old = FusionFixSettings(id);
                        if (old > 5)
                            value = 5;
                        else
                            value = std::distance(std::begin(timecycText), std::end(timecycText)) - 1;
                    }
                }
                FusionFixSettings.Set(id, value);
            }
        }; injector::MakeInline<IniWriter>(pattern.get_first(0), pattern.get_first(7));

        pattern = hook::pattern("8B 1C 95 ? ? ? ? 89 54 24 14");
        struct MenuTogglesHook1
        {
            void operator()(injector::reg_pack& regs)
            {
                regs.ebx = FusionFixSettings.Get(regs.edx);
            }
        }; injector::MakeInline<MenuTogglesHook1>(pattern.get_first(0), pattern.get_first(7));

        pattern = hook::pattern("8B 14 85 ? ? ? ? 66 83 F9 31");
        struct MenuTogglesHook2
        {
            void operator()(injector::reg_pack& regs)
            {
                regs.edx = FusionFixSettings.Get(regs.eax);
            }
        }; injector::MakeInline<MenuTogglesHook2>(pattern.get_first(0), pattern.get_first(7));

        // show game in display menu
        pattern = hook::pattern("75 1F FF 35 ? ? ? ? E8 ? ? ? ? 8B 4C 24 18");
        injector::MakeNOP(pattern.get_first(), 2);

        pattern = hook::pattern("83 F8 03 0F 44 CE");
        struct MenuHook
        {
            void operator()(injector::reg_pack& regs)
            {
                regs.ecx = 1;
            }
        }; injector::MakeInline<MenuHook>(pattern.get_first(0), pattern.get_first(6));

        pattern = hook::pattern("7E 4E 8A 1D");
        injector::WriteMemory<uint8_t>(pattern.get_first(0), 0xEB, true);
    }

    {
        static std::map<IDirect3DPixelShader9*, std::tuple<IDirect3DPixelShader9*, IDirect3DPixelShader9*, IDirect3DPixelShader9*, IDirect3DPixelShader9*>> shadermap;
        auto pattern = hook::pattern("53 57 51 FF 90");
        struct CreatePixelShaderHook
        {
            void operator()(injector::reg_pack& regs)
            {
                auto pDevice = (IDirect3DDevice9*)regs.ecx;
                DWORD* pFunction = (DWORD*)regs.edi;
                IDirect3DPixelShader9** ppShader = (IDirect3DPixelShader9**)regs.ebx;
                pDevice->CreatePixelShader(pFunction, ppShader);
                IDirect3DPixelShader9* pShader = *ppShader;

                if (pShader != nullptr)
                {
                    static std::vector<uint8_t> pbFunc;
                    UINT len;
                    pShader->GetFunction(nullptr, &len);
                    if (pbFunc.size() < len)
                        pbFunc.resize(len);

                    pShader->GetFunction(pbFunc.data(), &len);

                    //def c27, 1, 77, 88, 99            // FXAA Toggle
                    //def c28, 0, 77, 88, 99            // Console Gamma Toggle
                    auto pattern = hook::pattern((uintptr_t)pbFunc.data(), (uintptr_t)pbFunc.data() + pbFunc.size(), "05 ? 00 0F A0 ? ? ? ? 00 00 00 00 00 00 B0 42 00 00 C6 42");
                    if (!pattern.empty())
                    {
                        if (!shadermap.contains(pShader))
                        {
                            auto create_shader_variation = [&](int32_t fxaa, int32_t gamma) -> IDirect3DPixelShader9*
                            {
                                IDirect3DPixelShader9* shader;
                                pattern.for_each_result([&](hook::pattern_match match) {
                                    if (*match.get<uint8_t>(1) == 0x1B) // fxaa
                                        injector::WriteMemory(match.get<void>(5), fxaa, true);
                                    else if (*match.get<uint8_t>(1) == 0x1C) // gamma
                                        injector::WriteMemory(match.get<void>(5), gamma, true);
                                });
                                pDevice->CreatePixelShader((DWORD*)pbFunc.data(), &shader);
                                return shader;
                            };

                            static constexpr auto ENABLE  = 0x3F800000;
                            static constexpr auto DISABLE = 0x00000000;
                            auto fxaa_off_gamma_off = create_shader_variation(DISABLE, DISABLE);
                            auto fxaa_off_gamma_on = create_shader_variation(DISABLE, ENABLE);
                            auto fxaa_on_gamma_off = create_shader_variation(ENABLE, DISABLE);
                            auto fxaa_on_gamma_on = create_shader_variation(ENABLE, ENABLE);
                            shadermap.emplace(pShader, std::make_tuple(fxaa_off_gamma_off, fxaa_off_gamma_on, fxaa_on_gamma_off, fxaa_on_gamma_on));
                        }
                    }
                }
            }
        }; injector::MakeInline<CreatePixelShaderHook>(pattern.get_first(0), pattern.get_first(9));

        pattern = hook::pattern("A1 ? ? ? ? 52 8B 08 50 89 15 ? ? ? ? FF 91 ? ? ? ? 8B 44 24 10");
        static auto pD3DDevice = *pattern.get_first<IDirect3DDevice9**>(1);
        struct SetPixelShaderHook
        {
            void operator()(injector::reg_pack& regs)
            {
                regs.eax = *(uint32_t*)pD3DDevice;
                auto pDevice = *pD3DDevice;
                {
                    auto pShader = (IDirect3DPixelShader9*)regs.edx;
                    if (shadermap.contains(pShader))
                    {
                        static auto fxaa = FusionFixSettings.GetRef("PREF_FXAA");
                        static auto gamma = FusionFixSettings.GetRef("PREF_CONSOLE_GAMMA");
                        if (fxaa && gamma)
                        {
                            if (fxaa->get())
                                if (gamma->get())
                                    regs.edx = (uint32_t)std::get<3>(shadermap.at(pShader));
                                else
                                    regs.edx = (uint32_t)std::get<2>(shadermap.at(pShader));
                            else
                                if (gamma->get())
                                    regs.edx = (uint32_t)std::get<1>(shadermap.at(pShader));
                                else
                                    regs.edx = (uint32_t)std::get<0>(shadermap.at(pShader));
                        }
                    }
                }
            }
        }; injector::MakeInline<SetPixelShaderHook>(pattern.get_first(0));
    }

    if (bExtraDynamicShadows || bDynamicShadowForTrees)
    {
        auto pattern = hook::pattern("E8 ? ? ? ? EB 11 8D 44 24 54");
        CModelInfoStore__allocateBaseModel = injector::GetBranchDestination(pattern.get_first(0));
        injector::MakeCALL(pattern.get_first(0), CModelInfoStore__allocateBaseModelHook, true);
        pattern = hook::pattern("E8 ? ? ? ? 8B F0 83 C4 04 8D 44 24 6C");
        CModelInfoStore__allocateInstanceModel = injector::GetBranchDestination(pattern.get_first(0));
        injector::MakeCALL(pattern.get_first(0), CModelInfoStore__allocateInstanceModelHook, true);
        pattern = hook::pattern("D9 6C 24 0E 56");
        CBaseModelInfo__setFlags = injector::GetBranchDestination(pattern.get_first(5));
        injector::MakeCALL(pattern.get_first(5), CBaseModelInfo__setFlagsHook, true);

        std::vector<std::string> vegetationNames = {
            "bush", "weed", "grass", "azalea", "bholly", "fern", "tree"
        };

        if (bExtraDynamicShadows == 2)
            modelNames.insert(modelNames.end(), vegetationNames.begin(), vegetationNames.end());

        //sway
        if (bDynamicShadowForTrees)
        {
            static auto dw1036C00 = *hook::get_pattern<float*>("F3 0F 5C 2D ? ? ? ? F3 0F 10 35", 4);
            static auto dw1036C04 = dw1036C00 + 1;
            static auto dw1036C08 = dw1036C00 + 2;
            static auto dw11A2948 = *hook::get_pattern<float*>("C7 05 ? ? ? ? ? ? ? ? 0F 85 ? ? ? ? 6A 00", 2);
            pattern = hook::pattern("8B 80 ? ? ? ? FF 74 24 20 8B 08 53 FF 74 24 20");
            struct SetVertexShaderConstantFHook
            {
                void operator()(injector::reg_pack& regs)
                {
                    regs.eax = *(uint32_t*)(regs.eax + 0x11AC);
                    auto pD3DDevice = (IDirect3DDevice9*)(regs.eax);
                    auto StartRegister = *(UINT*)(regs.esp + 0x18);
                    auto pConstantData = (float*)regs.ebx;
                    auto Vector4fCount = *(UINT*)(regs.esp + 0x20);

                    if (StartRegister == 51 && Vector4fCount == 1) {
                        if (pConstantData[0] == 1.0f && pConstantData[1] == 1.0f && pConstantData[2] == 1.0f && pConstantData[3] == 1.0f) {
                            static float arr[4];
                            arr[0] = *dw1036C00;
                            arr[1] = *dw1036C04;
                            arr[2] = *dw1036C08;
                            arr[3] = *dw11A2948;
                            pD3DDevice->SetVertexShaderConstantF(233, &arr[0], 1);
                        }
                    }
                }
            }; injector::MakeInline<SetVertexShaderConstantFHook>(pattern.count(2).get(1).get<void*>(0), pattern.count(2).get(1).get<void*>(6));
        }
    }
}

CEXP void InitializeASI()
{
    std::call_once(CallbackHandler::flag, []()
    {
        CallbackHandler::RegisterCallback(Init, hook::pattern("F3 0F 10 44 24 ? F3 0F 59 05 ? ? ? ? EB ? E8"));
    });
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID lpReserved)
{
    if (reason == DLL_PROCESS_ATTACH)
    {
        if (!IsUALPresent()) { InitializeASI(); }
    }
    if (reason == DLL_PROCESS_DETACH)
    {
        if (nFrameLimitType == FrameLimiter::FPSLimitMode::FPS_ACCURATE)
            timeEndPeriod(1);
    }
    return TRUE;
}
