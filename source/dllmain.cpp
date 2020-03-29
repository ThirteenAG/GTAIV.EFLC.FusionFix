#include <misc.h>

uint32_t* dword_11CC9D0;
int32_t* dword_112EAC0;
int32_t* dword_11402D4;
int32_t* dword_F30468;
float* float_F33C18;
LARGE_INTEGER(*sub_456F60)();
int64_t* qword_11CCA80;
float* float_18CAE9C;
void* unk_11CC9D8;
double* qword_10FCB84;
float* float_F33C24;
float* float_11CC9D4;
void(__cdecl* sub_855470)(float a1);
bool(*CCutscenes__hasCutsceneFinished)();
bool(*CCamera__isWidescreenBordersActive)();

int32_t nFrameLimitType;
float fFpsLimit;
float fCutsceneFpsLimit;
float fScriptCutsceneFpsLimit;
float fScriptCutsceneFovLimit;

void __cdecl sub_855640()
{
    //injector::fastcall<void(uintptr_t)>::call(*(uintptr_t*)(*(uintptr_t*)(*dword_11CC9D0) + 20), *dword_11CC9D0);

    static auto wants_to_limit_fps = []() -> bool
    {
        if (fFpsLimit)
            return true;

        if (CCamera__isWidescreenBordersActive())
        {
            if (CCutscenes__hasCutsceneFinished())
            {
                if (fCutsceneFpsLimit)
                    return true;
            }
            else if (fScriptCutsceneFpsLimit)
                return true;
        }

        return false;
    };

    static auto get_fps_limit = []() -> float
    {
        float limit = 30.0f;

        if (fFpsLimit)
            limit = fFpsLimit;

        if (CCamera__isWidescreenBordersActive())
        {
            if (CCutscenes__hasCutsceneFinished())
            {
                if (fCutsceneFpsLimit)
                    limit = fCutsceneFpsLimit;
            }
            else if (fScriptCutsceneFpsLimit)
                limit = fScriptCutsceneFpsLimit;
        }

        return limit;
    };

    if (wants_to_limit_fps() || *dword_112EAC0 == 1 || *dword_11402D4 != -1 || *dword_F30468 == 18)
    {
        if (nFrameLimitType == 0)
        {
            *float_F33C18 = 2.0f;
            if (*dword_112EAC0 == 1 || *dword_11402D4 != -1 || *dword_F30468 == 18)
                *float_F33C18 = 1.0f;
            double v1 = (double)((*float_F33C18 - 0.1f) * ((1.0f / get_fps_limit()) / 2.0f));
            double v3 = (double)(sub_456F60().QuadPart - *qword_11CCA80);
            if (v1 > v3 * *float_18CAE9C)
            {
                do
                {
                    v3 = (double)(sub_456F60().QuadPart - *qword_11CCA80) * *float_18CAE9C;
                    Sleep(0);
                }
                while (v1 > v3);
            }
            *qword_11CCA80 = sub_456F60().QuadPart;
        }
        else
        {
            static LARGE_INTEGER PerformanceCount1;
            static LARGE_INTEGER PerformanceCount2;
            static bool bOnce1 = false;
            static double t = 0.0;
            static DWORD i = 0;
            double targetFrameTime = 1000.0 / get_fps_limit();

            if (!bOnce1)
            {
                bOnce1 = true;
                QueryPerformanceCounter(&PerformanceCount1);
                PerformanceCount1.QuadPart = PerformanceCount1.QuadPart >> i;
            }

            while (true)
            {
                QueryPerformanceCounter(&PerformanceCount2);
                if (t == 0.0)
                {
                    LARGE_INTEGER PerformanceCount3;
                    static bool bOnce2 = false;

                    if (!bOnce2)
                    {
                        bOnce2 = true;
                        QueryPerformanceFrequency(&PerformanceCount3);
                        i = 0;
                        t = 1000.0 / (double)PerformanceCount3.QuadPart;
                        auto v = t * 2147483648.0;
                        if (60000.0 > v)
                        {
                            while (true)
                            {
                                ++i;
                                v *= 2.0;
                                t *= 2.0;
                                if (60000.0 <= v)
                                    break;
                            }
                        }
                    }
                    SleepEx(0, 1);
                    break;
                }

                if (((double)((PerformanceCount2.QuadPart >> i) - PerformanceCount1.QuadPart) * t) >= targetFrameTime)
                    break;

                SleepEx(0, 1);
            }
            QueryPerformanceCounter(&PerformanceCount2);
            PerformanceCount1.QuadPart = PerformanceCount2.QuadPart >> i;
        }
    }

    //is that even used?
    {
        auto v2 = 0.0f;
        auto v01 = (uint8_t **)unk_11CC9D8;
        auto v02 = v01[2];
        if (v02 && *v02)
            v2 = atof((const char*)v01[2]);

        if (v2 > 0.0f)
        {
            auto v0 = *qword_10FCB84;
            if (*qword_10FCB84 < 0.0083333002f)
                v0 = 0.0083333002f;
            *float_F33C24 = (((60.0f - 1.0f) * (1.0f / 60.0f)) * *float_F33C24) + ((1.0f / v0) * (1.0f / 60.0f));
            if ((v2 - 3.0f) <= *float_F33C24)
            {
                if (*float_F33C24 > (v2 + 3.0f))
                {
                    sub_855470(1.0 + 0.029999999f);
                    *float_11CC9D4 = 1.0f;
                }
            }
            else
            {
                sub_855470(1.0f - 0.029999999f);
                *float_11CC9D4 = -1.0f;
            }
        }
    }
}

