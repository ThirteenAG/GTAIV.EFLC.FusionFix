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
        auto pattern = hook::pattern("8B 04 F5 ? ? ? ? 5E C3 8B 04 F5");
        auto pattern2 = hook::pattern("8B 04 F5 ? ? ? ? 50 57 E8 ? ? ? ? 83 C4 08 85 C0 74 7C 83 C6 01");
        auto originalPrefs = *pattern.count(4).get(0).get<MenuPrefs*>(3);
        auto pOriginalPrefsNum = pattern2.get_first<uint32_t>(26);

        for (auto i = 0; originalPrefs[i].prefID < *pOriginalPrefsNum; i++)
        {
            aMenuPrefs.emplace_back(originalPrefs[i].prefID, originalPrefs[i].name);
        }
        
        aMenuPrefs.reserve(aMenuPrefs.size() * 2);
        firstCustomID = aMenuPrefs.back().prefID + 1;

        injector::WriteMemory(pattern.count(4).get(0).get<void*>(3), &aMenuPrefs[0].prefID, true);
        injector::WriteMemory(pattern2.get_first(3), &aMenuPrefs[0].name, true);

        pattern = hook::pattern("89 1C 8D ? ? ? ? E8");
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
			{ 0, "PREF_TCYC_DOF",         "MISC",       "DepthOfField",       1, nullptr },
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
        pattern = hook::pattern("8B 04 F5 ? ? ? ? 5E C3 8B 04 F5");
        pattern2 = hook::pattern("8B 14 F5 ? ? ? ? 52 57 E8");
        auto pattern3 = hook::pattern("83 FE 3C 7C E3 33 C0 5E");
        auto originalEnums = *pattern.count(4).get(2).get<MenuPrefs*>(3);
        auto pOriginalEnumsNum = pattern2.get_first<uint8_t>(26);
        auto pOriginalEnumsNum2 = pattern3.get_first<uint8_t>(2);

        for (auto i = 0; originalEnums[i].prefID < *pOriginalEnumsNum; i++)
        {
            aMenuEnums.emplace_back(originalEnums[i].prefID, originalEnums[i].name);
        }
        aMenuEnums.reserve(aMenuEnums.size() * 2);
        auto firstEnumCustomID = aMenuEnums.back().prefID + 1;

        injector::WriteMemory(pattern.count(4).get(2).get<void*>(3), &aMenuEnums[0].prefID, true);
        injector::WriteMemory(pattern2.get_first(3), &aMenuEnums[0].name, true);
        injector::WriteMemory(pattern3.get_first(12), &aMenuEnums[0].prefID, true);
        injector::WriteMemory(pattern3.get_first(-21), &aMenuEnums[0].name, true);

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
float fScriptCutsceneFpsLimit;
float fScriptCutsceneFovLimit;
std::vector<int32_t> fpsCaps = { 0, 1, 2, 30, 40, 50, 60, 75, 100, 120, 144, 165, 240, 360 };

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
FrameLimiter ScriptCutsceneFpsLimiter;
bool(*CCutscenes__hasCutsceneFinished)();
bool(*CCamera__isWidescreenBordersActive)();
uint8_t* bLoadscreenShown = nullptr;
void __cdecl sub_855640()
{
    static auto preset = FusionFixSettings.GetRef("PREF_FPS_LIMIT_PRESET");

	if (bLoadscreenShown && !*bLoadscreenShown && !bLoadingShown)
	{
		if (preset && *preset >= 2) {
			if (fFpsLimit > 0.0f || (*preset > 2 && *preset < fpsCaps.size()))
				FpsLimiter.Sync();
		}
	}

    if (CCamera__isWidescreenBordersActive())
    {
        if (CCutscenes__hasCutsceneFinished())
            if (fScriptCutsceneFpsLimit)
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

HWND WINAPI CreateWindowExW_Hook(DWORD dwExStyle, LPCWSTR lpClassName, LPCWSTR lpWindowName, DWORD dwStyle, int X, int Y, int nWidth, int nHeight, HWND hWndParent, HMENU hMenu, HINSTANCE hInstance, LPVOID lpParam)
{
    gWnd = CreateWindowExW(dwExStyle, lpClassName, lpWindowName, 0, X, Y, nWidth, nHeight, hWndParent, hMenu, hInstance, lpParam);
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

static int bTimecycUpdated = 0;

void* fnAD0B0 = nullptr;
int __cdecl sub_AD0B0(int a1, int a2)
{
	if (bTimecycUpdated > 0)
	{
		bTimecycUpdated--;
		return 0;
	}
	return injector::cstd<int()>::call(fnAD0B0);
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

	if (FusionFixSettings("PREF_TCYC_DOF") == 0) {
		*a124 = 0.0f;
		*a125 = 0.0f;
	}

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
            if (bExtraDynamicShadows >= 3 || std::any_of(std::begin(modelNames), std::end(modelNames), [](auto& i) { return curModelName.find(i) != std::string::npos; }))
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

    static float& fTimeStep = **hook::get_pattern<float*>("F3 0F 59 44 24 18 83 C4 04", -4);

	// reverse lights fix
	{
		auto pattern = hook::pattern("11 01 00 00 8B 16 8B 42 64 8B CE FF D0 F3 0F 10");
		if (!pattern.empty()) injector::WriteMemory<uint8_t>(pattern.get_first(8), 0x60, true);
	}

	// animation fix for phone interaction on bikes
	{
		auto pattern = hook::pattern("80 BE 18 02 00 00 00 0F 85 36 01 00 00 80 BE");
		if (!pattern.empty()) injector::MakeNOP(pattern.get(0).get<int>(0x21), 6, true);
	}

    //fix for lods appearing inside normal models, unless the graphics menu was opened once (draw distances aren't set properly?)
    {
        auto pattern = hook::pattern("E8 ? ? ? ? 8D 44 24 10 83 C4 04 50");
        auto sub_477300 = injector::GetBranchDestination(pattern.get_first(0)); // sub_19E620
        pattern = hook::pattern("E8 ? ? ? ? E8 ? ? ? ? E8 ? ? ? ? 8B 35 ? ? ? ? E8 ? ? ? ? 25 FF FF 00 00");
        injector::MakeCALL(pattern.get_first(0), sub_477300, true);
    }

    if (bSkipIntro)
    {
		auto pattern = hook::pattern("89 91 ? ? ? ? 8D 44 24 ? 68 ? ? ? ? 50");
		struct Loadsc
		{
			void operator()(injector::reg_pack& regs)
			{
				*(int32_t*)&regs.ecx *= 400;
				if (!bSkipIntroNotNeeded && FusionFixSettings("PREF_SKIP_INTRO") && regs.edx < 8000)
					regs.edx = 0;
			}
		}; injector::MakeInline<Loadsc>(pattern.get_first(-6), pattern.get_first(0));
    }

    //if (bSkipMenu)
    {
        auto pattern = hook::pattern("83 F8 03 75 ? A1 ? ? ? ? 80 88 ? ? ? ? ? 84 DB 74 0A 6A 00 E8 ? ? ? ? 83 C4 04 5F 5E");
		hbsub_7870A0.fun = injector::MakeCALL(pattern.get_first(23), sub_7870A0).get();
    }

    //if (bBorderlessWindowed)
    {
        auto pattern = hook::pattern("FF 15 ? ? ? ? 8B F0 6A 01 56 FF 15");
        injector::MakeNOP(pattern.get_first(0), 6, true);
        injector::MakeCALL(pattern.get_first(0), CreateWindowExW_Hook, true);
        pattern = hook::pattern("FF 15 ? ? ? ? 8B 4C 24 2C 89 3D");
        injector::MakeNOP(pattern.get_first(0), 6, true);
        injector::MakeCALL(pattern.get_first(0), MoveWindow_Hook, true);
        pattern = hook::pattern("FF 15 ? ? ? ? 8B 7D F0 85 DB 8B 5D EC");
        injector::MakeNOP(pattern.get_first(0), 6, true);
        injector::MakeCALL(pattern.get_first(0), AdjustWindowRect_Hook, true);
        pattern = hook::pattern("FF 15 ? ? ? ? 8B 4C 24 14 2B 4C 24 1C");
        injector::MakeNOP(pattern.get_first(0), 6, true);
        injector::MakeCALL(pattern.get_first(0), SetRect_Hook, true);
        pattern = hook::pattern("FF 15 ? ? ? ? 6A 00 68 00 00 CF 00 8D 55 E4");
        injector::MakeNOP(pattern.get_first(0), 6, true);
        injector::MakeCALL(pattern.get_first(0), SetRect_Hook, true);

        FusionFixSettings.SetCallback("PREF_BORDERLESS", [](int32_t value) {
            SwitchWindowStyle();
        });
    }

    //fix for zoom flag in tbogt
    if (nAimingZoomFix)
    {
		auto pattern = hook::pattern("8A D0 32 15");
		static auto& byte_F47AB1 = **pattern.get_first<uint8_t*>(4);
		if (nAimingZoomFix > 1)
			injector::MakeNOP(pattern.get_first(-2), 2, true);
		else if (nAimingZoomFix < 0)
			injector::WriteMemory<uint8_t>(pattern.get_first(-2), 0xEB, true);

		pattern = hook::pattern("08 9E ? ? ? ? E9");
		struct AimZoomHook1
		{
			void operator()(injector::reg_pack& regs)
			{
				*(uint8_t*)(regs.esi + 0x200) |= 1;
				byte_F47AB1 = 1;
			}
		}; injector::MakeInline<AimZoomHook1>(pattern.get_first(0), pattern.get_first(6));

		pattern = hook::pattern("80 A6 ? ? ? ? ? EB 25");
		struct AimZoomHook2
		{
			void operator()(injector::reg_pack& regs)
			{
				*(uint8_t*)(regs.esi + 0x200) &= 0xFE;
				byte_F47AB1 = 0;
			}
		}; injector::MakeInline<AimZoomHook2>(pattern.get_first(0), pattern.get_first(7));

		//let's default to 0 as well
		pattern = hook::pattern("88 1D ? ? ? ? 74 10");
		injector::WriteMemory<uint8_t>(pattern.get_first(1), 0x25, true); //mov ah  
    }

    if (bFlickeringShadowsFix)
    {
		auto pattern = hook::pattern("52 8D 44 24 50 50 68");
		injector::WriteMemory(pattern.get_first(7), 0x100, true);
    }

    if (bRecoilFix)
    {
		static float fRecMult = 0.65f;
		auto pattern = hook::pattern("F3 0F 10 44 24 ? F3 0F 59 05 ? ? ? ? EB ? E8");
		injector::WriteMemory(pattern.get_first(10), &fRecMult, true);
    }

    if (bDefinitionFix)
    {
		//disable forced definition-off in cutscenes
		auto pattern = hook::pattern("E8 ? ? ? ? F6 D8 1A C0 F6 D0 22 05 ? ? ? ? C3"); //0x88CC6B
		injector::WriteMemory<uint16_t>(pattern.get_first(11), 0xA090, true); //mov al ...
    }

    if (bEmissiveShaderFix)
    { 
		// workaround for gta_emissivestrong.fxc lights on patch 6+,
        //"A0 01 00 00 02 00 00 08" replaced in shader files with "A1 01 00 00 02 00 00 08" (5 occurrences)
        auto pattern = hook::pattern("C1 E7 04 C1 E3 04 8B C7 83 F8 04 8D 9B ? ? ? ? 8B CB 72 14 8B 32 3B 31 75 12 83 E8 04");
        struct ShaderTest
        {
            void operator()(injector::reg_pack& regs)
            {
                if (
                    *(uint32_t*)(regs.edx + 0x00) == 0x39D1B717 &&
                    *(uint32_t*)(regs.edx + 0x10) == 0x41000000 && //traffic lights and lamps
                    *(uint32_t*)(regs.edx - 0x10) == 0x3B03126F &&
                    *(uint32_t*)(regs.edx + 0x20) == 0x3E59999A &&
                    *(uint32_t*)(regs.edx + 0x24) == 0x3F372474 &&
                    *(uint32_t*)(regs.edx + 0x28) == 0x3D93A92A
                )
                {
                    auto f_ptr = (uint32_t*)(regs.edx - 0x158);
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
                            *(float*)(regs.edx + 0x00) = -*(float*)(regs.edx + 0x00);
                        }
                    }
                }

                regs.edi = regs.edi << 4;
                regs.ebx = regs.ebx << 4;
            }
        }; injector::MakeInline<ShaderTest>(pattern.count(2).get(1).get<void>(0), pattern.count(2).get(1).get<void>(6)); //417800  
    }

    if (bHandbrakeCamFix)
    {
		static auto GET_POSITION_OF_ANALOGUE_STICKS = hook::get_pattern("6A 00 E8 ? ? ? ? 83 C4 04 80 B8 8C 32 00 00 00 74 78", 0);
		auto pattern = hook::pattern("D9 44 24 20 8B 54 24 64 F3 0F 10 02 51 D9 1C 24 8D 44 24 20 50");
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
					*(float*)(regs.esp + 0x20) *= (1.0f / 30.0f) / fTimeStep;
				float f = *(float*)(regs.esp + 0x20);
				regs.edx = *(uint32_t*)(regs.esp + 0x64);
				_asm fld dword ptr[f]
			}
		}; injector::MakeInline<HandbrakeCam>(pattern.get_first(0), pattern.get_first(8));
    }

    //camera position in TLAD
    if (bDefaultCameraAngleInTLAD)
    {
        static uint32_t episode_id = 0;
		auto pattern = hook::pattern("83 3D ? ? ? ? ? 75 06 C7 00 ? ? ? ? B0 01 C2 08 00");
		injector::WriteMemory(pattern.count(2).get(0).get<void>(2), &episode_id, true);
    }

    if (bPedDeathAnimFixFromTBOGT)
    {
		auto pattern = hook::pattern("BB ? ? ? ? 75 29");
		injector::MakeNOP(pattern.get_first(5), 2, true);
    }

    if (bDisableCameraCenteringInCover)
    {
		static constexpr float xmm_0 = FLT_MAX / 2.0f;
		auto pattern = hook::pattern("F3 0F 10 05 ? ? ? ? F3 0F 58 46 ? 89 8C 24");
		injector::WriteMemory(pattern.get_first(4), &xmm_0, true);
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
        ScriptCutsceneFpsLimiter.Init(mode, fScriptCutsceneFpsLimit);

        auto pattern = hook::pattern("E8 ? ? ? ? 84 C0 75 15 38 05");
        CCutscenes__hasCutsceneFinished = (bool(*)()) injector::GetBranchDestination(pattern.get_first(0)).get();
        pattern = hook::pattern("E8 ? ? ? ? 84 C0 75 42 38 05");
        CCamera__isWidescreenBordersActive = (bool(*)()) injector::GetBranchDestination(pattern.get_first(0)).get();

		pattern = hook::pattern("A1 ? ? ? ? 83 F8 01 8B 0D ? ? ? ? 8B 15 ? ? ? ? 8B 35 ? ? ? ? 74 0D 3B CA");
		injector::WriteMemory(pattern.get_first(0), 0x901CC483, true);
		injector::MakeJMP(pattern.get_first(4), sub_855640, true);

        FusionFixSettings.SetCallback("PREF_FPS_LIMIT_PRESET", [](int32_t value) {
            auto mode = (nFrameLimitType == 2) ? FrameLimiter::FPSLimitMode::FPS_ACCURATE : FrameLimiter::FPSLimitMode::FPS_REALTIME;
            if (value > 2 && value < fpsCaps.size())
                FpsLimiter.Init(mode, (float)fpsCaps[value]);
            else
                FpsLimiter.Init(mode, fFpsLimit);
        });

		pattern = hook::pattern("80 3D ? ? ? ? 00 53 8A 5C 24 1C");
		bLoadscreenShown = *pattern.get_first<uint8_t*>(2);

		pattern = hook::pattern("8B 54 24 04 8B 42 08 85 C0");
		injector::MakeJMP(pattern.get_first(0), sub_411F50, true);
    }

    if (fScriptCutsceneFovLimit)
    {
		auto pattern = hook::pattern("D9 46 0C D9 58 60 5E C3");
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
        auto pattern = hook::pattern("B8 56 55 55 55 F7 E9 8B 0D");
        injector::WriteMemory(*pattern.get_first<void*>(-4), nVehicleBudget, true);
    }

    if (nPedBudget)
    {
        auto pattern = hook::pattern("B8 56 55 55 55 F7 E9 8B 0D");
        injector::WriteMemory(*pattern.get_first<void*>(9), nPedBudget, true);
    }

    {
        //IMG Loader
        auto pattern = hook::pattern("E8 ? ? ? ? 6A 00 E8 ? ? ? ? 83 C4 14 6A 00 B9");
        static auto CImgManager__addImgFile = (void(__cdecl*)(const char*, char, int)) injector::GetBranchDestination(pattern.get_first(0)).get();
        static auto sub_A95980 = (void(__cdecl*)(char)) injector::GetBranchDestination(pattern.get_first(7)).get();

        pattern = hook::pattern("39 35 ? ? ? ? 75 0E 56");
        static auto _dwCurrentEpisode = *pattern.get_first<int*>(2);

        pattern = hook::pattern("89 44 24 44 8B 44 24 4C 57");
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
								return str1.find(str2) != std::wstring::npos;
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

        auto pattern = hook::pattern("E8 ? ? ? ? 8B D8 83 C4 08 85 DB 0F 84 99 0D 00 00");
        CFileMgrOpenFile = injector::GetBranchDestination(pattern.get_first(0)).get();
        injector::MakeCALL(pattern.get_first(0), sub_8C4CF0, true);

        pattern = hook::pattern("55 8B EC 83 E4 F0 81 EC C4 02 00 00 A1 ? ? ? ? 33 C4 89 84 24 C0 02");
        static auto CTimeCycleInitialise = pattern.get_first(0);

        FusionFixSettings.SetCallback("PREF_TIMECYC", [](int32_t value) {
            injector::fastcall<void()>::call(CTimeCycleInitialise);
            bTimecycUpdated = 200;
        });

        // make timecyc changes visible in menu
		// this is a hack for 1080 because the code differs too much and im lazy
        pattern = hook::pattern("E8 ? ? ? ? 84 C0 5F 0F 85 85 00 00 00");
		injector::MakeJMP(pattern.get_first(8), pattern.get_first(23));
        pattern = hook::pattern("E8 ? ? ? ? 84 C0 74 1F A1 ? ? ? ? 69 C0");
		fnAD0B0 = injector::GetBranchDestination(pattern.get_first(0)).get();
        injector::MakeCALL(pattern.get_first(0), sub_AD0B0);
		
		pattern = hook::pattern("E8 ? ? ? ? 0F B6 84 24 10 04 00 00");
		injector::MakeCALL(pattern.get_first(0), timecyc_scanf, true);

		FusionFixSettings.SetCallback("PREF_TCYC_DOF", [](int32_t value) {
			injector::fastcall<void()>::call(CTimeCycleInitialise);
			bTimecycUpdated = 200;
			});
    }

    // runtime settings
    {
        auto pattern = hook::pattern("89 1C 8D ? ? ? ? E8");
        struct IniWriter
        {
            void operator()(injector::reg_pack& regs)
            {
                auto id = regs.ecx;
                auto value = regs.ebx;
                if (FusionFixSettings.isSame(id, "PREF_FPS_LIMIT_PRESET")) {
                    if (value == 1) {
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

        pattern = hook::pattern("8B 1C 8D ? ? ? ? 89 4C 24 18");
        struct MenuTogglesHook1
        {
            void operator()(injector::reg_pack& regs)
            {
                regs.ebx = FusionFixSettings.Get(regs.ecx);
            }
        }; injector::MakeInline<MenuTogglesHook1>(pattern.get_first(0), pattern.get_first(7));

        pattern = hook::pattern("8B 0C 8D ? ? ? ? 75 12");
        struct MenuTogglesHook2
        {
            void operator()(injector::reg_pack& regs)
            {
                regs.ecx = FusionFixSettings.Get(regs.ecx);
            }
        }; injector::MakeInline<MenuTogglesHook2>(pattern.get_first(0), pattern.get_first(7));

        // show game in display menu
        // this seems to make the game not launch on GFWL for some reason, should be looked into further
		if (IsXLivelessPresent())
		{
			pattern = hook::pattern("75 10 57 E8 ? ? ? ? 83 C4 04 83 F8 03");
			injector::WriteMemory(pattern.get_first(), 0x0EEB);
		
			pattern = hook::pattern("83 F8 03 7F 06 B8 01 00 00 00 C3 33 C0 C3");
			injector::MakeNOP(pattern.get_first(3), 2);
		}
    }

    {
        static std::map<IDirect3DPixelShader9*, std::tuple<IDirect3DPixelShader9*, IDirect3DPixelShader9*, IDirect3DPixelShader9*, IDirect3DPixelShader9*>> shadermap;
        auto pattern = hook::pattern("50 8B 82 A8 01 00 00 56 51 FF D0");
        struct CreatePixelShaderHook
        {
            void operator()(injector::reg_pack& regs)
            {
				*(uint32_t*)regs.eax = *(uint32_t*)(regs.edx + 0x1A8);

                auto pDevice = (IDirect3DDevice9*)regs.ecx;
                DWORD* pFunction = (DWORD*)regs.esi;
                IDirect3DPixelShader9** ppShader = (IDirect3DPixelShader9**)regs.eax;
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
        }; injector::MakeInline<CreatePixelShaderHook>(pattern.get_first(0), pattern.get_first(11));

        pattern = hook::pattern("8B 0D ? ? ? ? 8B 11 50 A3 ? ? ? ? 8B 82 AC 01 00 00 51 FF D0");
        static auto pD3DDevice = *pattern.get_first<IDirect3DDevice9**>(2);
        struct SetPixelShaderHook
        {
            void operator()(injector::reg_pack& regs)
            {
                regs.ecx = *(uint32_t*)pD3DDevice;
                auto pDevice = *pD3DDevice;
                {
                    auto pShader = (IDirect3DPixelShader9*)regs.eax;
                    if (shadermap.contains(pShader))
                    {
                        static auto fxaa = FusionFixSettings.GetRef("PREF_FXAA");
                        static auto gamma = FusionFixSettings.GetRef("PREF_CONSOLE_GAMMA");
                        if (fxaa && gamma)
                        {
                            if (fxaa->get())
                                if (gamma->get())
                                    regs.eax = (uint32_t)std::get<3>(shadermap.at(pShader));
                                else
                                    regs.eax = (uint32_t)std::get<2>(shadermap.at(pShader));
                            else
                                if (gamma->get())
                                    regs.eax = (uint32_t)std::get<1>(shadermap.at(pShader));
                                else
                                    regs.eax = (uint32_t)std::get<0>(shadermap.at(pShader));
                        }
                    }
                }
            }
        }; injector::MakeInline<SetPixelShaderHook>(pattern.get_first(0), pattern.get_first(6));
    }

    if (bExtraDynamicShadows || bDynamicShadowForTrees)
    {
        auto pattern = hook::pattern("E8 ? ? ? ? EB 0E 09 4C 24 08");
        CModelInfoStore__allocateBaseModel = injector::GetBranchDestination(pattern.get_first(0));
        injector::MakeCALL(pattern.get_first(0), CModelInfoStore__allocateBaseModelHook, true);
        CModelInfoStore__allocateInstanceModel = injector::GetBranchDestination(pattern.get_first(16));
        injector::MakeCALL(pattern.get_first(16), CModelInfoStore__allocateInstanceModelHook, true);
        pattern = hook::pattern("E8 ? ? ? ? F3 0F 10 44 24 40 F3 0F 10 4C 24 44 F3 0F 10 54 24 3C");
        CBaseModelInfo__setFlags = injector::GetBranchDestination(pattern.get_first(0));
        injector::MakeCALL(pattern.get_first(0), CBaseModelInfo__setFlagsHook, true);

        std::vector<std::string> vegetationNames = {
            "bush", "weed", "grass", "azalea", "bholly", "fern", "tree"
        };

        if (bExtraDynamicShadows == 2)
            modelNames.insert(modelNames.end(), vegetationNames.begin(), vegetationNames.end());

        //sway
        if (bDynamicShadowForTrees)
        {
            static auto dw1036C00 = *hook::get_pattern<float*>("F3 0F 10 1D ? ? ? ? F3 0F 10 2D ? ? ? ? F3 0F 10 05", 4);
            static auto dw1036C04 = dw1036C00 + 1;
            static auto dw1036C08 = dw1036C00 + 2;
            static auto dw11A2948 = *hook::get_pattern<float*>("F3 0F 11 05 ? ? ? ? 0F 85 04 03 00 00", 4);
            pattern = hook::pattern("8B 81 AC 11 00 00 8B 4C 24 2C 8B 10 8B 92 78 01 00 00 83 C4 0C 51");
            struct SetVertexShaderConstantFHook
            {
                void operator()(injector::reg_pack& regs)
                {
                    regs.eax = *(uint32_t*)(regs.ecx + 0x11AC);
                    auto pD3DDevice = (IDirect3DDevice9*)(regs.ecx);
                    auto StartRegister = *(UINT*)(regs.esp + 0x24);
                    auto pConstantData = (float*)regs.esi;
                    auto Vector4fCount = *(UINT*)(regs.esp + 0x2C);

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
            }; injector::MakeInline<SetVertexShaderConstantFHook>(pattern.get_first(0), pattern.get_first(6));
        }
    }

    // Make LOD lights appear at the appropriate time like on the console versions (consoles: 7 PM, pc: 10 PM)
    {
        auto pattern = hook::pattern("8D 51 13");
        if (!pattern.empty()) injector::WriteMemory<uint8_t>(pattern.get_first(2), 0x10, true);
        pattern = hook::pattern("83 C1 14 3B C1");
        if (!pattern.empty()) injector::WriteMemory<uint8_t>(pattern.get_first(2), 0x10, true);
        if (!pattern.empty()) injector::WriteMemory<uint8_t>(pattern.get_first(8), 0x07, true);
        // Removing episode id check that resulted in flickering LOD lights at certain camera angles in TBOGT
        pattern = hook::pattern("83 3D ? ? ? ? ? 75 68 F3 0F 10 05 ? ? ? ?");
        if (!pattern.empty()) injector::MakeNOP(pattern.get_first(0), 113, true);
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
