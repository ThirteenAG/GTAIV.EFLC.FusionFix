#include <misc.h>
#include <mmsystem.h>
#pragma comment(lib, "winmm.lib") // needed for timeBeginPeriod()/timeEndPeriod()
#include <filesystem>
#include <map>
#include <d3d9.h>
#include <bitset>

struct Vector3
{
    float fX;
    float fY;
    float fZ;
};

struct Vector4
{
    float fX;
    float fY;
    float fZ;
    float fW;
};

class scrNativeCallContext
{
protected:
    void*    m_pReturn;
    uint32_t m_nArgCount;
    void*    m_pArgs;
    uint32_t m_nDataCount;
    Vector3* m_pOriginalData[4];
    Vector4  m_TemporaryData[4];

public:
    template<typename T>
    inline T GetArgument(int idx)
    {
        auto arguments = (intptr_t*)m_pArgs;
        return *reinterpret_cast<T*>(&arguments[idx]);
    }

    inline void* GetArgumentBuffer()
    {
        return m_pArgs;
    }

    template<typename T>
    inline void SetResult(int idx, T value)
    {
        auto returnValues = (intptr_t*)m_pReturn;
        *reinterpret_cast<T*>(&returnValues[idx]) = value;
    }

    inline int GetArgumentCount()
    {
        return m_nArgCount;
    }

    template<typename T>
    inline T GetResult(int idx)
    {
        while (m_nDataCount > 0)
        {
            m_nDataCount--;
            Vector3* pVec3 =  m_pOriginalData[m_nDataCount];
            Vector4* pVec4 = &m_TemporaryData[m_nDataCount];
            pVec3->fX = pVec4->fX;
            pVec3->fY = pVec4->fY;
            pVec3->fZ = pVec4->fZ;
        }
        auto returnValues = (intptr_t*)m_pReturn;
        return *reinterpret_cast<T*>(&returnValues[idx]);
    }
};

class NativeContext : public scrNativeCallContext
{
private:
    // Configuration
    enum
    {
        MaxNativeParams = 16,
        ArgSize = 4,
    };

    // Anything temporary that we need
    uint8_t m_TempStack[MaxNativeParams * ArgSize] = {};

public:
    inline NativeContext()
    {
        m_pArgs = &m_TempStack;
        m_pReturn = &m_TempStack;		// It's okay to point both args and return at the same pointer. The game should handle this.
        m_nArgCount = 0;
        m_nDataCount = 0;
    }

    template <typename T>
    inline void Push(T value)
    {
        if (sizeof(T) > ArgSize)
        {
            // We only accept sized 4 or less arguments... that means no double/f64 or large structs are allowed.
            throw "Argument has an invalid size";
        }
        else if (sizeof(T) < ArgSize)
        {
            // Ensure we don't have any stray data
            *reinterpret_cast<uintptr_t*>(m_TempStack + ArgSize * m_nArgCount) = 0;
        }

        *reinterpret_cast<T*>(m_TempStack + ArgSize * m_nArgCount) = value;
        m_nArgCount++;
    }

    template <typename T>
    inline T GetResult()
    {
        return *reinterpret_cast<T*>(m_TempStack);
    }
};

class NativeInvoke
{
public:
    template<uint32_t Hash, typename R, typename... Args>
    static inline R Invoke(Args... args)
    {
        NativeContext cxt;
        (cxt.Push(args), ...);

        static auto GetNativeHandler = hook::pattern("56 8B 35 ? ? ? ? 85 F6 75 06").count(2).get(1).get<void*(__stdcall)(uint32_t)>(0);
        auto fn = GetNativeHandler(Hash);
        if (fn)
            ((void(*)(NativeContext*))fn)(&cxt);

        if constexpr (!std::is_void_v<R>)
        {
            return cxt.GetResult<R>();
        }
    }
};

enum class NATIVES : uint32_t {
    GET_ROOT_CAM = 0x75E005F1,
    GET_CAM_NEAR_CLIP = 0x2EF477FD,
    GET_CAM_FAR_CLIP = 0x752643C9,
};

using Cam = int32_t;
static auto GET_ROOT_CAM(Cam* rootcam) { return NativeInvoke::Invoke<std::to_underlying(NATIVES::GET_ROOT_CAM), void>(rootcam); }
static auto GET_CAM_NEAR_CLIP(Cam cam, float* clip) { return NativeInvoke::Invoke<std::to_underlying(NATIVES::GET_CAM_NEAR_CLIP), void>(cam, clip); }
static auto GET_CAM_FAR_CLIP(Cam cam, float* clip) { return NativeInvoke::Invoke<std::to_underlying(NATIVES::GET_CAM_FAR_CLIP), void>(cam, clip); }

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
        eRadio, eSequential, eShuffle, evanilla, evanillaplus, eSimpleBilinear, escreennoise,
        escreennoisefpcss, e1040, e1040fpcss, esoft3x3box, esoft3x3gausian, esoft5x5box, esoft5x5gausian, everysoft7x7box,
        everysoft7x7gausian, etest1, etest2, etest3
    };
    std::vector<const char*> data = { "Radio", "Sequential", "Shuffle", "vanilla", "vanilla+", "Simple Bilinear", "screen noise", "screen noise fpcss",
        "1040", "1040 fpcss", "soft 3x3 box", "soft 3x3 gausian", "soft 5x5 box", "soft 5x5 gausian", "very soft 7x7 box",
        "very soft 7x7 gausian", "test 1", "test 2", "test 3" };
} ShadowFilterText;

struct
{
    enum eDofText {
        eAuto, e43, e54, e159, e169, eOff, eLow, eMedium, eHigh, eVeryHigh
    };
    std::vector<const char*> data = { "Auto", "4:3", "5:4", "15:9", "16:9", "Off", "Low", "Medium", "High", "Very High" };
} DofText;