injector::hook_back<void(__cdecl*)(int)> hbsub_7870A0;
void __cdecl sub_7870A0(int a1)
{
    static bool bOnce = false;
    if (!bOnce)
    {
        if (a1 == 0)
        {
            bool bNoLoad = (GetAsyncKeyState(VK_SHIFT) & 0xF000) != 0;
            if (!bNoLoad)
                a1 = 6;

            bOnce = true;
        }
    }
    return hbsub_7870A0.fun(a1);
}

uint32_t* grcWindow__m_dwWidth;
uint32_t* grcWindow__m_dwHeight;
void MakeBorderless(HWND hWnd)
{
    if (grcWindow__m_dwWidth && grcWindow__m_dwHeight)
    {
        if (*grcWindow__m_dwWidth && *grcWindow__m_dwHeight)
        {
            HMONITOR monitor = MonitorFromWindow(GetDesktopWindow(), MONITOR_DEFAULTTONEAREST);
            MONITORINFO info = {};
            info.cbSize = sizeof(MONITORINFO);
            GetMonitorInfo(monitor, &info);
            int32_t DesktopResW = info.rcMonitor.right - info.rcMonitor.left;
            int32_t DesktopResH = info.rcMonitor.bottom - info.rcMonitor.top;

            RECT rect;
            rect.left = (LONG)(((float)DesktopResW / 2.0f) - ((float)*grcWindow__m_dwWidth / 2.0f));
            rect.top = (LONG)(((float)DesktopResH / 2.0f) - ((float)*grcWindow__m_dwHeight / 2.0f));
            rect.right = *grcWindow__m_dwWidth;
            rect.bottom = *grcWindow__m_dwHeight;
            SetWindowLong(hWnd, GWL_STYLE, GetWindowLong(hWnd, GWL_STYLE) & ~WS_OVERLAPPEDWINDOW);
            SetWindowPos(hWnd, NULL, rect.left, rect.top, rect.right, rect.bottom, SWP_NOACTIVATE | SWP_NOZORDER);
        }
    }
}

LRESULT WINAPI DefWindowProcAProxy(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam)
{
    if (Msg == WM_PAINT)
        MakeBorderless(hWnd);

    return DefWindowProcA(hWnd, Msg, wParam, lParam);
}

