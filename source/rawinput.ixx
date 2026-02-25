module;

#include <common.hxx>
#include <cmath>
#include <regex>

export module rawinput;

import common;
import comvars;
import natives;
import settings;

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

double __cdecl GetMouseAxisData2(int pInput, int32_t requestedAxis)
{
    static auto inv = FusionFixSettings.GetRef("PREF_INVERT_MOUSE");
    return GetMouseAxisData(pInput, requestedAxis) * ((inv->get() && requestedAxis == 1) ? -2.0 : 2.0f);
}

injector::hook_back<decltype(&Natives::GetMouseInput)> hbNATIVE_GET_MOUSE_INPUT;
POINT centerPos = { 0, 0 };
bool centerInitialized = false;
void __cdecl NATIVE_GET_MOUSE_INPUT(int* a1, int* a2)
{
    static auto ri = FusionFixSettings.GetRef("PREF_RAWINPUT");
    if (ri->get())
    {
        if (gWnd && GetFocus() == gWnd)
        {
            POINT currentPos;
            GetCursorPos(&currentPos);

            if (!centerInitialized)
            {
                RECT rect;
                GetWindowRect(gWnd, &rect);
                centerPos.x = (rect.left + rect.right) / 2;
                centerPos.y = (rect.top + rect.bottom) / 2;
                centerInitialized = true;
            }

            int deltaX = currentPos.x - centerPos.x;
            int deltaY = currentPos.y - centerPos.y;

            *a1 = (int)std::clamp((double)deltaX, -128.0, 128.0);
            *a2 = (int)std::clamp((double)deltaY, -128.0, 128.0);

            SetCursorPos(centerPos.x, centerPos.y);
            return;
        }
    }
    return hbNATIVE_GET_MOUSE_INPUT.fun(a1, a2);
}

constexpr float fMouseAimSensitivityScaler = 21.0f;
constexpr float fGamepadAimSensitivityScaler = 21.0f;

float fMouseLookSensitivityMin = 0.1f;
float fMouseLookSensitivityMax = 2.0f;
float fGamepadLookSensitivityMin = 0.1f;
float fGamepadLookSensitivityMax = 2.0f;
float fMouseAimSensitivityMin = 0.1f;
float fMouseAimSensitivityMax = 2.0f;
float fGamepadAimSensitivityMin = 0.1f;
float fGamepadAimSensitivityMax = 2.0f;

inline float GetMouseLookSensitivity()
{
    static auto MouseSensitivity = FusionFixSettings.GetRef("PREF_MOUSE_SENSITIVITY");
    float sliderValue = (float)MouseSensitivity->get();
    float sliderMax = fMouseAimSensitivityScaler;

    // Linear mapping: slider(0..20) -> [Min, Max]
    float t = std::clamp(sliderValue / sliderMax, 0.0f, 1.0f);
    return std::lerp(fMouseLookSensitivityMin, fMouseLookSensitivityMax, t) * 0.1f;
}

inline float GetMouseAimSensitivity()
{
    static auto MouseAimSensitivity = FusionFixSettings.GetRef("PREF_MOUSEAIMSENSITIVITY");
    float sliderValue = (float)MouseAimSensitivity->get();
    float sliderMax = fMouseAimSensitivityScaler; // 21.0f

    float t = std::clamp(sliderValue / sliderMax, 0.0f, 1.0f);
    return std::lerp(fMouseAimSensitivityMin, fMouseAimSensitivityMax, t) * 0.1f;
}

inline float GetGamepadLookSensitivity()
{
    static auto GamepadLookSensitivity = FusionFixSettings.GetRef("PREF_PADLOOKSENSITIVITY");
    float sliderValue = (float)GamepadLookSensitivity->get();
    float sliderMax = fGamepadAimSensitivityScaler; // 21.0f

    float t = std::clamp(sliderValue / sliderMax, 0.0f, 1.0f);
    return std::lerp(fGamepadLookSensitivityMin, fGamepadLookSensitivityMax, t);
}

