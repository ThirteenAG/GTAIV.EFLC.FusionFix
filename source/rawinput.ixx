module;

#include <common.hxx>

export module rawinput;

import common;
import settings;
import comvars;
import natives;

uint32_t pCamPointers;
int32_t* diMouseAxisX;
int32_t* diMouseAxisY;
double __cdecl GetMouseAxisData(int pInput, int32_t requestedAxis)
{
    static auto ri = FusionFixSettings.GetRef("PREF_RAWINPUT");
    if (ri->get())
    {
        if (requestedAxis == 0)
        {
            return (double)(*diMouseAxisX) * (1.0 / 255.0);
        }
        else if (requestedAxis == 1)
        {
            return (double)(*diMouseAxisY) * (1.0 / 255.0);
        }
    }
    else
    {
        if (pInput)
        {
            auto mouseDelta = std::clamp(((float)(uint8_t)(*(uint8_t*)(pInput + 4) ^ *(uint8_t*)(pInput + 6)) - 127.5f) * (1.0f / 127.5f), -1.0f, 1.0f);
            if (fabs(mouseDelta) > (1.0f / 255.0f))
                return requestedAxis == 0 ? (float)*diMouseAxisX * (1.0 / 128.0) : (float)*diMouseAxisY * (1.0 / 128.0);
        }
    }
    return 0.0;
}

double __cdecl GetRIMouseAxisData(int32_t requestedAxis)
{
    return GetMouseAxisData(0, requestedAxis);
}

int __cdecl sub_8EFE40_PlayerCam(int a1, int a2)
{
    static auto ri = FusionFixSettings.GetRef("PREF_RAWINPUT");
    if (ri->get())
    {
        auto pGameCam = *(int32_t*)(pCamPointers + 0xC);
        if (*(uint8_t*)(pGameCam + 0x1C4) & 8)
            return 0;
        return (int)(GetMouseAxisData(a1, a2) * 255.0f);
    }
    return (int)(GetMouseAxisData(a1, a2) * 127.5f);
}

int __cdecl sub_8EFE40(int a1, int a2)
{
    static auto ri = FusionFixSettings.GetRef("PREF_RAWINPUT");
    if (ri->get())
    {
        return (int)(GetMouseAxisData(a1, a2) * 255.0f);
    }
    return (int)(GetMouseAxisData(a1, a2) * 127.5f);
}

float GetMouseSensitivity()
{
    static auto ms = FusionFixSettings.GetRef("PREF_MOUSE_SENSITIVITY");
    auto f = (float)ms->get();
    if (f < 1.0f) f = 1.0f;
    return f;
}

float GetMouseSensitivityForRI()
{
    static auto ms = FusionFixSettings.GetRef("PREF_MOUSE_SENSITIVITY");
    auto f = (float)ms->get();
    if (f < 1.0f) f = 1.0f;
    f /= 10.0f;
    return f;
}

double __cdecl GetMouseAxisData2(int pInput, int32_t requestedAxis)
{
    static auto inv = FusionFixSettings.GetRef("PREF_INVERT_MOUSE");
    return GetMouseAxisData(pInput, requestedAxis) * ((inv->get() && requestedAxis == 1) ? -2.0 : 2.0f);
}

float TryMatchPedCamSensitivity()
{
    return ((GetMouseSensitivityForRI() / 20.0f) * 0.6f) + 0.2f;
}

injector::hook_back<decltype(&Natives::GetMouseInput)> hbNATIVE_GET_MOUSE_INPUT;
void __cdecl NATIVE_GET_MOUSE_INPUT(int* a1, int* a2)
{
    static auto ri = FusionFixSettings.GetRef("PREF_RAWINPUT");
    if (ri->get())
    {
        *a1 = (int)std::clamp(GetRIMouseAxisData(0) * 2000 * TryMatchPedCamSensitivity(), -128.0, 128.0);
        *a2 = (int)std::clamp(GetRIMouseAxisData(1) * 2000 * TryMatchPedCamSensitivity(), -128.0, 128.0);
        return;
    }
    return hbNATIVE_GET_MOUSE_INPUT.fun(a1, a2);
}