class CSettings
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

        static CSetting arr[] = {
            { 0, "PREF_SKIP_INTRO",       "MAIN",       "SkipIntro",                    "",                           1, nullptr, 0, 1 },
            { 0, "PREF_SKIP_MENU",        "MAIN",       "SkipMenu",                     "",                           1, nullptr, 0, 1 },
            { 0, "PREF_BORDERLESS",       "MAIN",       "BorderlessWindowed",           "",                           1, nullptr, 0, 1 },
            { 0, "PREF_FPS_LIMIT_PRESET", "FRAMELIMIT", "FpsLimitPreset",               "MENU_DISPLAY_FRAMELIMIT",    0, nullptr, FpsCaps.eOFF, std::distance(std::begin(FpsCaps.data), std::end(FpsCaps.data)) - 1 },
            { 0, "PREF_FXAA",             "MISC",       "FXAA",                         "",                           1, nullptr, 0, 1 },
            { 0, "PREF_CONSOLE_GAMMA",    "MISC",       "ConsoleGamma",                 "",                           0, nullptr, 0, 1 },
            { 0, "PREF_TIMECYC",          "MISC",       "ScreenFilter",                 "MENU_DISPLAY_TIMECYC",       5, nullptr, TimecycText.eMO_DEF, std::distance(std::begin(TimecycText.data), std::end(TimecycText.data)) - 1 },
            { 0, "PREF_CUTSCENE_DOF",     "MISC",       "DepthOfField",                 "",                           0, nullptr, 0, 1 },
            { 0, "PREF_CONSOLE_SHADOWS",  "SHADOWS",    "ConsoleShadows",               "",                           1, nullptr, 0, 1 },
            { 0, "PREF_SHADOW_FILTER",    "SHADOWS",    "ShadowFilter",                 "MENU_DISPLAY_SHADOWFILTER",  0, nullptr, ShadowFilterText.evanilla, std::distance(std::begin(ShadowFilterText.data), std::end(ShadowFilterText.data)) - 1 },
            { 0, "PREF_TREE_LIGHTING",    "MISC",       "TreeLighting",                 "MENU_DISPLAY_TREE_LIGHTING", 0, nullptr, 0, 1 },
            { 0, "PREF_TCYC_DOF",         "MISC",       "DistantBlur",                  "MENU_DISPLAY_DOF",           5, nullptr, DofText.eOff, std::distance(std::begin(DofText.data), std::end(DofText.data)) - 1 },
            { 0, "PREF_DEFINITION",       "MAIN",       "Definition",                   ""                          , 0, nullptr, 0, 1 },
        };

        auto i = firstCustomID;
        for (auto& it : arr)
        {
            mFusionPrefs[i] = it;
            mFusionPrefs[i].value = it.ReadFromIni(iniReader);
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

        for (auto& it : arr)
        {
            if (!it.strEnum.empty()) {
                aMenuEnums.emplace_back(firstEnumCustomID, it.strEnum.data());
                firstEnumCustomID += 1;
            }
        }

        injector::WriteMemory(pattern.count(4).get(1).get<void*>(3), &aMenuEnums[0].prefID, true);
        injector::WriteMemory(pattern2.get_first(3), &aMenuEnums[0].name, true);
        pattern = hook::pattern("8B 04 F5 ? ? ? ? 5F 5E C3 8B 04 F5");
        injector::WriteMemory(pattern.get_first(3), &aMenuEnums[0].prefID, true);
        pattern = hook::pattern("FF 34 F5 ? ? ? ? 57 E8 ? ? ? ? 83 C4 08 85 C0 74 0B 46 83 FE 3C");
        injector::WriteMemory(pattern.get_first(3), &aMenuEnums[0].name, true);

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
    void ForEachPref(std::function<void(int32_t id, int32_t idStart, int32_t idEnd)>&& cb)
    {
        for (auto& it : mFusionPrefs)
        {
            cb(it.first, it.second.idStart, it.second.idEnd);
        }
    }
    auto operator()(int32_t i) { return Get(i); }
    auto operator()(std::string_view name) { return Get(name); }
} FusionFixSettings;

int32_t nFrameLimitType;
float fFpsLimit;
float fCutsceneFpsLimit;
float fScriptCutsceneFpsLimit;
float fScriptCutsceneFovLimit;

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
    bool   bFpsLimitWasUpdated = false;