LRESULT WINAPI DefWindowProcWProxy(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam)
{
    if (Msg == WM_PAINT)
        MakeBorderless(hWnd);

    return DefWindowProcW(hWnd, Msg, wParam, lParam);
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

void Init()
{
    CIniReader iniReader("");
    //[MAIN]
    bool bSkipIntro = iniReader.ReadInteger("MAIN", "SkipIntro", 1) != 0;
    bool bSkipMenu = iniReader.ReadInteger("MAIN", "SkipMenu", 1) != 0;
    bool bBorderlessWindowed = iniReader.ReadInteger("MAIN", "BorderlessWindowed", 1) != 0;
    bool bRecoilFix = iniReader.ReadInteger("MAIN", "RecoilFix", 1) != 0;
    bool bDefinitionFix = iniReader.ReadInteger("MAIN", "DefinitionFix", 1) != 0;
    bool bEmissiveShaderFix = iniReader.ReadInteger("MAIN", "EmissiveShaderFix", 1) != 0;
    bool bHandbrakeCamFix = iniReader.ReadInteger("MAIN", "HandbrakeCamFix", 1) != 0;
    int32_t nAimingZoomFix = iniReader.ReadInteger("MAIN", "AimingZoomFix", 1);
    bool bFlickeringShadowsFix = iniReader.ReadInteger("MAIN", "FlickeringShadowsFix", 1) != 0;

    //[FRAMELIMIT]
    nFrameLimitType = iniReader.ReadInteger("FRAMELIMIT", "FrameLimitType", 1);
    fFpsLimit = static_cast<float>(iniReader.ReadInteger("FRAMELIMIT", "FpsLimit", 0));
    fCutsceneFpsLimit = static_cast<float>(iniReader.ReadInteger("FRAMELIMIT", "CutsceneFpsLimit", 0));
    fScriptCutsceneFpsLimit = static_cast<float>(iniReader.ReadInteger("FRAMELIMIT", "ScriptCutsceneFpsLimit", 0));
    fScriptCutsceneFovLimit = static_cast<float>(iniReader.ReadInteger("FRAMELIMIT", "ScriptCutsceneFovLimit", 0));

    //[MISC]
    bool bDefaultCameraAngleInTLAD = iniReader.ReadInteger("MISC", "DefaultCameraAngleInTLAD", 0) != 0;
    bool bPedDeathAnimFixFromTBOGT = iniReader.ReadInteger("MISC", "PedDeathAnimFixFromTBOGT", 1) != 0;
    bool bDisableCameraCenteringInCover = iniReader.ReadInteger("MISC", "DisableCameraCenteringInCover", 1) != 0;
    bool bMouseFix = iniReader.ReadInteger("MISC", "MouseFix", 0) != 0;

    static float& fTimeStep = **hook::get_pattern<float*>("F3 0F 10 05 ? ? ? ? F3 0F 59 05 ? ? ? ? 8B 43 20 53", 4);

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
                if (*(int32_t*)&regs.eax < 8000)
                    regs.eax = 0;
            }
        }; injector::MakeInline<Loadsc>(pattern.count(2).get(1).get<void*>(0), pattern.count(2).get(1).get<void*>(10));
    }

    if (bSkipMenu)
    {
        auto pattern = hook::pattern("E8 ? ? ? ? 83 C4 04 8B 8C 24 ? ? ? ? 5F 5E 5D 5B 33 CC E8 ? ? ? ? 81 C4 ? ? ? ? C3");
        hbsub_7870A0.fun = injector::MakeCALL(pattern.count(5).get(1).get<void*>(0), sub_7870A0).get();
    }

    if (bBorderlessWindowed)
    {
        grcWindow__m_dwWidth = *hook::get_pattern<uint32_t*>("8B 0D ? ? ? ? F3 0F 10 8C B7 ? ? ? ? F3 0F 59 C2 84 C0 0F 45 0D ? ? ? ? F3 0F 5C C8 66 0F 6E C1 0F 5B C0 F3 0F 59 C8 0F 57 C0 0F 2F C1", 2);
        grcWindow__m_dwHeight = *hook::get_pattern<uint32_t*>("8B 0D ? ? ? ? F3 0F 10 94 B7 ? ? ? ? F3 0F 59 C1 84 C0 0F 45 0D ? ? ? ? F3 0F 5C D0 66 0F 6E C1 0F 5B C0 F3 0F 59 D0 0F 57 C0 0F 2F C2", 2);
        auto pattern = hook::pattern("FF 15 ? ? ? ? 5F 5E 5D 5B 83 C4 10 C2 10 00");
        injector::MakeNOP(pattern.count(2).get(0).get<void>(0), 6, true);
        injector::MakeCALL(pattern.count(2).get(0).get<void>(0), DefWindowProcWProxy, true);
        injector::MakeNOP(pattern.count(2).get(1).get<void>(0), 6, true);
        injector::MakeCALL(pattern.count(2).get(1).get<void>(0), DefWindowProcAProxy, true);
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

        pattern = hook::pattern("80 A6 ? ? ? ? ? 5B 5F 5E C2 14 00");
        struct AimZoomHook2
        {
            void operator()(injector::reg_pack& regs)
            {
                *(uint8_t*)(regs.esi + 0x200) &= 0xFE;
                byte_F47AB1 = 0;
            }
        }; injector::MakeInline<AimZoomHook2>(pattern.get_first(0), pattern.get_first(7));

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
        auto pattern = hook::pattern("F3 0F 10 44 24 ? F3 0F 11 04 24 8D 44 24 34 50 8D 44 24 28 50 53 8B CF");
        struct HandbrakeCam
        {
            void operator()(injector::reg_pack& regs)
            {
                *(float*)(regs.esp + 0x2C) *= (1.0f / 30.0f) / fTimeStep;
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
    }

    if (fFpsLimit || fCutsceneFpsLimit || fScriptCutsceneFpsLimit)
    {
        dword_11CC9D0 = *hook::get_pattern<uint32_t*>("8B 0D ? ? ? ? 83 EC 18 8B 01 56 FF 50 14", 2);
        float_11CC9D4 = (float*)(dword_11CC9D0 + 1);
        unk_11CC9D8 = dword_11CC9D0 + 2;
        dword_112EAC0 = *hook::get_pattern<int32_t*>("83 3D ? ? ? ? ? F3 0F 10 15 ? ? ? ? 8B 15 ? ? ? ? 8B 35 ? ? ? ? A1 ? ? ? ? 0F 28 CA", 2);
        dword_11402D4 = *hook::get_pattern<int32_t*>("A1 ? ? ? ? 3B 05 ? ? ? ? 0F 85 ? ? ? ? 57 B9", 1);
        dword_F30468 = *hook::get_pattern<int32_t*>("83 3D ? ? ? ? ? 0F 84 ? ? ? ? A1 ? ? ? ? A8 01", 2);
        float_F33C18 = *hook::get_pattern<float*>("F3 0F 11 05 ? ? ? ? 83 FE 01 74 09 3B CA 75 05 83 F8 12 75 10", 4);
        float_F33C24 = *hook::get_pattern<float*>("F3 0F 59 1D ? ? ? ? F3 0F 58 D8 0F 28 C5 F3 0F 5C C1 F3 0F 11 1D ? ? ? ? 0F 2F C3", 4);
        qword_11CCA80 = *hook::get_pattern<int64_t*>("A3 ? ? ? ? 89 15 ? ? ? ? 8D 44 24 08", 1);
        float_18CAE9C = *hook::get_pattern<float*>("89 5D E4 F3 0F 10 05", 7);
        qword_10FCB84 = *hook::get_pattern<double*>("F3 0F 10 05 ? ? ? ? 0F 2F E0 77 03 0F 28 E0 F3 0F 10 15 ? ? ? ? F3 0F 10 1D", 2);
        sub_456F60 = (LARGE_INTEGER(*)()) hook::get_pattern<void*>("55 8B EC 83 E4 F8 83 EC 18 83 3D ? ? ? ? ? 53 55 56 8B 35 ? ? ? ? 57", 0);
        sub_855470 = (void(*)(float)) hook::get_pattern<void*>("F3 0F 10 05 ? ? ? ? F3 0F 59 44 24 ? F3 0F 10 0D ? ? ? ? 66 0F 6E 1D", 0);

        auto pattern = hook::pattern("E8 ? ? ? ? 84 C0 75 89");
        CCutscenes__hasCutsceneFinished = (bool(*)()) injector::GetBranchDestination(pattern.get_first(0)).get();
        pattern = hook::pattern("E8 ? ? ? ? 84 C0 75 44 38 05 ? ? ? ? 74 26");
        CCamera__isWidescreenBordersActive = (bool(*)()) injector::GetBranchDestination(pattern.get_first(0)).get();

        pattern = hook::pattern("8B 35 ? ? ? ? 8B 0D ? ? ? ? 8B 15 ? ? ? ? A1");
        injector::WriteMemory(pattern.get_first(0), 0x901CC483, true); //nop + add esp,1C
        injector::MakeJMP(pattern.get_first(4), sub_855640, true); // + jmp
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
    return TRUE;
}