inline float GetGamepadAimSensitivity()
{
    static auto GamepadAimSensitivity = FusionFixSettings.GetRef("PREF_PADAIMSENSITIVITY");
    float sliderValue = (float)GamepadAimSensitivity->get();
    float sliderMax = fGamepadAimSensitivityScaler; // 21.0f

    float t = std::clamp(sliderValue / sliderMax, 0.0f, 1.0f);
    return std::lerp(fGamepadAimSensitivityMin, fGamepadAimSensitivityMax, t);
}

class RawInput
{
public:
    RawInput()
    {
        FusionFix::onInitEventAsync() += []()
        {
            CIniReader iniReader("");
            auto parseRange = [](const std::string& rangeStr, float defaultMin, float defaultMax) -> std::pair<float, float>
            {
                std::regex floatRegex(R"(-?\d+(\.\d+)?)");
                std::sregex_iterator iter(rangeStr.begin(), rangeStr.end(), floatRegex);
                std::sregex_iterator end;
                std::vector<float> numbers;

                for (; iter != end; ++iter)
                {
                    try
                    {
                        numbers.push_back(std::stof(iter->str()));
                    } catch (...)
                    {
                        // Skip invalid matches
                    }
                }

                if (numbers.size() >= 2)
                {
                    return { numbers[0], numbers[1] };
                }
                return { defaultMin, defaultMax };
            };

            std::tie(fMouseLookSensitivityMin, fMouseLookSensitivityMax) = parseRange(iniReader.ReadString("CAMERASENSITIVITY", "MouseLookSensitivityRange", "0.1,2.0"), 0.1f, 2.0f);
            std::tie(fGamepadLookSensitivityMin, fGamepadLookSensitivityMax) = parseRange(iniReader.ReadString("CAMERASENSITIVITY", "GamepadLookSensitivityRange", "0.1,2.0"), 0.1f, 2.0f);
            std::tie(fMouseAimSensitivityMin, fMouseAimSensitivityMax) = parseRange(iniReader.ReadString("CAMERASENSITIVITY", "MouseAimSensitivityRange", "0.1,2.0"), 0.1f, 2.0f);
            std::tie(fGamepadAimSensitivityMin, fGamepadAimSensitivityMax) = parseRange(iniReader.ReadString("CAMERASENSITIVITY", "GamepadAimSensitivityRange", "0.1,2.0"), 0.1f, 2.0f);

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
            if (!pattern.empty())
            {
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
            else
            {
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

                        auto TryMatchAimCamSensitivity = []() -> float
                        {
                            return GetMouseAimSensitivity();
                        };

                        *(float*)(ptr + 0x148) += (-(float)GetRIMouseAxisData(0) * TryMatchAimCamSensitivity()) / fDiff;
                        *(float*)(ptr + 0x144) += (-(float)GetRIMouseAxisData(1) * (inv->get() ? -TryMatchAimCamSensitivity() : TryMatchAimCamSensitivity())) / fDiff;
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
                        *(float*)(ptr + 0x1B4) += -(float)GetRIMouseAxisData(0) * GetMouseLookSensitivity();
                        *(float*)(ptr + 0x1B0) += -(float)GetRIMouseAxisData(1) * (inv->get() ? -GetMouseLookSensitivity() : GetMouseLookSensitivity());
                    }
                    regs.xmm0.f32[0] = *(float*)(ptr + 0x1B0);
                }
            }; injector::MakeInline<CCamFollowVehicleHook>(pattern.get_first(0), pattern.get_first(8));

            // Mouse look sensitivity slider and multipliers
            {
                // CCamFollowPed
                auto pattern = hook::pattern("F3 0F 11 54 24 ? F3 0F 11 5C 24 ? EB ? F3 0F 10 4C 24 ? 0F 54 CC");
                if (!pattern.empty())
                {
                    injector::MakeNOP(pattern.get_first(0), 12, true);
                    static auto CCamFollowPed_MouseSens1 = safetyhook::create_mid(pattern.get_first(0), [](SafetyHookContext& regs)
                    {
                        if (regs.ebx && *(uint8_t*)(regs.ebx + 0x3289))
                        {
                            *(float*)(regs.esp + 0x18) *= GetMouseLookSensitivity() * 0.022222223f;
                            *(float*)(regs.esp + 0x28) *= GetMouseLookSensitivity() * 0.022222223f;
                        }
                        else
                        {
                            *(float*)(regs.esp + 0x18) = regs.xmm2.f32[0];
                            *(float*)(regs.esp + 0x28) = regs.xmm3.f32[0];
                        }
                    });

                    pattern = hook::pattern("F3 0F 59 0D ? ? ? ? F3 0F 11 4C 24 ? F3 0F 10 4C 24 ? 0F 54 CC");
                    injector::MakeNOP(pattern.get_first(0), 8, true);
                    static auto CCamFollowPed_MouseSens2 = safetyhook::create_mid(pattern.get_first(0), [](SafetyHookContext& regs)
                    {
                        if (regs.ebx && *(uint8_t*)(regs.ebx + 0x3289))
                        {
                            regs.xmm1.f32[0] *= 0.022222223f;
                        }
                        else
                        {
                            regs.xmm1.f32[0] *= 0.0625f;
                        }
                    });
                }
                else
                {
                    pattern = hook::pattern("F3 0F 59 05 ? ? ? ? F3 0F 11 44 24 ? 76 ? 0F 28 C4");
                    injector::MakeNOP(pattern.get_first(0), 8, true);
                    static auto CCamFollowPed_MouseSens1 = safetyhook::create_mid(pattern.get_first(0), [](SafetyHookContext& regs)
                    {
                        if (regs.ebx && *(uint8_t*)(regs.ebx + 0x3289))
                        {
                            regs.xmm0.f32[0] *= GetMouseLookSensitivity() * 0.022222223f;
                        }
                        else
                        {
                            regs.xmm0.f32[0] *= 0.0625f;
                        }
                    });

                    pattern = hook::pattern("F3 0F 59 05 ? ? ? ? F3 0F 59 4C 24 ? 0F 57 ED");
                    injector::MakeNOP(pattern.get_first(0), 8, true);
                    static auto CCamFollowPed_MouseSens2 = safetyhook::create_mid(pattern.get_first(0), [](SafetyHookContext& regs)
                    {
                        if (regs.ebx && *(uint8_t*)(regs.ebx + 0x3289))
                        {
                            regs.xmm0.f32[0] *= 0.022222223f;
                        }
                        else
                        {
                            regs.xmm0.f32[0] *= 0.0625f;
                        }
                    });
                }

                pattern = find_pattern("F3 0F 59 0D ? ? ? ? F3 0F 11 4C 24 ? 80 A6", "F3 0F 59 0D ? ? ? ? F3 0F 11 4C 24 ? 74 ? 8D 4C 24");
                if (!pattern.empty())
                {
                    injector::MakeNOP(pattern.get_first(0), 8, true);
                    static auto CCamFollowPed_MouseSens3 = safetyhook::create_mid(pattern.get_first(0), [](SafetyHookContext& regs)
                    {
                        if (regs.ebx && *(uint8_t*)(regs.ebx + 0x3289))
                        {
                            regs.xmm1.f32[0] *= 0.022222223f;
                        }
                        else
                        {
                            regs.xmm1.f32[0] *= 0.0375f;
                        }
                    });
                }

                // CCamFollowVehicle
                pattern = hook::pattern("F3 0F 11 4C 24 ? 85 C0 0F 84 ? ? ? ? 8D 4C 24");
                if (!pattern.empty())
                {
                    injector::MakeNOP(pattern.get_first(0), 6, true);
                    static auto CCamFollowVehicle_MouseSens = safetyhook::create_mid(pattern.get_first(0), [](SafetyHookContext& regs)
                    {
                        *(float*)(regs.esp + 0x14) = GetMouseLookSensitivity();
                    });
                }
                else
                {
                    pattern = hook::pattern("F3 0F 11 44 24 ? F3 0F 10 05 ? ? ? ? F3 0F 11 44 24 ? 0F 84 ? ? ? ? 8D 44 24 ? 50 E8");
                    injector::MakeNOP(pattern.get_first(0), 6, true);
                    static auto CCamFollowVehicle_MouseSens = safetyhook::create_mid(pattern.get_first(0), [](SafetyHookContext& regs)
                    {
                        *(float*)(regs.esp + 0x28) = GetMouseLookSensitivity();
                    });
                }

                pattern = hook::pattern("F3 0F 59 0D ? ? ? ? F3 0F 59 05 ? ? ? ? F3 0F 59 0D ? ? ? ? F3 0F 10 1E");
                if (!pattern.empty())
                {
                    injector::MakeNOP(pattern.get_first(0), 24, true);
                    static auto CCamFollowVehicle_MouseSens1 = safetyhook::create_mid(pattern.get_first(0), [](SafetyHookContext& regs)
                    {
                        auto ptr = *(uintptr_t*)(regs.ebp + 8);

                        if (ptr && *(uint8_t*)(ptr + 0x3289))
                        {
                            regs.xmm1.f32[0] *= 0.022222223f;
                            regs.xmm0.f32[0] *= 0.022222223f;
                            regs.xmm1.f32[0] *= 1.0f;
                        }
                        else
                        {
                            regs.xmm1.f32[0] *= 0.019444445f;
                            regs.xmm0.f32[0] *= 0.0070000002f;
                            regs.xmm1.f32[0] *= 0.071428575f;
                        }
                    });

                    pattern = hook::pattern("F3 0F 59 1D ? ? ? ? F3 0F 59 C8 F3 0F 11 0F");
                    if (!pattern.empty())
                    {
                        injector::MakeNOP(pattern.get_first(0), 8, true);
                        static auto CCamFollowVehicle_MouseSens2 = safetyhook::create_mid(pattern.get_first(0), [](SafetyHookContext& regs)
                        {
                            auto ptr = *(uintptr_t*)(regs.ebp + 8);

                            if (ptr && *(uint8_t*)(ptr + 0x3289))
                            {
                                regs.xmm3.f32[0] *= 0.022222223f;
                            }
                            else
                            {
                                regs.xmm3.f32[0] *= 0.0070000002f;
                            }
                        });
                    }

                    pattern = hook::pattern("F3 0F 59 0D ? ? ? ? 0D ? ? ? ? 80 3D");
                    if (!pattern.empty())
                    {
                        injector::MakeNOP(pattern.get_first(0), 8, true);
                        static auto CCamFollowVehicle_MouseSens3 = safetyhook::create_mid(pattern.get_first(0), [](SafetyHookContext& regs)
                        {
                            auto ptr = *(uintptr_t*)(regs.ebp + 8);

                            if (ptr && *(uint8_t*)(ptr + 0x3289))
                            {
                                regs.xmm1.f32[0] *= 1.0f;
                            }
                            else
                            {
                                regs.xmm1.f32[0] *= 0.042857144f;
                            }
                        });
                    }
                }
                else
                {
                    pattern = hook::pattern("F3 0F 10 15 ? ? ? ? F3 0F 10 0E F3 0F 10 1D");
                    injector::MakeNOP(pattern.get_first(0), 8, true);
                    static auto CCamFollowVehicle_MouseSens1 = safetyhook::create_mid(pattern.get_first(0), [](SafetyHookContext& regs)
                    {
                        auto ptr = *(uintptr_t*)(regs.ebp + 8);

                        if (ptr && *(uint8_t*)(ptr + 0x3289))
                        {
                            regs.xmm2.f32[0] = 0.022222223f;
                        }
                        else
                        {
                            regs.xmm2.f32[0] = 0.019444445f;
                        }
                    });

                    pattern = hook::pattern("F3 0F 10 1D ? ? ? ? 0F B7 44 24 ? F3 0F 59 C2");
                    injector::MakeNOP(pattern.get_first(0), 8, true);
                    static auto CCamFollowVehicle_MouseSens2 = safetyhook::create_mid(pattern.get_first(0), [](SafetyHookContext& regs)
                    {
                        auto ptr = *(uintptr_t*)(regs.ebp + 8);

                        if (ptr && *(uint8_t*)(ptr + 0x3289))
                        {
                            regs.xmm3.f32[0] = 0.022222223f;
                        }
                        else
                        {
                            regs.xmm3.f32[0] = 0.0070000002f;
                        }
                    });

                    pattern = hook::pattern("F3 0F 59 05 ? ? ? ? F3 0F 59 07 F3 0F 59 C3");
                    injector::MakeNOP(pattern.get_first(0), 8, true);
                    static auto CCamFollowVehicle_MouseSens3 = safetyhook::create_mid(pattern.get_first(0), [](SafetyHookContext& regs)
                    {
                        auto ptr = *(uintptr_t*)(regs.ebp + 8);

                        if (ptr && *(uint8_t*)(ptr + 0x3289))
                        {
                            regs.xmm0.f32[0] *= 1.0f;
                        }
                        else
                        {
                            regs.xmm0.f32[0] *= 0.071428575f;
                        }
                    });

                    pattern = hook::pattern("F3 0F 59 05 ? ? ? ? F3 0F 59 C1 F3 0F 59 C3");
                    injector::MakeNOP(pattern.get_first(0), 8, true);
                    static auto CCamFollowVehicle_MouseSens4 = safetyhook::create_mid(pattern.get_first(0), [](SafetyHookContext& regs)
                    {
                        auto ptr = *(uintptr_t*)(regs.ebp + 8);

                        if (ptr && *(uint8_t*)(ptr + 0x3289))
                        {
                            regs.xmm0.f32[0] *= 1.0f;
                        }
                        else
                        {
                            regs.xmm0.f32[0] *= 0.042857144f;
                        }
                    });
                }

                // CCamFollowVehicle (First person "Hood" mode)
                pattern = hook::pattern("F3 0F 11 4C 24 ? E8 ? ? ? ? 50 E8 ? ? ? ? D9 5C 24");
                if (!pattern.empty())
                {
                    injector::MakeNOP(pattern.get_first(0), 6, true);
                    static auto CCamFollowVehicleFirstPerson_MouseSens = safetyhook::create_mid(pattern.get_first(0), [](SafetyHookContext& regs)
                    {
                        *(float*)(regs.esp + 0x34) = GetMouseLookSensitivity();
                    });
                }
                else
                {
                    pattern = hook::pattern("D9 5C 24 ? E8 ? ? ? ? 50 E8 ? ? ? ? D9 E0");
                    injector::MakeNOP(pattern.get_first(0), 4, true);
                    static auto CCamFollowVehicleFirstPerson_MouseSens = safetyhook::create_mid(pattern.get_first(0), [](SafetyHookContext& regs)
                    {
                        *(float*)(regs.esp + 0x18) *= GetMouseLookSensitivity();
                    });
                }
            }

            // Gamepad look-around sensitivity slider and multipliers
            {
                // CCamFollowPed
                auto pattern = hook::pattern("F3 0F 11 4C 24 ? 80 A6");
                if (!pattern.empty())
                {
                    injector::MakeNOP(pattern.get_first(0), 6, true);
                    static auto CCamFollowPed_GamepadSensXY = safetyhook::create_mid(pattern.get_first(0), [](SafetyHookContext& regs)
                    {
                        *(float*)(regs.esp + 0x18) *= GetGamepadLookSensitivity();
                        *(float*)(regs.esp + 0x28) = regs.xmm1.f32[0] * GetGamepadLookSensitivity();
                    });
                }
                else
                {
                    pattern = hook::pattern("F3 0F 11 44 24 ? 76 ? 0F 28 C4 F3 0F 5C C2");
                    injector::MakeNOP(pattern.get_first(0), 6, true);
                    static auto CCamFollowPed_GamepadSensX = safetyhook::create_mid(pattern.get_first(0), [](SafetyHookContext& regs)
                    {
                        *(float*)(regs.esp + 0x14) = regs.xmm0.f32[0] * GetGamepadLookSensitivity();
                    });

                    pattern = hook::pattern("F3 0F 59 C8 F3 0F 59 CA 80 A6");
                    injector::MakeNOP(pattern.get_first(0), 4, true);
                    static auto CCamFollowPed_GamepadSensY = safetyhook::create_mid(pattern.get_first(0), [](SafetyHookContext& regs)
                    {
                        regs.xmm1.f32[0] *= regs.xmm0.f32[0] * GetGamepadLookSensitivity();
                    });
                }

                // CCamFollowVehicle
                pattern = hook::pattern("E8 ? ? ? ? 8B 4C 24 ? 57 56 E8 ? ? ? ? D9 7C 24");
                if (!pattern.empty())
                {
                    static auto CCamFollowVehicle_GamepadSens = safetyhook::create_mid(pattern.get_first(0), [](SafetyHookContext& regs)
                    {
                        if (regs.eax && !*(uint8_t*)(regs.eax + 0x3289))
                        {
                            *(float*)(regs.ebp + 0x18) *= GetGamepadLookSensitivity();
                        }
                    });
                }
                else
                {
                    pattern = hook::pattern("F3 0F 10 06 F3 0F 59 44 24 ? F3 0F 11 06");
                    static auto CCamFollowVehicle_GamepadSens = safetyhook::create_mid(pattern.get_first(0), [](SafetyHookContext& regs)
                    {
                        if (regs.ebx && !*(uint8_t*)(regs.ebx + 0x3289))
                        {
                            *(float*)(regs.esp + 0x20) *= GetGamepadLookSensitivity();
                            *(float*)(regs.esp + 0x24) *= GetGamepadLookSensitivity();
                        }
                    });
                }

                // CCamFollowVehicle (First person "Hood" mode)
                pattern = hook::pattern("F3 0F 10 0C 85 ? ? ? ? F3 0F 58 05");
                if (!pattern.empty())
                {
                    injector::MakeNOP(pattern.get_first(0), 9, true);
                    static auto CCamFollowVehicleFirstPerson_OriginalSensOverride1 = safetyhook::create_mid(pattern.get_first(0), [](SafetyHookContext& regs)
                    {
                        regs.xmm1.f32[0] = 1.0f;
                    });
                }
                else
                {
                    pattern = hook::pattern("F3 0F 10 14 8D ? ? ? ? 0F 2F CA");
                    injector::MakeNOP(pattern.get_first(0), 9, true);
                    static auto CCamFollowVehicleFirstPerson_OriginalSensOverride1 = safetyhook::create_mid(pattern.get_first(0), [](SafetyHookContext& regs)
                    {
                        regs.xmm2.f32[0] = 1.0f;
                    });
                }

                pattern = hook::pattern("F3 0F 10 04 85 ? ? ? ? F3 0F 10 5C 24");
                if (!pattern.empty())
                {
                    injector::MakeNOP(pattern.get_first(0), 9, true);
                    static auto CCamFollowVehicleFirstPerson_OriginalSensOverride2 = safetyhook::create_mid(pattern.get_first(0), [](SafetyHookContext& regs)
                    {
                        regs.xmm0.f32[0] = 1.0f;
                    });
                }
                else
                {
                    pattern = hook::pattern("F3 0F 10 0C 95 ? ? ? ? F3 0F 10 5C 24");
                    injector::MakeNOP(pattern.get_first(0), 9, true);
                    static auto CCamFollowVehicleFirstPerson_OriginalSensOverride2 = safetyhook::create_mid(pattern.get_first(0), [](SafetyHookContext& regs)
                    {
                        regs.xmm1.f32[0] = 1.0f;
                    });
                }

                pattern = hook::pattern("F3 0F 11 44 24 ? C6 44 24 ? ? E8 ? ? ? ? 83 C4 ? 88 44 24");
                if (!pattern.empty())
                {
                    injector::MakeNOP(pattern.get_first(0), 6, true);
                    static auto CCamFollowVehicleFirstPerson_GamepadSens = safetyhook::create_mid(pattern.get_first(0), [](SafetyHookContext& regs)
                    {
                        *(float*)(regs.esp + 0x40) = regs.xmm0.f32[0] * GetGamepadLookSensitivity();
                    });
                }
                else
                {
                    pattern = hook::pattern("F3 0F 11 44 24 ? C6 44 24 ? ? E8 ? ? ? ? 83 C4 ? 85 F6");
                    injector::MakeNOP(pattern.get_first(0), 6, true);
                    static auto CCamFollowVehicleFirstPerson_GamepadSens = safetyhook::create_mid(pattern.get_first(0), [](SafetyHookContext& regs)
                    {
                        *(float*)(regs.esp + 0x24) = regs.xmm0.f32[0] * GetGamepadLookSensitivity();
                    });
                }
            }

            // Mouse aim sensitivity slider and multipliers
            {
                // CCamAimWeapon
                auto pattern = hook::pattern("0F 45 C1 0F 2F 0D");
                if (!pattern.empty())
                {
                    injector::MakeNOP(pattern.get_first(0), 3, true);
                    injector::WriteMemory<uint16_t>(pattern.get_first(0), 0xC18B, true);
                }
                else
                {
                    pattern = hook::pattern("74 ? B8 ? ? ? ? 0F 2F 05");
                    injector::MakeNOP(pattern.get_first(0), 2, true);
                }

                pattern = hook::pattern("0F 28 E3 EB ? F3 0F 10 25 ? ? ? ? 84 C0");
                if (!pattern.empty())
                {
                    injector::MakeNOP(pattern.get_first(0), 3, true);
                    static auto CCamAimWeapon_MouseAimSens = safetyhook::create_mid(pattern.get_first(0), [](SafetyHookContext& regs)
                    {
                        regs.xmm3.f32[0] = GetMouseAimSensitivity();
                        regs.xmm4.f32[0] = regs.xmm3.f32[0];
                    });
                }
                else
                {
                    pattern = hook::pattern("0F 28 D8 EB ? F3 0F 10 1D");
                    injector::MakeNOP(pattern.get_first(0), 3, true);
                    static auto CCamAimWeapon_MouseAimSens = safetyhook::create_mid(pattern.get_first(0), [](SafetyHookContext& regs)
                    {
                        regs.xmm0.f32[0] = GetMouseAimSensitivity();
                        regs.xmm3.f32[0] = regs.xmm0.f32[0];
                    });
                }

                // CCamFpsWeapon (Sniper)
                pattern = hook::pattern("0F 28 CD EB ? F3 0F 10 0D ? ? ? ? 85 C0");
                if (!pattern.empty())
                {
                    injector::MakeNOP(pattern.get_first(0), 3, true);
                    static auto CCamFpsWeapon_MouseAimSens = safetyhook::create_mid(pattern.get_first(0), [](SafetyHookContext& regs)
                    {
                        regs.xmm1.f32[0] = GetMouseAimSensitivity();
                        regs.xmm5.f32[0] = regs.xmm1.f32[0];
                    });
                }
                else
                {
                    pattern = hook::pattern("0F 28 D0 EB ? F3 0F 10 15 ? ? ? ? 85 DB");
                    injector::MakeNOP(pattern.get_first(0), 3, true);
                    static auto CCamFpsWeapon_MouseAimSens = safetyhook::create_mid(pattern.get_first(0), [](SafetyHookContext& regs)
                    {
                        regs.xmm2.f32[0] = GetMouseAimSensitivity();
                        regs.xmm0.f32[0] = regs.xmm2.f32[0];
                    });
                }
            }

            // Gamepad aiming sensitivity slider and multipliers
            {
                // CCamAimWeapon
                auto pattern = hook::pattern("F3 0F 10 1D ? ? ? ? F3 0F 10 4C 24 ? F3 0F 10 45");
                if (!pattern.empty())
                {
                    injector::MakeNOP(pattern.get_first(0), 8, true);
                    static auto CCamAimWeapon_GamepadAimSens = safetyhook::create_mid(pattern.get_first(0), [](SafetyHookContext& regs)
                    {
                        regs.xmm3.f32[0] = GetGamepadAimSensitivity() * 0.015f;
                        regs.xmm4.f32[0] = GetGamepadAimSensitivity() * 0.02f;
                    });
                }
                else
                {
                    pattern = hook::pattern("F3 0F 10 15 ? ? ? ? 84 C0 F3 0F 10 45");
                    injector::MakeNOP(pattern.get_first(0), 8, true);
                    static auto CCamAimWeapon_GamepadAimSens = safetyhook::create_mid(pattern.get_first(0), [](SafetyHookContext& regs)
                    {
                        regs.xmm2.f32[0] = GetGamepadAimSensitivity() * 0.015f;
                        regs.xmm3.f32[0] = GetGamepadAimSensitivity() * 0.02f;
                    });
                }

                // CCamFpsWeapon (Sniper)
                pattern = hook::pattern("F3 0F 10 04 8D ? ? ? ? F3 0F 59 05 ? ? ? ? F3 0F 58 87");
                if (!pattern.empty())
                {
                    injector::MakeNOP(pattern.get_first(0), 9, true);
                    static auto CCamFpsWeapon_OriginalSensOverride1 = safetyhook::create_mid(pattern.get_first(0), [](SafetyHookContext& regs)
                    {
                        regs.xmm0.f32[0] = 1.0f;
                    });
                }
                else
                {
                    pattern = hook::pattern("F3 0F 10 0C 85 ? ? ? ? F3 0F 59 0D ? ? ? ? F3 0F 58 8E");
                    injector::MakeNOP(pattern.get_first(0), 9, true);
                    static auto CCamFpsWeapon_OriginalSensOverride1 = safetyhook::create_mid(pattern.get_first(0), [](SafetyHookContext& regs)
                    {
                        regs.xmm1.f32[0] = 1.0f;
                    });
                }

                pattern = hook::pattern("F3 0F 10 0C 8D ? ? ? ? 0F 2F C1 76");
                if (!pattern.empty())
                {
                    injector::MakeNOP(pattern.get_first(0), 9, true);
                    static auto CCamFpsWeapon_OriginalSensOverride2 = safetyhook::create_mid(pattern.get_first(0), [](SafetyHookContext& regs)
                    {
                        regs.xmm1.f32[0] = 1.0f;
                    });
                }
                else
                {
                    pattern = hook::pattern("F3 0F 10 14 85 ? ? ? ? 0F 2F CA");
                    injector::MakeNOP(pattern.get_first(0), 9, true);
                    static auto CCamFpsWeapon_OriginalSensOverride2 = safetyhook::create_mid(pattern.get_first(0), [](SafetyHookContext& regs)
                    {
                        regs.xmm2.f32[0] = 1.0f;
                    });
                }

                pattern = find_pattern("F3 0F 10 04 85 ? ? ? ? F3 0F 59 C1 F3 0F 11 44 24 ? 66 0F 6E 05", "F3 0F 10 04 8D ? ? ? ? F3 0F 59 C1");
                if (!pattern.empty())
                {
                    injector::MakeNOP(pattern.get_first(0), 9, true);
                    static auto CCamFpsWeapon_OriginalSensOverride3 = safetyhook::create_mid(pattern.get_first(0), [](SafetyHookContext& regs)
                    {
                        regs.xmm0.f32[0] = 1.0f;
                    });
                }

                pattern = hook::pattern("F3 0F 10 2D ? ? ? ? 80 7C 24 ? ? F3 0F 10 64 24");
                if (!pattern.empty())
                {
                    injector::MakeNOP(pattern.get_first(0), 8, true);
                    static auto CCamFpsWeapon_GamepadAimSens = safetyhook::create_mid(pattern.get_first(0), [](SafetyHookContext& regs)
                    {
                        regs.xmm5.f32[0] = GetGamepadAimSensitivity() * 0.022222223f; // * 0.015f;
                        regs.xmm1.f32[0] = GetGamepadAimSensitivity() * 0.022222223f; // * 0.02f;
                    });
                }
                else
                {
                    pattern = hook::pattern("F3 0F 10 2D ? ? ? ? 80 7C 24 ? ? F3 0F 10 44 24");
                    injector::MakeNOP(pattern.get_first(0), 8, true);
                    static auto CCamFpsWeapon_GamepadAimSens = safetyhook::create_mid(pattern.get_first(0), [](SafetyHookContext& regs)
                    {
                        regs.xmm5.f32[0] = GetGamepadAimSensitivity() * 0.022222223f; // * 0.015f;
                        regs.xmm2.f32[0] = GetGamepadAimSensitivity() * 0.022222223f; // * 0.02f;
                    });
                }
            }
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

        //FusionFix::onInitEvent() += []()
        //{
        //    // Script
        //    hbNATIVE_GET_MOUSE_INPUT.fun = NativeOverride::Register(Natives::NativeHashes::GET_MOUSE_INPUT, NATIVE_GET_MOUSE_INPUT, "E8 ? ? ? ? 83 C4 08 C3", 30);
        //};
    }
} RawInput;