public:
    void Init(FPSLimitMode mode, float fps_limit)
    {
        bFpsLimitWasUpdated = true;
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
        if (bFpsLimitWasUpdated)
        {
            bFpsLimitWasUpdated = false;
            return 1;
        }

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
        if (bFpsLimitWasUpdated)
        {
            bFpsLimitWasUpdated = false;
            return 1;
        }

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

bool bLoadingShown = false;
bool __cdecl sub_411F50(uint32_t* a1, uint32_t* a2)
{
    bLoadingShown = false;
    if (!a1[2] && !a2[2]) {
        bLoadingShown = *a1 == *a2;
        return *a1 == *a2;
    }
    if (a1[2] != a2[2])
        return false;
    if (a1[0] != a2[0])
        return false;
    if (a1[1] != a2[1])
        return false;
    bLoadingShown = true;
    return true;
}

FrameLimiter FpsLimiter;
FrameLimiter CutsceneFpsLimiter;
FrameLimiter ScriptCutsceneFpsLimiter;
bool(*CCutscenes__hasCutsceneFinished)();
bool(*CCamera__isWidescreenBordersActive)();
uint8_t* bLoadscreenShown = nullptr;
void __cdecl sub_855640()
{
    static auto preset = FusionFixSettings.GetRef("PREF_FPS_LIMIT_PRESET");

    if (bLoadscreenShown && !*bLoadscreenShown && !bLoadingShown)
    {
        if (preset && *preset >= FpsCaps.eCustom) {
            if (fFpsLimit > 0.0f || (*preset > FpsCaps.eCustom && *preset < FpsCaps.data.size()))
                FpsLimiter.Sync();
        }
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

void* fnAE3DE0 = nullptr;
void* fnAE3310 = nullptr;
int __cdecl sub_AE3DE0(int a1, int a2)
{
    static auto console_shadows = FusionFixSettings.GetRef("PREF_CONSOLE_SHADOWS");
    if (console_shadows->get())
        injector::cstd<void(int, int, int, int, int)>::call(fnAE3310, a1, 0, 0, 0, a2);
    return injector::cstd<int(int, int)>::call(fnAE3DE0, a1, a2);
}

injector::hook_back<void(__cdecl*)(int, int, int, float*, int*, float*, int, int, int, int, float, float, float, int, int, int, int, int)> hbsub_ABCCD0;
void __cdecl sub_ABCCD0(int a1, int a2, int a3, float* a4, int* a5, float* a6, int a7, int a8, int a9, int a10, float a11, float a12, float a13, int a14, int a15, int a16, int a17, int a18)
{
    static auto console_shadows = FusionFixSettings.GetRef("PREF_CONSOLE_SHADOWS");
    if (console_shadows->get())
        return;
    else
        return hbsub_ABCCD0.fun(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14, a15, a16, a17, a18);
}

injector::hook_back<bool(*)()> hbsub_924D90;
bool sub_924D90()
{
    static auto console_shadows = FusionFixSettings.GetRef("PREF_CONSOLE_SHADOWS");
    if (console_shadows->get())
        return false;
    else
        return hbsub_924D90.fun();
};

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

//void* CFileMgrOpenFile = nullptr;
//void* __cdecl sub_8C4CF0(char* a1, char* a2)
//{
//    if (FusionFixSettings("PREF_TIMECYC"))
//    {
//        static constexpr auto timecyc = "timecyc.dat";
//        auto full_path = std::string_view(a1);
//        if (full_path.ends_with(timecyc))
//        {
//            auto path = full_path.substr(0, full_path.find(timecyc));
//            switch (FusionFixSettings("PREF_TIMECYC"))
//            {
//            case TimecycText.eOFF:
//            {
//                auto new_path = std::string(path) + std::string("timecycOFF.dat");
//                auto opened = injector::cstd<void* (const char*, const char*)>::call(CFileMgrOpenFile, new_path.c_str(), a2);
//                if (opened) return opened;
//            }
//            break;
//            case TimecycText.eIV:
//            {
//                auto new_path = std::string(path) + std::string("timecycIV.dat");
//                auto opened = injector::cstd<void* (const char*, const char*)>::call(CFileMgrOpenFile, new_path.c_str(), a2);
//                if (opened) return opened;
//            }
//            break;
//            case TimecycText.eTLAD:
//            {
//                auto new_path = std::string(path) + std::string("timecycTLAD.dat");
//                auto opened = injector::cstd<void* (const char*, const char*)>::call(CFileMgrOpenFile, new_path.c_str(), a2);
//                if (opened) return opened;
//            }
//            break;
//            case TimecycText.eTBOGT:
//            {
//                auto new_path = std::string(path) + std::string("timecycTBOGT.dat");
//                auto opened = injector::cstd<void* (const char*, const char*)>::call(CFileMgrOpenFile, new_path.c_str(), a2);
//                if (opened) return opened;
//            }
//            break;
//            default:
//                break;
//            }
//        }
//    }
//    return injector::cstd<void* (char*, char*)>::call(CFileMgrOpenFile, a1, a2);
//}

int32_t* _dwCurrentEpisode;
int scanfCount = 0;
int timecyc_scanf(const char* i, const char* fmt, int* a1, int* a2, int* a3, int* a4, int* a5, int* a6, int* a7, int* a8, int* a9, int* a10,
    int* a11, int* a12, int* a13, int* a14, int* a15, int* a16, int* a17, int* a18, int* a19, int* a20, int* a21, float* a22, float* a23,
    float* a24, float* a25, int* a26, int* a27, int* a28, int* a29, int* a30, int* a31, int* a32, int* a33, int* a34, int* a35, float* a36,
    float* a37, float* a38, float* a39, int* a40, int* a41, int* a42, int* a43, int* a44, int* a45, float* a46, float* a47, float* a48, float* a49,
    float* a50, float* a51, float* a52, float* a53, float* a54, float* a55, float* a56, int* a57, float* a58, float* a59, float* a60, float* a61,
    float* a62, int* a63, float* a64, float* a65, float* a66, float* a67, float* a68, float* a69, float* a70, float* a71, float* a72, float* a73,
    float* a74, float* a75, float* a76, float* a77, float* a78, float* a79, float* a80, float* a81, float* a82, float* a83, float* a84, float* a85,
    float* a86, float* a87, float* a88, float* a89, float* a90, float* a91, float* a92, float* a93, float* a94, float* a95, float* a96, float* a97,
    float* a98, float* a99, float* a100, float* a101, float* a102, float* a103, float* a104, float* a105, float* a106, float* a107, float* a108,
    float* a109, float* a110, float* a111, float* a112, float* a113, float* a114, float* a115, float* a116, float* a117, float* a118, float* a119,
    float* a120, float* a121, float* a122, float* a123, float* a124, float* a125, float* a126, float* a127, float* a128, float* a129, float* a130,
    float* a131, float* a132, float* a133, float* a134)
{
    auto res = sscanf(i, fmt, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14, a15, a16, a17, a18, a19, a20, a21, a22, a23, a24, a25, a26, a27,
        a28, a29, a30, a31, a32, a33, a34, a35, a36, a37, a38, a39, a40, a41, a42, a43, a44, a45, a46, a47, a48, a49, a50, a51, a52, a53, a54, a55,
        a56, a57, a58, a59, a60, a61, a62, a63, a64, a65, a66, a67, a68, a69, a70, a71, a72, a73, a74, a75, a76, a77, a78, a79, a80, a81, a82, a83,
        a84, a85, a86, a87, a88, a89, a90, a91, a92, a93, a94, a95, a96, a97, a98, a99, a100, a101, a102, a103, a104, a105, a106, a107, a108, a109,
        a110, a111, a112, a113, a114, a115, a116, a117, a118, a119, a120, a121, a122, a123, a124, a125, a126, a127, a128, a129, a130, a131, a132,
        a133, a134);

    switch (FusionFixSettings("PREF_TCYC_DOF"))
    {
    case DofText.eOff:
        *a124 *= 0.0f;
        *a125 *= 0.0f;
        break;
    case DofText.eLow:
        *a124 *= 0.7f;
        *a125 *= 0.7f;
        break;
    case DofText.eMedium:
        *a124 *= 0.8f;
        *a125 *= 0.8f;
        break;
    case DofText.eHigh:
        *a124 *= 0.9f;
        *a125 *= 0.9f;
        break;
    case DofText.eVeryHigh:
        [[fallthrough]];
    default:
        break;
    }

    {
        static int IV2TLAD[3][100] =
        {
            { 96,101,92,130,158,161,156,143,140,138,107,128,128,101,122,143,125,133,122,122,133,128,119,119,110,115,138,133,138,148,133,133,119,155,155,78,98,135,130,130,138,98,98,155,99,99,89,104,136,133,133,104,102,91,91,91,91,104,104,121,121,121,121,117,117,117,96,96,96,109,117,117,109,117,99,107,107,104,104,130,92,99,104,104,124,104,104,104,99,119,122,128,120,128,115,118,122,122,128 },
            { 146,142,136,111,133,117,129,104,104,108,133,128,128,130,110,104,109,106,110,110,96,128,140,140,120,101,127,122,125,132,96,96,140,181,181,100,108,127,124,118,119,108,108,181,112,112,88,117,138,143,143,120,97,125,125,125,125,131,131,133,133,133,142,130,130,130,111,111,111,121,117,117,122,117,105,120,120,120,120,120,112,117,117,117,130,124,120,120,112,141,110,128,114,128,122,140,110,110,128 },
            { 156,148,138,83,92,74,73,54,56,69,138,128,128,109,94,69,77,68,94,94,66,128,144,144,102,64,90,82,80,75,66,66,144,186,186,105,110,88,95,86,85,110,110,186,120,120,60,130,118,142,142,99,80,116,116,116,116,140,140,143,143,143,143,125,125,125,120,120,120,122,117,117,122,117,97,103,103,99,99,104,120,130,130,130,104,130,99,99,120,145,94,128,90,128,108,134,94,94,128 },
        };

        static int IV2TBOGT[3][100] =
        {
            { 107,103,86,120,130,115,130,138,138,138,107,128,128,130,122,109,109,94,122,94,133,128,119,119,108,122,116,138,138,138,133,133,119,155,155,78,98,107,107,107,138,115,115,155,99,99,89,104,136,133,133,104,102,91,91,91,91,104,104,121,121,121,121,117,117,117,96,96,96,109,117,117,109,117,99,107,107,104,104,130,92,99,104,104,124,104,104,104,99,119,122,128,120,128,115,118,122,122,128 },
            { 133,130,116,105,120,122,120,108,108,108,133,128,128,101,110,120,120,111,110,111,103,128,140,140,120,110,140,131,131,131,96,96,140,181,181,100,108,124,124,126,124,126,126,181,112,112,88,117,138,143,143,120,97,125,125,125,125,131,131,133,133,133,142,130,130,130,111,111,111,121,117,117,122,117,105,120,120,120,120,120,112,117,117,117,130,124,120,120,112,141,110,128,114,128,122,140,110,110,128 },
            { 138,134,117,84,98,113,98,69,69,69,138,128,128,125,94,122,122,122,94,122,101,128,144,144,99,94,128,106,106,106,66,66,144,186,186,105,110,128,128,128,98,128,128,186,120,120,60,130,118,142,142,99,80,116,116,116,116,140,140,143,143,143,143,125,125,125,120,120,120,122,117,117,122,117,97,103,103,99,99,104,120,130,130,130,104,130,99,99,120,145,94,128,90,128,108,134,94,94,128 },
        };

        static int IV2OFF[3][100] =
        {
            { 125,123,107,103,116,117,116,105,105,105,125,128,128,113,109,117,117,108,109,109,98,128,134,134,109,109,128,125,125,125,98,98,134,174,174,94,105,120,120,120,120,105,105,174,110,110,79,117,131,139,139,108,93,111,111,111,111,125,125,132,132,132,135,124,124,124,109,109,109,117,117,117,117,117,100,110,110,108,108,118,108,115,117,117,119,119,108,108,110,135,109,128,108,128,115,131,109,109,128 },
            { 125,123,107,103,116,117,116,105,105,105,125,128,128,113,109,117,117,108,109,109,98,128,134,134,109,109,128,125,125,125,98,98,134,174,174,94,105,120,120,120,120,105,105,174,110,110,79,117,131,139,139,108,93,111,111,111,111,125,125,132,132,132,135,124,124,124,109,109,109,117,117,117,117,117,100,110,110,108,108,118,108,115,117,117,119,119,108,108,110,135,109,128,108,128,115,131,109,109,128 },
            { 125,123,107,103,116,117,116,105,105,105,125,128,128,113,109,117,117,108,109,109,98,128,134,134,109,109,128,125,125,125,98,98,134,174,174,94,105,120,120,120,120,105,105,174,110,110,79,117,131,139,139,108,93,111,111,111,111,125,125,132,132,132,135,124,124,124,109,109,109,117,117,117,117,117,100,110,110,108,108,118,108,115,117,117,119,119,108,108,110,135,109,128,108,128,115,131,109,109,128 },
        };

        static int TLAD2IV[3][100] =
        {
            { 107,103,86,120,130,115,130,138,138,138,107,128,128,101,122,109,125,122,122,122,133,128,119,119,108,122,116,138,138,138,133,133,119,155,155,78,98,107,107,138,98,98,98,155,99,99,89,104,136,133,133,104,102,91,91,91,91,104,104,121,121,121,121,117,117,117,96,96,96,109,117,117,109,117,99,107,107,104,104,130,92,99,104,104,124,104,104,104,99,119,122,128,120,128,115,118,122,122,128 },
            { 133,130,116,105,120,122,120,108,108,108,133,128,128,130,110,120,110,110,110,110,96,128,140,140,120,110,140,131,131,131,96,96,140,181,181,100,108,124,126,124,108,108,108,181,112,112,88,117,138,143,143,120,97,125,125,125,125,131,131,133,133,133,142,130,130,130,111,111,111,121,117,117,122,117,105,120,120,120,120,120,112,117,117,117,130,124,120,120,112,141,110,128,114,128,122,140,110,110,128 },
            { 138,134,117,84,98,113,98,69,69,69,138,128,128,109,94,122,90,94,94,94,66,128,144,144,99,94,128,106,106,106,66,66,144,186,186,105,110,128,128,98,110,110,110,186,120,120,60,130,118,142,142,99,80,116,116,116,116,140,140,143,143,143,143,125,125,125,120,120,120,122,117,117,122,117,97,103,103,99,99,104,120,130,130,130,104,130,99,99,120,145,94,128,90,128,108,134,94,94,128 },
        };

        static int TLAD2TBOGT[3][100] =
        {
            { 107,103,86,120,130,115,130,122,122,138,107,128,128,130,122,109,94,94,94,94,133,128,119,119,108,122,116,138,138,138,133,133,119,155,155,78,98,107,107,138,115,115,115,155,99,99,89,104,136,133,133,104,102,91,91,91,91,104,104,121,121,121,121,117,117,117,96,96,96,109,117,117,109,117,99,107,107,104,104,130,92,99,104,104,124,104,104,104,99,119,122,128,120,128,115,118,122,122,128 },
            { 133,130,116,105,120,122,120,110,110,108,133,128,128,101,110,120,111,111,103,103,103,128,140,140,120,110,140,131,131,131,96,96,140,181,181,100,108,124,126,124,126,126,126,181,112,112,88,117,138,143,143,120,97,125,125,125,125,131,131,133,133,133,142,130,130,130,111,111,111,121,117,117,122,117,105,120,120,120,120,120,112,117,117,117,130,124,120,120,112,141,110,128,114,128,122,140,110,110,128 },
            { 138,134,117,84,98,113,98,94,94,69,138,128,128,125,94,122,122,122,122,122,101,128,144,144,99,94,128,106,106,106,66,66,144,186,186,105,110,128,128,98,128,128,128,186,120,120,60,130,118,142,142,99,80,116,116,116,116,140,140,143,143,143,143,125,125,125,120,120,120,122,117,117,122,117,97,103,103,99,99,104,120,130,130,130,104,130,99,99,120,145,94,128,90,128,108,134,94,94,128 },
        };

        static int TLAD2OFF[3][100] =
        {
            { 133,130,122,108,128,117,119,100,100,105,129,128,128,113,109,105,104,102,109,109,98,128,134,134,111,93,118,112,114,118,98,98,134,174,174,94,105,117,116,111,114,105,105,174,110,110,79,117,131,139,139,108,93,111,111,116,116,125,125,132,132,132,135,124,124,124,109,109,109,117,117,117,118,117,101,110,110,108,108,118,108,115,117,117,119,119,108,108,110,135,109,128,108,128,115,130,109,109,128 },
            { 133,130,122,108,128,117,119,100,100,105,129,128,128,113,109,105,104,102,109,109,98,128,134,134,111,93,118,112,114,118,98,98,134,174,174,94,105,117,116,111,114,105,105,174,110,110,79,117,131,139,139,108,93,111,111,116,116,125,125,132,132,132,135,124,124,124,109,109,109,117,117,117,118,117,101,110,110,108,108,118,108,115,117,117,119,119,108,108,110,135,109,128,108,128,115,130,109,109,128 },
            { 133,130,122,108,128,117,119,100,100,105,129,128,128,113,109,105,104,102,109,109,98,128,134,134,111,93,118,112,114,118,98,98,134,174,174,94,105,117,116,111,114,105,105,174,110,110,79,117,131,139,139,108,93,111,111,116,116,125,125,132,132,132,135,124,124,124,109,109,109,117,117,117,118,117,101,110,110,108,108,118,108,115,117,117,119,119,108,108,110,135,109,128,108,128,115,130,109,109,128 },
        };

        static int TBOGT2IV[3][100] =
        {
            { 107,103,86,120,130,115,130,138,122,138,107,128,128,101,122,109,109,125,122,122,133,128,119,119,108,122,116,138,138,138,133,133,119,155,155,78,98,107,107,107,138,115,115,155,99,99,89,104,136,133,133,104,102,91,91,91,91,104,104,121,121,121,121,117,117,117,96,96,96,109,117,117,109,117,99,107,107,104,104,130,92,99,104,104,124,104,104,104,99,119,122,128,120,128,115,118,122,122,128 },
            { 133,130,116,105,120,122,120,108,110,108,133,128,128,130,110,120,120,110,110,110,96,128,140,140,120,110,140,131,131,131,96,96,140,181,181,100,108,124,124,126,124,126,126,181,112,112,88,117,138,143,143,120,97,125,125,125,125,131,131,133,133,133,142,130,130,130,111,111,111,121,117,117,122,117,105,120,120,120,120,120,112,117,117,117,130,124,120,120,112,141,110,128,114,128,122,140,110,110,128 },
            { 138,134,117,84,98,113,98,69,94,69,138,128,128,109,94,122,122,90,94,94,66,128,144,144,99,94,128,106,106,106,66,66,144,186,186,105,110,128,128,128,98,128,128,186,120,120,60,130,118,142,142,99,80,116,116,116,116,140,140,143,143,143,143,125,125,125,120,120,120,122,117,117,122,117,97,103,103,99,99,104,120,130,130,130,104,130,99,99,120,145,94,128,90,128,108,134,94,94,128 },
        };

        static int TBOGT2TLAD[3][100] =
        {
            { 96,101,92,130,158,161,156,143,140,138,90,128,128,101,122,143,125,133,122,122,133,128,119,119,110,115,138,133,138,148,133,133,119,155,155,78,98,135,130,130,138,98,98,155,99,99,89,104,136,133,133,104,102,91,91,91,91,104,104,121,121,121,121,117,117,117,96,96,96,109,117,117,109,117,99,107,107,104,104,130,92,99,104,104,124,104,104,104,99,119,122,128,120,128,115,118,122,122,128 },
            { 146,142,136,111,133,117,129,104,104,108,143,128,128,130,110,104,109,106,110,110,96,128,140,140,120,101,127,122,125,132,96,96,140,181,181,100,108,127,124,118,119,108,108,181,112,112,88,117,138,143,143,120,97,125,125,125,125,131,131,133,133,133,142,130,130,130,111,111,111,121,117,117,122,117,105,120,120,120,120,120,112,117,117,117,130,124,120,120,112,141,110,128,114,128,122,140,110,110,128 },
            { 156,148,138,83,92,74,73,54,56,69,153,128,128,109,94,69,77,68,94,94,66,128,144,144,102,64,90,82,80,75,66,66,144,186,186,105,110,88,95,86,85,110,110,186,120,120,60,130,118,142,142,99,80,116,116,116,116,140,140,143,143,143,143,125,125,125,120,120,120,122,117,117,122,117,97,103,103,99,99,104,120,130,130,130,104,130,99,99,120,145,94,128,90,128,108,134,94,94,128 },
        };

        static int TBOGT2OFF[3][100] =
        {
            { 126,122,106,103,116,117,116,105,109,105,126,128,128,119,109,117,117,109,109,106,112,128,134,134,109,109,128,125,125,125,98,98,134,174,174,94,105,120,120,120,120,123,123,174,110,110,79,117,131,139,139,108,93,111,111,111,111,125,125,132,132,132,135,124,124,124,109,109,109,117,117,117,117,117,100,110,110,108,108,118,108,115,117,117,119,119,108,108,110,135,109,128,108,128,115,131,109,109,128 },
            { 126,122,106,103,116,117,116,105,109,105,126,128,128,119,109,117,117,109,109,106,112,128,134,134,109,109,128,125,125,125,98,98,134,174,174,94,105,120,120,120,120,123,123,174,110,110,79,117,131,139,139,108,93,111,111,111,111,125,125,132,132,132,135,124,124,124,109,109,109,117,117,117,117,117,100,110,110,108,108,118,108,115,117,117,119,119,108,108,110,135,109,128,108,128,115,131,109,109,128 },
            { 126,122,106,103,116,117,116,105,109,105,126,128,128,119,109,117,117,109,109,106,112,128,134,134,109,109,128,125,125,125,98,98,134,174,174,94,105,120,120,120,120,123,123,174,110,110,79,117,131,139,139,108,93,111,111,111,111,125,125,132,132,132,135,124,124,124,109,109,109,117,117,117,117,117,100,110,110,108,108,118,108,115,117,117,119,119,108,108,110,135,109,128,108,128,115,131,109,109,128 },
        };

        switch (FusionFixSettings("PREF_TIMECYC"))
        {
        case TimecycText.eOFF:
        {
            if (*_dwCurrentEpisode == 0)
            {
                *a40 = IV2OFF[0][scanfCount];
                *a41 = IV2OFF[1][scanfCount];
                *a42 = IV2OFF[2][scanfCount];
            }
            else if (*_dwCurrentEpisode == 1)
            {
                *a40 = TLAD2OFF[0][scanfCount];
                *a41 = TLAD2OFF[1][scanfCount];
                *a42 = TLAD2OFF[2][scanfCount];
            }
            else if(*_dwCurrentEpisode == 2)
            {
                *a40 = TBOGT2OFF[0][scanfCount];
                *a41 = TBOGT2OFF[1][scanfCount];
                *a42 = TBOGT2OFF[2][scanfCount];
            }
        }
        break;
        case TimecycText.eIV:
        {
            if (*_dwCurrentEpisode == 1)
            {
                *a40 = TLAD2IV[0][scanfCount];
                *a41 = TLAD2IV[1][scanfCount];
                *a42 = TLAD2IV[2][scanfCount];
            }
            else if (*_dwCurrentEpisode == 2)
            {
                *a40 = TBOGT2IV[0][scanfCount];
                *a41 = TBOGT2IV[1][scanfCount];
                *a42 = TBOGT2IV[2][scanfCount];
            }
        }
        break;
        case TimecycText.eTLAD:
        {
            if (*_dwCurrentEpisode == 0)
            {
                *a40 = IV2TLAD[0][scanfCount];
                *a41 = IV2TLAD[1][scanfCount];
                *a42 = IV2TLAD[2][scanfCount];
            }
            else if (*_dwCurrentEpisode == 2)
            {
                *a40 = TBOGT2TLAD[0][scanfCount];
                *a41 = TBOGT2TLAD[1][scanfCount];
                *a42 = TBOGT2TLAD[2][scanfCount];
            }
        }
        break;
        case TimecycText.eTBOGT:
        {
            if (*_dwCurrentEpisode == 0)
            {
                *a40 = IV2TBOGT[0][scanfCount];
                *a41 = IV2TBOGT[1][scanfCount];
                *a42 = IV2TBOGT[2][scanfCount];
            }
            else if (*_dwCurrentEpisode == 1)
            {
                *a40 = TLAD2TBOGT[0][scanfCount];
                *a41 = TLAD2TBOGT[1][scanfCount];
                *a42 = TLAD2TBOGT[2][scanfCount];
            }
        }
        break;
        default:
            break;
        }
    }

    scanfCount++;

    if (scanfCount >= 99)
        scanfCount = 0;

    return res;
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
            "ag_bigandbushy", "ag_bigandbushygrn", "ag_tree00", "ag_tree02", "ag_tree06", "azalea_md_ingame",
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
    bool bHandbrakeCamFix = iniReader.ReadInteger("MAIN", "HandbrakeCamFix", 0) != 0;
    int32_t nAimingZoomFix = iniReader.ReadInteger("MAIN", "AimingZoomFix", 1);

    //[SHADOWS]
    bool bFlickeringShadowsFix = iniReader.ReadInteger("SHADOWS", "FlickeringShadowsFix", 1) != 0;
    bExtraDynamicShadows = iniReader.ReadInteger("SHADOWS", "ExtraDynamicShadows", 1);
    bDynamicShadowForTrees = iniReader.ReadInteger("SHADOWS", "DynamicShadowForTrees", 0) != 0;
    static auto bFixCascadedShadowMapResolution = iniReader.ReadInteger("SHADOWS", "FixCascadedShadowMapResolution", 0) != 0;

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
    static auto bFixRainDrops = iniReader.ReadInteger("MISC", "FixRainDrops", 1) != 0;
    static auto nRainDropsBlur = iniReader.ReadInteger("MISC", "RainDropsBlur", 2);
    if (nRainDropsBlur < 1) {
        nRainDropsBlur = 1;
    }
    if (nRainDropsBlur > 4) {
        nRainDropsBlur = 4;
    }
    if (nRainDropsBlur != 1 && nRainDropsBlur != 2 && nRainDropsBlur != 4) {
        nRainDropsBlur = 2;
    }

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

    //if (bDefinitionFix)
    //{
    //    //disable forced definition-off in cutscenes
    //    auto pattern = hook::pattern("E8 ? ? ? ? 0F B6 0D ? ? ? ? 33 D2 84 C0 0F 45 CA 8A C1 C3");
    //    injector::MakeNOP(pattern.count(2).get(1).get<void*>(12), 7, true);
    //}

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
        if (preset > FpsCaps.eCustom && preset < FpsCaps.data.size())
            FpsLimiter.Init(mode, (float)FpsCaps.data[preset]);
        else
        {
            if (preset >= FpsCaps.data.size() || preset < FpsCaps.eOFF)
                FusionFixSettings.Set("PREF_FPS_LIMIT_PRESET", FpsCaps.eOFF);
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
            if (value > FpsCaps.eCustom && value < FpsCaps.data.size())
                FpsLimiter.Init(mode, (float)FpsCaps.data[value]);
            else
                FpsLimiter.Init(mode, fFpsLimit);
        });

        pattern = hook::pattern("80 3D ? ? ? ? ? 53 56 8A FA");
        bLoadscreenShown = *pattern.get_first<uint8_t*>(2);

        pattern = hook::pattern("8B 4C 24 04 8B 54 24 08 8B 41 08");
        injector::MakeJMP(pattern.get_first(0), sub_411F50, true);

        //unlimit loadscreens fps
        //static float f0 = 0.0f;
        //pattern = hook::pattern("F3 0F 10 05 ? ? ? ? 2B C1 1B D6 89 44 24 18 89 54 24 1C DF 6C 24 18 D9 5C 24 18 F3 0F 10 4C 24 ? F3 0F 59 0D ? ? ? ? 0F 2F C1");
        //injector::WriteMemory(pattern.get_first(4), &f0, true);
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
        static auto sub_A95980 = (void(__cdecl*)(unsigned char)) injector::GetBranchDestination(pattern.get_first(7)).get();

        pattern = hook::pattern("83 3D ? ? ? ? ? 75 0F 6A 02");
        _dwCurrentEpisode = *pattern.get_first<int*>(2);

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

    // Console Shadows
    {
        auto pattern = hook::pattern("E8 ? ? ? ? 83 C4 08 57 56");
        fnAE3DE0 = injector::GetBranchDestination(pattern.get_first()).get();
        injector::MakeCALL(pattern.get_first(), sub_AE3DE0, true);
        
        pattern = hook::pattern("55 8B EC 83 E4 F0 83 EC 28 80 3D ? ? ? ? ? 56 57 74 27");
        fnAE3310 = pattern.get_first();
        
        pattern = hook::pattern("E8 ? ? ? ? 8B 3D ? ? ? ? 84 C0 75 59");
        hbsub_924D90.fun = injector::MakeCALL(pattern.get_first(0), static_cast<bool(*)()>(sub_924D90)).get();
        pattern = hook::pattern("E8 ? ? ? ? 8B 0D ? ? ? ? 84 C0 75 4F");
        hbsub_924D90.fun = injector::MakeCALL(pattern.get_first(0), static_cast<bool(*)()>(sub_924D90)).get();
        
        pattern = hook::pattern("E8 ? ? ? ? F3 0F 10 2D ? ? ? ? F3 0F 10 9C 24");
        hbsub_ABCCD0.fun = injector::MakeCALL(pattern.get_first(0), sub_ABCCD0).get();
    }

    //timecycb
    {
        if (FusionFixSettings("PREF_TIMECYC") < TimecycText.eMO_DEF || FusionFixSettings("PREF_TIMECYC") >= std::size(TimecycText.data))
            FusionFixSettings.Set("PREF_TIMECYC", TimecycText.eMO_DEF);

        if (FusionFixSettings("PREF_TCYC_DOF") < DofText.eOff || FusionFixSettings("PREF_TCYC_DOF") >= std::size(DofText.data))
            FusionFixSettings.Set("PREF_TCYC_DOF", DofText.eVeryHigh);

        //auto pattern = hook::pattern("E8 ? ? ? ? 8B F0 83 C4 08 85 F6 0F 84 ? ? ? ? BB");
        //CFileMgrOpenFile = injector::GetBranchDestination(pattern.get_first(0)).get();
        //injector::MakeCALL(pattern.get_first(0), sub_8C4CF0, true);

        auto pattern = hook::pattern("55 8B EC 83 E4 F0 81 EC ? ? ? ? 8B 0D ? ? ? ? 53 0F B7 41 04");
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

        pattern = hook::pattern("E8 ? ? ? ? 0F B6 8C 24 ? ? ? ? 0F B6 84 24");
        injector::MakeCALL(pattern.get_first(0), timecyc_scanf, true);

        FusionFixSettings.SetCallback("PREF_TCYC_DOF", [](int32_t value) {
            injector::fastcall<void()>::call(CTimeCycleInitialise);
            bTimecycUpdated = 200;
        });
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
                auto old = FusionFixSettings(id);

                FusionFixSettings.ForEachPref([&](int32_t prefID, int32_t idStart, int32_t idEnd) {
                    if (prefID == id) {
                        if (value <= idStart) {
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

        //menu scrolling
        pattern = hook::pattern("83 F8 10 7E 37 6A 00 E8 ? ? ? ? 83 C4 04 8D 70 F8 E8 ? ? ? ? D9 5C 24 30");
        injector::WriteMemory<uint8_t>(pattern.get_first(2), 0x10 * 2, true);
        injector::WriteMemory<uint8_t>(pattern.get_first(17), 0xF0, true);
        pattern = hook::pattern("83 FE 10 7F 08");
        injector::WriteMemory<uint8_t>(pattern.get_first(2), 0x10 * 2, true);
        pattern = hook::pattern("83 F8 10 7E 37 6A 00 E8 ? ? ? ? 83 C4 04 8D 70 F8 E8 ? ? ? ? D9 5C 24 38");
        injector::WriteMemory<uint8_t>(pattern.get_first(2), 0x10 * 2, true);
        injector::WriteMemory<uint8_t>(pattern.get_first(17), 0xF0, true);
        pattern = hook::pattern("8D 46 F0 66 0F 6E C0");
        injector::WriteMemory<uint8_t>(pattern.get_first(2), 0xE0, true);
    }

    {
        //static std::map<IDirect3DPixelShader9*, std::tuple<IDirect3DPixelShader9*, IDirect3DPixelShader9*, IDirect3DPixelShader9*, IDirect3DPixelShader9*>> shadermap;
        //auto pattern = hook::pattern("53 57 51 FF 90");
        //struct CreatePixelShaderHook
        //{
        //    void operator()(injector::reg_pack& regs)
        //    {
        //        auto pDevice = (IDirect3DDevice9*)regs.ecx;
        //        DWORD* pFunction = (DWORD*)regs.edi;
        //        IDirect3DPixelShader9** ppShader = (IDirect3DPixelShader9**)regs.ebx;
        //        pDevice->CreatePixelShader(pFunction, ppShader);
        //        IDirect3DPixelShader9* pShader = *ppShader;
        //
        //        if (pShader != nullptr)
        //        {
        //            static std::vector<uint8_t> pbFunc;
        //            UINT len;
        //            pShader->GetFunction(nullptr, &len);
        //            if (pbFunc.size() < len)
        //                pbFunc.resize(len);
        //
        //            pShader->GetFunction(pbFunc.data(), &len);
        //
        //            //def c27, 1, 77, 88, 99            // FXAA Toggle
        //            //def c28, 0, 77, 88, 99            // Console Gamma Toggle
        //            auto pattern = hook::pattern((uintptr_t)pbFunc.data(), (uintptr_t)pbFunc.data() + pbFunc.size(), "05 ? 00 0F A0 ? ? ? ? 00 00 00 00 00 00 B0 42 00 00 C6 42");
        //            if (!pattern.empty())
        //            {
        //                if (!shadermap.contains(pShader))
        //                {
        //                    auto create_shader_variation = [&](int32_t fxaa, int32_t gamma) -> IDirect3DPixelShader9*
        //                    {
        //                        IDirect3DPixelShader9* shader;
        //                        pattern.for_each_result([&](hook::pattern_match match) {
        //                            if (*match.get<uint8_t>(1) == 0x1B) // fxaa
        //                                injector::WriteMemory(match.get<void>(5), fxaa, true);
        //                            else if (*match.get<uint8_t>(1) == 0x1C) // gamma
        //                                injector::WriteMemory(match.get<void>(5), gamma, true);
        //                        });
        //                        pDevice->CreatePixelShader((DWORD*)pbFunc.data(), &shader);
        //                        return shader;
        //                    };
        //
        //                    static constexpr auto ENABLE  = 0x3F800000;
        //                    static constexpr auto DISABLE = 0x00000000;
        //                    auto fxaa_off_gamma_off = create_shader_variation(DISABLE, DISABLE);
        //                    auto fxaa_off_gamma_on = create_shader_variation(DISABLE, ENABLE);
        //                    auto fxaa_on_gamma_off = create_shader_variation(ENABLE, DISABLE);
        //                    auto fxaa_on_gamma_on = create_shader_variation(ENABLE, ENABLE);
        //                    shadermap.emplace(pShader, std::make_tuple(fxaa_off_gamma_off, fxaa_off_gamma_on, fxaa_on_gamma_off, fxaa_on_gamma_on));
        //                }
        //            }
        //        }
        //    }
        //}; injector::MakeInline<CreatePixelShaderHook>(pattern.get_first(0), pattern.get_first(9));

        //auto pattern = hook::pattern("A1 ? ? ? ? 52 8B 08 50 89 15 ? ? ? ? FF 91 ? ? ? ? 8B 44 24 10");
        //static auto pD3DDevice = *pattern.get_first<IDirect3DDevice9**>(1);
        //struct SetPixelShaderHook
        //{
        //    void operator()(injector::reg_pack& regs)
        //    {
        //        regs.eax = *(uint32_t*)pD3DDevice;
        //        auto pDevice = *pD3DDevice;
        //        {
        //            auto pShader = (IDirect3DPixelShader9*)regs.edx;
        //            if (shadermap.contains(pShader))
        //            {
        //                static auto fxaa = FusionFixSettings.GetRef("PREF_FXAA");
        //                static auto gamma = FusionFixSettings.GetRef("PREF_CONSOLE_GAMMA");
        //                if (fxaa && gamma)
        //                {
        //                    if (fxaa->get())
        //                        if (gamma->get())
        //                            regs.edx = (uint32_t)std::get<3>(shadermap.at(pShader));
        //                        else
        //                            regs.edx = (uint32_t)std::get<2>(shadermap.at(pShader));
        //                    else
        //                        if (gamma->get())
        //                            regs.edx = (uint32_t)std::get<1>(shadermap.at(pShader));
        //                        else
        //                            regs.edx = (uint32_t)std::get<0>(shadermap.at(pShader));
        //                }
        //            }
        //        }
        //    }
        //}; injector::MakeInline<SetPixelShaderHook>(pattern.get_first(0));
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
    }

    static UINT oldCascadesWidth = 0;
    static UINT oldCascadesHeight = 0;
    static IDirect3DTexture9** ppHDRTexQuarter = nullptr;
    static IDirect3DTexture9* pHDRTexQuarter = nullptr;

    //sway, z-fighting fix
    {
        //SetRenderState D3DRS_ADAPTIVETESS_X
        {
            auto pattern = hook::pattern("74 ? 68 4E 56 44 42 68");
            injector::WriteMemory<uint8_t>(pattern.get_first(0), 0xEB, true);
        }

        static auto dw1036C00 = *hook::get_pattern<float*>("F3 0F 5C 2D ? ? ? ? F3 0F 10 35", 4);
        static auto dw1036C04 = dw1036C00 + 1;
        static auto dw1036C08 = dw1036C00 + 2;
        static auto dw11A2948 = *hook::get_pattern<float*>("C7 05 ? ? ? ? ? ? ? ? 0F 85 ? ? ? ? 6A 00", 2);
        static auto dw103E49C = *hook::get_pattern<void**>("A1 ? ? ? ? 6A 00 89 74 24 50", 1);
        auto pattern = hook::pattern("8B 80 ? ? ? ? FF 74 24 20 8B 08 53 FF 74 24 20");
        struct SetVertexShaderConstantFHook
        {
            void operator()(injector::reg_pack& regs)
            {
                regs.eax = *(uint32_t*)(regs.eax + 0x11AC);
                auto pD3DDevice = (IDirect3DDevice9*)(regs.eax);
                auto StartRegister = *(UINT*)(regs.esp + 0x18);
                auto pConstantData = (float*)regs.ebx;
                auto Vector4fCount = *(UINT*)(regs.esp + 0x20);

                if (bDynamicShadowForTrees)
                {
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

                if (*dw103E49C)
                {
                    static Cam cam = 0;
                    GET_ROOT_CAM(&cam);
                    if (cam)
                    {
                        static float farclip;
                        static float nearclip;

                        GET_CAM_FAR_CLIP(cam, &farclip);
                        GET_CAM_NEAR_CLIP(cam, &nearclip);

                        static float arr[4];
                        arr[0] = nearclip;
                        arr[1] = farclip;
                        arr[2] = nearclip;
                        arr[3] = farclip;

                        pD3DDevice->SetVertexShaderConstantF(227, &arr[0], 1);
                    }
                }
            }
        }; injector::MakeInline<SetVertexShaderConstantFHook>(pattern.count(2).get(1).get<void*>(0), pattern.count(2).get(1).get<void*>(6));

        struct SetPixelShaderConstantFHook
        {
            void operator()(injector::reg_pack& regs)
            {
                regs.eax = *(uint32_t*)(regs.eax + 0x11AC);

                auto pD3DDevice = (IDirect3DDevice9*)(regs.eax);
                auto StartRegister = *(UINT*)(regs.esp + 0x18);
                auto pConstantData = (float*)regs.ebx;
                auto Vector4fCount = *(UINT*)(regs.esp + 0x20);

                // Definition
                {
                    static auto definition = FusionFixSettings.GetRef("PREF_DEFINITION");
                    static float arr[4];
                    arr[0] = static_cast<float>(definition->get());
                    arr[1] = 0.0f;
                    arr[2] = 0.0f;
                    arr[3] = 0.0f;
                    pD3DDevice->SetPixelShaderConstantF(220, &arr[0], 1);
                }

                {
                    //Tree Translucency
                    static auto tree_lighting = FusionFixSettings.GetRef("PREF_TREE_LIGHTING");
                    static float arr[4];
                    arr[0] = static_cast<float>(tree_lighting->get());
                    arr[1] = 0.0f;
                    arr[2] = 0.0f;
                    arr[3] = 0.0f;
                    pD3DDevice->SetPixelShaderConstantF(221, &arr[0], 1);
                }

                {
                    static auto fxaa = FusionFixSettings.GetRef("PREF_FXAA");
                    static auto dof = FusionFixSettings.GetRef("PREF_TCYC_DOF");
                    static auto cutscene_dof = FusionFixSettings.GetRef("PREF_CUTSCENE_DOF");
                    static auto gamma = FusionFixSettings.GetRef("PREF_CONSOLE_GAMMA");
                    static float arr[4];
                    arr[0] = static_cast<float>(fxaa->get() ? 0 : 1);
                    arr[1] = static_cast<float>(cutscene_dof->get() ? 0 : 1);
                    arr[2] = static_cast<float>(gamma->get());
                    arr[3] = 0.0f;
                    pD3DDevice->SetPixelShaderConstantF(222, &arr[0], 1);
                }

                {
                    static auto shadowFilter = FusionFixSettings.GetRef("PREF_SHADOW_FILTER");
                    static float arr[4];
                    arr[0] = static_cast<float>(shadowFilter->get());
                    arr[1] = static_cast<float>(shadowFilter->get());
                    arr[2] = static_cast<float>(shadowFilter->get());
                    arr[3] = bFixCascadedShadowMapResolution ? 1.0f : 0.0f;
                    pD3DDevice->SetPixelShaderConstantF(223, &arr[0], 1);
                }

                if (bFixCascadedShadowMapResolution) {
                    if (StartRegister == 53 && Vector4fCount == 1 &&
                        oldCascadesWidth != 0 && oldCascadesHeight != 0 &&
                        pConstantData[0] == 1.0f / (float)oldCascadesWidth && pConstantData[1] == 1.0f / (float)oldCascadesHeight)
                    {
                        // set pixel size to pixel shader
                        pConstantData[0] = 1.0f / float(oldCascadesWidth * 2);
                        pConstantData[1] = 1.0f / float(oldCascadesHeight * 2);
                        pConstantData[2] = 1.0f / float(oldCascadesHeight * 2);
                    };
                }
            }
        }; injector::MakeInline<SetPixelShaderConstantFHook>(pattern.count(2).get(0).get<void*>(0), pattern.count(2).get(0).get<void*>(6));

        pattern = hook::pattern("8B 88 ? ? ? ? 8D 7D 40 8B 01 57 FF 75 10 FF 75 24 FF 75 0C FF 75 20 FF 75 18");
        struct CreateTextureHook
        {
            void operator()(injector::reg_pack& regs)
            {
                regs.ecx = *(uint32_t*)(regs.eax + 0x11AC);

                UINT& Width = *(UINT*)(regs.ebp + 0x14);
                UINT& Height = *(UINT*)(regs.ebp + 0x18);
                UINT& Levels = *(UINT*)(regs.ebp + 0x20);
                DWORD& Usage = *(DWORD*)(regs.ebp + 0x0C);
                D3DFORMAT& Format = *(D3DFORMAT*)(regs.ebp + 0x24);
                D3DPOOL& Pool = *(D3DPOOL*)(regs.ebp + 0x10);
                IDirect3DTexture9** ppTexture = (IDirect3DTexture9**)(regs.ebp + 0x40);

                if (bFixCascadedShadowMapResolution)
                {
                    if (Format == D3DFORMAT(D3DFMT_R16F) && Height >= 256 && Width == Height * 4 && Levels == 1)
                    {
                        oldCascadesWidth = Width;
                        oldCascadesHeight = Height;
                
                        Width *= 2;
                        Height *= 2;
                    }
                }

                if (bFixRainDrops && Format == D3DFMT_A16B16G16R16F && Width == (gRect.right - gRect.left) / nRainDropsBlur &&
                    Height == (gRect.bottom - gRect.top) / nRainDropsBlur && ppTexture != nullptr) {
                    ppHDRTexQuarter = ppTexture;
                }
            }
        }; injector::MakeInline<CreateTextureHook>(pattern.get_first(0), pattern.get_first(6));

        pattern = hook::pattern("E9 ? ? ? ? A1 ? ? ? ? 6A 00 8B 88 ? ? ? ? 8D 7D 40 8B 01 57 FF 75 10 FF 75 24 FF 75 0C FF 75 20 FF 75 1C");
        static auto loc_429427 = injector::GetBranchDestination(pattern.get_first(0)).as_int();
        struct CreateTextureHook2
        {
            void operator()(injector::reg_pack& regs)
            {
                if (ppHDRTexQuarter) {
                    pHDRTexQuarter = *ppHDRTexQuarter;
                    ppHDRTexQuarter = nullptr;
                }

                *(uintptr_t*)(regs.esp - 4) = loc_429427;
            }
        }; injector::MakeInline<CreateTextureHook2>(pattern.get_first(0));

        pattern = hook::pattern("8B 82 ? ? ? ? 57 8B 08 56 50 FF 91 ? ? ? ? 5F 5E C2 0C 00");
        struct SetTextureHook
        {
            void operator()(injector::reg_pack& regs)
            {
                regs.eax = *(uint32_t*)(regs.edx + 0x11AC);

                auto Stage = (DWORD)regs.esi;
                auto pTexture = (IDirect3DBaseTexture9*)regs.edi;

                if (bFixRainDrops && Stage == 1 && pTexture == nullptr && pHDRTexQuarter) {
                    regs.edi = (uint32_t)pHDRTexQuarter;
                }
            }
        }; injector::MakeInline<SetTextureHook>(pattern.count(2).get(1).get<void*>(0), pattern.count(2).get(1).get<void*>(6));
    }

    // Make LOD lights appear at the appropriate time like on the console version (consoles: 7 PM, pc: 10 PM)
    {
        auto pattern = hook::pattern("8D 42 13");
        if (!pattern.empty()) injector::WriteMemory<uint8_t>(pattern.get_first(2), 0x10, true);
        pattern = hook::pattern("8D 42 14 3B C8");
        if (!pattern.empty()) injector::WriteMemory<uint8_t>(pattern.get_first(2), 0x10, true);
        if (!pattern.empty()) injector::WriteMemory<uint8_t>(pattern.get_first(8), 0x07, true);
        // Removing episode id check that resulted in flickering LOD lights at certain camera angles in TBOGT
        pattern = hook::pattern("83 3D ? ? ? ? ? 0F 85 ? ? ? ? F3 0F 10 05 ? ? ? ? F3 0F 10 8C 24");
        if (!pattern.empty()) injector::WriteMemory<uint16_t>(pattern.get_first(7), 0xE990, true); // jnz -> jmp
    }

    //if (bFixCascadedShadowMapResolution)
    //{
    //    //crashes randomly
    //    injector::hook_back<void(__cdecl*)(int, int, int)> hbsub_92C840;
    //    void __cdecl sub_92C840(int a1, int a2, int a3)
    //    {
    //        return hbsub_92C840.fun(a1 * 2, a2, a3);
    //    }
    //    auto pattern = hook::pattern("E8 ? ? ? ? 83 C4 0C C3 6A 01 E8");
    //    hbsub_92C840.fun = injector::MakeCALL(pattern.get_first(0), sub_92C840).get();
    //    pattern = hook::pattern("E8 ? ? ? ? 83 C4 0C C3 E9");
    //    hbsub_92C840.fun = injector::MakeCALL(pattern.get_first(0), sub_92C840).get();
    //    
    //    pattern = hook::pattern("03 F6 E8 ? ? ? ? 8B 0D");
    //    injector::MakeNOP(pattern.get_first(0), 2, true);
    //}
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