class RawInput
{
public:
    RawInput()
    {
        FusionFix::onInitEventAsync() += []()
        {
            // Menu
            auto pattern = hook::pattern("0F 48 C1 A3 ? ? ? ? 5F");
            if (!pattern.empty())
            {
                static auto dword_18B7A8C = *pattern.get_first<int32_t*>(4);
                static auto dword_18B7A80 = dword_18B7A8C - 3;
                struct RawInputMenu
                {
                    void operator()(injector::reg_pack& regs)
                    {
                        static auto ri = FusionFixSettings.GetRef("PREF_RAWINPUT");

                        if (ri->get())
                        {
                            POINT pt;
                            RECT rec;
                            GetClientRect(gWnd, &rec);
                            if (gWnd == GetFocus())
                                ClipCursor(&rec);
                            GetCursorPos(&pt);

                            *dword_18B7A80 = pt.x;
                            *dword_18B7A8C = pt.y;
                        }
                        else
                        {
                            *dword_18B7A8C = regs.eax;
                        }
                    }
                }; injector::MakeInline<RawInputMenu>(pattern.get_first(3));
            }
            else
            {
                pattern = hook::pattern("89 15 ? ? ? ? 5F 5E 5B C3");
                static auto dword_18B7A8C = *pattern.get_first<int32_t*>(2);
                static auto dword_18B7A80 = dword_18B7A8C - 3;
                struct RawInputMenu
                {
                    void operator()(injector::reg_pack& regs)
                    {
                        static auto ri = FusionFixSettings.GetRef("PREF_RAWINPUT");

                        if (ri->get())
                        {
                            POINT pt;
                            RECT rec;
                            GetWindowRect(gWnd, &rec);
                            if (gWnd == GetFocus())
                                ClipCursor(&rec);
                            GetCursorPos(&pt);

                            *dword_18B7A80 = pt.x;
                            *dword_18B7A8C = pt.y;
                        }
                        else
                        {
                            *dword_18B7A8C = regs.edx;
                        }
                    }
                }; injector::MakeInline<RawInputMenu>(pattern.get_first(0), pattern.get_first(6));
            }

            // Mouse Axis
            pattern = hook::pattern("B9 ? ? ? ? E8 ? ? ? ? 85 C0 0F 84 ? ? ? ? F3 0F 10 9C 24 ? ? ? ? F3 0F 10 A4 24 ? ? ? ? F3 0F 10 AC 24");
            if (!pattern.empty())
            {
                pCamPointers = *pattern.get_first<uint32_t>(1);
                pattern = hook::pattern("74 3D C7 05");
                diMouseAxisX = *pattern.get_first<int32_t*>(4);
                diMouseAxisY = *pattern.get_first<int32_t*>(14);
                pattern = hook::pattern("51 8B 54 24 0C C7 04 24 ? ? ? ? 85 D2 75 0D 39 15 ? ? ? ? 75 17 D9 04 24 59 C3");
                injector::MakeJMP(pattern.get_first(0), GetMouseAxisData, true);
            }
            else
            {
                auto pattern = hook::pattern("B9 ? ? ? ? E8 ? ? ? ? 85 C0 0F 84 ? ? ? ? 80 7D 08 00");
                pCamPointers = *pattern.get_first<uint32_t>(1);
                pattern = hook::pattern("53 33 DB 38 5C 24 08 74 26");
                diMouseAxisX = *pattern.get_first<int32_t*>(11);
                diMouseAxisY = *pattern.get_first<int32_t*>(11 + 6);
                pattern = hook::pattern("51 8B 44 24 0C 85 C0 0F 57 D2 56 8B 35");
                injector::MakeJMP(pattern.get_first(0), GetMouseAxisData, true);
            }

            // Ped cam behaves the same way as aiming cam
            pattern = hook::pattern("51 FF 74 24 0C FF 74 24 0C E8 ? ? ? ? D9 5C 24 08");
            if (!pattern.empty()) {
                injector::MakeJMP(pattern.get_first(0), sub_8EFE40_PlayerCam, true);
                pattern = hook::pattern("E8 ? ? ? ? D9 5C 24 20 F3 0F 10 44 24 ? 0F 57 05 ? ? ? ? 83 C4 08 8B CF");
                injector::MakeCALL(pattern.get_first(0), GetMouseAxisData2, true);
                pattern = hook::pattern("E8 ? ? ? ? D9 5C 24 20 F3 0F 10 44 24 ? 0F 57 05 ? ? ? ? 83 C4 08 F3 0F 11 44 24");
                injector::MakeCALL(pattern.get_first(0), GetMouseAxisData2, true);
                pattern = hook::pattern("E8 ? ? ? ? 83 C4 08 89 07 83 3D");
                injector::MakeCALL(pattern.get_first(0), sub_8EFE40, true);
                pattern = hook::pattern("E8 ? ? ? ? 83 C4 08 89 06 5F");
                injector::MakeCALL(pattern.get_first(0), sub_8EFE40, true);
            }
            else {
                pattern = hook::pattern("8B 44 24 08 8B 4C 24 04 50 51 E8 ? ? ? ? D8 0D");
                injector::MakeJMP(pattern.get_first(0), sub_8EFE40_PlayerCam, true);
                pattern = hook::pattern("E8 ? ? ? ? D9 E0 83 C4 08 D9 5C 24 14");
                injector::MakeCALL(pattern.get_first(0), GetMouseAxisData2, true);
                pattern = hook::pattern("E8 ? ? ? ? D9 E0 D9 5C 24 18");
                injector::MakeCALL(pattern.get_first(0), GetMouseAxisData2, true);
                pattern = hook::pattern("E8 ? ? ? ? 83 C4 08 89 06 83 3D");
                injector::MakeCALL(pattern.get_first(0), sub_8EFE40, true);
                pattern = hook::pattern("E8 ? ? ? ? 83 C4 08 89 07");
                injector::MakeCALL(pattern.get_first(0), sub_8EFE40, true);
            }

            // Sniper Camera
            pattern = find_pattern("F3 0F 10 87 ? ? ? ? F3 0F 10 48 ? 83 C4 04", "F3 0F 10 86 ? ? ? ? F3 0F 10 4B ? 83 C4 04");
            static auto reg = *pattern.get_first<uint8_t>(3);
            struct CCamFpsWeaponHook
            {
                void operator()(injector::reg_pack& regs)
                {
                    static auto inv = FusionFixSettings.GetRef("PREF_INVERT_MOUSE");
                    static auto ri = FusionFixSettings.GetRef("PREF_RAWINPUT");
                    auto ptr = regs.edi;
                    if (reg == 0x86) ptr = regs.esi;
                    if (ri->get())
                    {
                        auto fFOVZoomed = *(float*)(ptr + 0x60);
                        static auto fFOVDefault = fFOVZoomed;
                        fFOVDefault = std::max(fFOVDefault, fFOVZoomed);
                        auto fDiff = fFOVDefault / fFOVZoomed;

                        *(float*)(ptr + 0x148) += (-(float)GetRIMouseAxisData(0) * TryMatchPedCamSensitivity()) / fDiff;
                        *(float*)(ptr + 0x144) += (-(float)GetRIMouseAxisData(1) * (inv->get() ? -TryMatchPedCamSensitivity() : TryMatchPedCamSensitivity())) / fDiff;
                    }
                    regs.xmm0.f32[0] = *(float*)(ptr + 0x144);
                }
            }; injector::MakeInline<CCamFpsWeaponHook>(pattern.get_first(0), pattern.get_first(8));

            // First Person Vehicle Camera
            pattern = find_pattern("F3 0F 10 86 ? ? ? ? F3 0F 10 0D ? ? ? ? 0F 2F C8 76 0F F3 0F 10 0D ? ? ? ? 0F 2F C1", "F3 0F 10 87 ? ? ? ? F3 0F 10 0D ? ? ? ? 0F 2F C8 76 0D F3 0F 10 0D ? ? ? ? 0F 2F C1 77 03 0F 28 C1 F3 0F 11 87");
            static auto reg2 = *pattern.get_first<uint8_t>(3);
            struct CCamFollowVehicleHook
            {
                void operator()(injector::reg_pack& regs)
                {
                    static auto inv = FusionFixSettings.GetRef("PREF_INVERT_MOUSE");
                    static auto ri = FusionFixSettings.GetRef("PREF_RAWINPUT");
                    auto ptr = regs.esi;
                    if (reg2 == 0x87) ptr = regs.edi;
                    if (ri->get())
                    {
                        *(float*)(ptr + 0x1B4) += -(float)GetRIMouseAxisData(0) * TryMatchPedCamSensitivity();
                        *(float*)(ptr + 0x1B0) += -(float)GetRIMouseAxisData(1) * (inv->get() ? -TryMatchPedCamSensitivity() : TryMatchPedCamSensitivity());
                    }
                    regs.xmm0.f32[0] = *(float*)(ptr + 0x1B0);
                }
            }; injector::MakeInline<CCamFollowVehicleHook>(pattern.get_first(0), pattern.get_first(8));
        };

        FusionFix::onActivateApp() += [](bool wParam)
        {
            if (!wParam)
            {
                static auto ri = FusionFixSettings.GetRef("PREF_RAWINPUT");

                if (ri->get())
                {
                    ClipCursor(NULL);
                }
            }
        };

        FusionFix::onInitEvent() += []()
        {
            // Script
            hbNATIVE_GET_MOUSE_INPUT.fun = NativeOverride::Register(Natives::NativeHashes::GET_MOUSE_INPUT, NATIVE_GET_MOUSE_INPUT, "E8 ? ? ? ? 83 C4 08 C3", 30);
        };
    }
} RawInput;