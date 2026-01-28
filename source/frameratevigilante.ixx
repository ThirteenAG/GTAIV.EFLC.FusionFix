module;

#include <common.hxx>

export module frameratevigilante;

import common;
import comvars;
import natives;
import settings;

injector::hook_back<double(__fastcall*)(void* _this, void* edx, void* a2, void* a3)> hbsub_A18510;
double __fastcall sub_A18510(void* _this, void* edx, void* a2, void* a3)
{
    float f = 1.0f;

    if (!Natives::IsUsingController())
    {
        f = 3.0f;
    }

    return hbsub_A18510.fun(_this, edx, a2, a3) * (*CTimer::fTimeStep / (1.0f / 30.0f)) * f;
}

int (__cdecl *game_rand)() = nullptr;
uint32_t* dword_11F7060 = nullptr;
uint32_t* dword_12088B4 = nullptr;
uint32_t* dword_1037720 = nullptr;
uint32_t* dword_11F704C = nullptr;
SafetyHookInline shCameraShake = {};
void __fastcall OnFootCameraShake(float* CameraData, void* edx, float Multiplier)
{
    static auto CameraShake = FusionFixSettings.GetRef("PREF_CAMERASHAKE");
    if (!CameraShake->get())
    {
        float Output[] = { 0.0f, 0.0f, 0.0f };
        return Matrix34::fromEulersXYZ(CameraData, 0, Output);
    }

    float DeltaTime = *CTimer::fTimeStep;

    float TimeScale = DeltaTime * 30.0f;
    if (*dword_11F7060 == 1 || *dword_12088B4 != -1 || *dword_1037720 == 18)
    {
        TimeScale = *dword_11F704C * 0.001f;
        TimeScale *= 30.0f;
    }

    static float NoiseTimer = 0.0f;
    static float CamX = 0.0f;
    static float CamY = 0.0f;
    static float CamZ = 0.0f;

    NoiseTimer += DeltaTime;

    if (NoiseTimer >= 1.0f / 30.0f)
    {
        NoiseTimer = 0.0f;

        CamX = fabs(CameraData[16] / CameraData[20]) * (CameraData[37] - CameraData[36]) + CameraData[36];
        CamY = fabs(CameraData[17] / CameraData[21]) * (CameraData[37] - CameraData[36]) + CameraData[36];
        CamZ = fabs(CameraData[18] / CameraData[22]) * (CameraData[37] - CameraData[36]) + CameraData[36];

        if (CameraData[16] > 0.0f && CameraData[28] > 0.0f || CameraData[16] < 0.0f && CameraData[28] < 0.0f)
            CamX *= CameraData[32];
        if (CameraData[17] > 0.0f && CameraData[29] > 0.0f || CameraData[17] < 0.0f && CameraData[29] < 0.0f)
            CamY *= CameraData[33];
        if (CameraData[18] > 0.0f && CameraData[30] > 0.0f || CameraData[18] < 0.0f && CameraData[30] < 0.0f)
            CamZ *= CameraData[34];

        CamX *= game_rand() / 32767.0f * CameraData[24];
        CamY *= game_rand() / 32767.0f * CameraData[25];
        CamZ *= game_rand() / 32767.0f * CameraData[26];

        if (CameraData[16] > 0.0f)
            CamX *= -1.0f;
        if (CameraData[17] > 0.0f)
            CamY *= -1.0f;
        if (CameraData[18] > 0.0f)
            CamZ *= -1.0f;

        if ((int)(game_rand() / 32768.0f * ((int)CameraData[38] - 1)) == 1)
        {
            CamX += CameraData[39] * (game_rand() / 32767.0f * 2.0f - 1.0f);
            CamY += CameraData[39] * (game_rand() / 32767.0f * 2.0f - 1.0f);
            CamZ += CameraData[39] * (game_rand() / 32767.0f * 2.0f - 1.0f);
        }

        CameraData[28] += CamX;
        CameraData[29] += CamY;
        CameraData[30] += CamZ;
    }

    CameraData[16] = std::clamp(CameraData[28] * TimeScale + CameraData[16], -CameraData[20], CameraData[20]);
    CameraData[17] = std::clamp(CameraData[29] * TimeScale + CameraData[17], -CameraData[21], CameraData[21]);
    CameraData[18] = std::clamp(CameraData[30] * TimeScale + CameraData[18], -CameraData[22], CameraData[22]);

    float Output[] = { CameraData[16] * Multiplier, CameraData[17] * Multiplier, CameraData[18] * Multiplier };

    Matrix34::fromEulersXYZ(CameraData, 0, Output);
}

std::unordered_map<int, float> last_fov_values;
std::unordered_map<int, bool> fov_cache_initialized;
injector::hook_back<decltype(&Natives::SetCamFov)> hbSET_CAM_FOV;
void __cdecl NATIVE_SET_CAM_FOV(int cam, float targetFOV)
{
    float fov;
    Natives::GetCamFov(cam, &fov);

    if (fov_cache_initialized.find(cam) == fov_cache_initialized.end() || !fov_cache_initialized[cam])
    {
        last_fov_values[cam] = fov;
        fov_cache_initialized[cam] = true;
    }

    // Calculate how much the original code wanted to increment
    float desired_increment = targetFOV - last_fov_values[cam];

    // Define a threshold for what constitutes a "small increment" vs a "jump"
    constexpr float SMALL_INCREMENT_THRESHOLD = 0.06f; // Adjust this value as needed

    float new_fov;

    if (fabsf(desired_increment) <= SMALL_INCREMENT_THRESHOLD)
    {
        // Small increment - apply time-based scaling
        float time_scaled_increment = desired_increment * *CTimer::fTimeStep / (1.0f / 30.0f);
        new_fov = fov + time_scaled_increment;
    }
    else
    {
        // Large jump - apply immediately without time scaling
        new_fov = targetFOV;
        // Reset the cache since we're jumping to a new value
        last_fov_values[cam] = targetFOV;
    }

    // Update cache only for small increments
    if (fabsf(desired_increment) <= SMALL_INCREMENT_THRESHOLD)
    {
        last_fov_values[cam] = targetFOV;
    }

    return hbSET_CAM_FOV.fun(cam, new_fov);
}

injector::hook_back<decltype(&Natives::SlideObject)> hbSLIDE_OBJECT;
bool __cdecl NATIVE_SLIDE_OBJECT_1(Object object, float x, float y, float z, float xs, float ys, float zs, bool flag)
{
    float Delta = *CTimer::fTimeStep / (1.0f / 30.0f);
    return hbSLIDE_OBJECT.fun(object, x, y, z, xs * Delta, ys * Delta, zs * Delta, flag);
}

SafetyHookInline shNATIVE_SLIDE_OBJECT{};
bool __cdecl NATIVE_SLIDE_OBJECT_2(Object object, float x, float y, float z, float xs, float ys, float zs, bool flag)
{
    float Delta = *CTimer::fTimeStep / (1.0f / 30.0f);
    return shNATIVE_SLIDE_OBJECT.unsafe_ccall<bool>(object, x, y, z, xs * Delta, ys * Delta, zs * Delta, flag);
}

class FramerateVigilante
{
public:
    FramerateVigilante()
    {
        FusionFix::onInitEventAsync() += []()
        {
            // Handbrake Cam force (Gamepad)
            auto pattern = find_pattern("E8 ? ? ? ? D9 5C 24 7C F3 0F 10 4C 24", "E8 ? ? ? ? D9 5C 24 70 F3 0F 10 44 24 ? F3 0F 58 86");
            hbsub_A18510.fun = injector::MakeCALL(pattern.get_first(0), sub_A18510).get();

            // CCamFollowVehicle auto centering force
            {
                // Skips some clamps set in the vehicle camera code that prevent auto centering from scaling properly with the frame rate
                pattern = find_pattern("77 ? 0F 28 C2 F3 0F 5C 8F", "77 ? 0F 28 D3 F3 0F 10 8E");
                if (!pattern.empty())
                {
                    injector::MakeNOP(pattern.get_first(0), 2, true);
                }

                pattern = find_pattern("76 ? 0F 28 C8 EB ? F3 0F 10 4C 24 ? 80 7C 24", "76 ? 0F 28 CE EB ? 0F 28 CF 84 D2");
                if (!pattern.empty())
                {
                    injector::WriteMemory<uint8_t>(pattern.get_first(0), 0xEB, true);
                }
            }

            // Bike physics (Originally by Sergeanur)
            // They will need a full fix in the future, one of the main things to look at are the slippery wheels, this only marginally improves them
            pattern = hook::pattern("F3 0F 10 45 ? 51 8B CF F3 0F 11 04 24 E8 ? ? ? ? 8A 8F");
            if (!pattern.empty())
            {
                injector::MakeNOP(pattern.get_first(0), 5, true);
                static auto BikePhysics = safetyhook::create_mid(pattern.get_first(0), [](SafetyHookContext& regs)
                {
                    float f = *(float*)(regs.ebp + 0x08);
                    f = std::clamp(f, 1.0f / 30.0f, FLT_MAX);
                    regs.xmm0.f32[0] = f;
                });
            }
            else
            {
                pattern = hook::pattern("D9 45 ? 51 8B CE D9 1C 24 E8 ? ? ? ? 8A 86");
                injector::MakeNOP(pattern.get_first(0), 3, true);
                static auto BikePhysics = safetyhook::create_mid(pattern.get_first(0), [](SafetyHookContext& regs)
                {
                    float f = *(float*)(regs.ebp + 0x08);
                    f = std::clamp(f, 1.0f / 30.0f, FLT_MAX);
                    *(float*)(regs.ebp + 0x08) = f;
                });
            }

            // Loading text flash speed (IV and TLAD)
            {
                // Skips an else path which post-processes the flash speed unnecessarily.
                // The speed previously worked correctly at variable frame rates on patches 1050 and lower,
                // but this check added in patch 1060 along with TBoGT's sparks prevents it from working properly.
                pattern = hook::pattern("F3 0F 10 05 ? ? ? ? F3 0F 58 C1 F3 0F 11 05 ? ? ? ? EB");
                if (!pattern.empty())
                {
                    injector::MakeNOP(pattern.get_first(0), 20, true);
                }
                else
                {
                    pattern = hook::pattern("F3 0F 10 05 ? ? ? ? F3 0F 58 05 ? ? ? ? F3 0F 11 05 ? ? ? ? EB");
                    injector::MakeNOP(pattern.get_first(0), 24, true);
                }

                // This just slightly corrects Toronto's old fix from 1040 to be fully accurate to the other patches
                pattern = hook::pattern("F3 0F 10 44 24 ? F3 0F 59 05 ? ? ? ? F3 0F 59 05 ? ? ? ? F3 0F 59 05 ? ? ? ? F3 0F 58 05");
                if (!pattern.empty())
                {
                    injector::MakeNOP(pattern.get_first(0), 22, true);
                    static auto LoadingTextFlashSpeed = safetyhook::create_mid(pattern.get_first(0), [](SafetyHookContext& regs)
                    {
                        // Ends up being dword_1175C40 += dword_1032790 * (SomeTimer * (1000 / 30));
                        regs.xmm0.f32[0] = *(float*)(regs.esp + 0x34);
                        regs.xmm0.f32[0] *= (1000 / 30);
                    });
                }
                else
                {
                    pattern = hook::pattern("F3 0F 10 44 24 ? F3 0F 59 05 ? ? ? ? F3 0F 59 05 ? ? ? ? F3 0F 58 05");
                    injector::MakeNOP(pattern.get_first(0), 14, true);
                    static auto LoadingTextFlashSpeed = safetyhook::create_mid(pattern.get_first(0), [](SafetyHookContext& regs)
                    {
                        // Ends up being dword_11FB434 += dword_F38420 * (SomeTimer * (1000 / 30));
                        regs.xmm0.f32[0] = *(float*)(regs.esp + 0x44);
                        regs.xmm0.f32[0] *= (1000 / 30);
                    });
                }
            }

            // Loading text sparks' speed (TBoGT)
            {
                // So for IV's flashing we had an initial speed and then a secondary speed.
                // Here its similar, however the secondary speed might actually drive everything as the initial one is just * 0.001 while the secondary one is * 0.085.
                // So we're just taking the timer variable Toronto used for IV in 1040 and scale the secondary value with that.
                pattern = hook::pattern("F3 0F 58 0D ? ? ? ? 0F 5B C0 F3 0F 11 0D");
                if (!pattern.empty())
                {
                    static auto dword_E81598 = *pattern.get_first<float*>(4);
                    injector::MakeNOP(pattern.get_first(0), 8, true);
                    static auto LoadingTextSparksSpeed = safetyhook::create_mid(pattern.get_first(0), [](SafetyHookContext& regs)
                    {
                        // Ends up being dword_1175770 += dword_E81598 * (SomeTimer * (1000 / 30));
                        regs.xmm1.f32[0] += *dword_E81598 * *(float*)(regs.esp + 0x34) * (1000 / 30);
                    });
                }
                else
                {
                    pattern = hook::pattern("F3 0F 58 05 ? ? ? ? F3 0F 2A 0D");
                    static auto flt_DEF584 = *pattern.get_first<float*>(4);
                    injector::MakeNOP(pattern.get_first(0), 8, true);
                    static auto LoadingTextSparksSpeed = safetyhook::create_mid(pattern.get_first(0), [](SafetyHookContext& regs)
                    {
                        // Ends up being dword_11FB41C += flt_DEF584 * (SomeTimer * (1000 / 30));
                        regs.xmm0.f32[0] += *flt_DEF584 * *(float*)(regs.esp + 0x44) * (1000 / 30);
                    });
                }
            }

            // Loading screen animation speed
            // Fixes loading screen animations running at double the intended speed in comparison to consoles.
            // That is normally pretty noticeable when using the console loading screens with the default console loadingscreens.dat files.
            // Note: This does not fix the somewhat fps dependent loadscreen animations on itself, but just slows them down so they match consoles at 30fps.
            {
                auto pattern = hook::pattern("F3 0F 59 0D ? ? ? ? C7 84 18");
                if (!pattern.empty())
                {
                    injector::MakeNOP(pattern.get_first(0), 8, true);
                    static auto LoadingScreenAnimationSpeed1 = safetyhook::create_mid(pattern.get_first(0), [](SafetyHookContext& regs)
                    {
                        regs.xmm1.f32[0] *= 33.3f; // 66.6f --> 33.3f
                    });

                    pattern = hook::pattern("F3 0F 59 25 ? ? ? ? C7 84 18");
                    injector::MakeNOP(pattern.get_first(0), 8, true);
                    static auto LoadingScreenAnimationSpeed2 = safetyhook::create_mid(pattern.get_first(0), [](SafetyHookContext& regs)
                    {
                        regs.xmm4.f32[0] *= 33.3f; // 66.6f --> 33.3f
                    });
                }
                else
                {
                    pattern = hook::pattern("F3 0F 10 15 ? ? ? ? F3 0F 59 E3 F3 0F 59 E2");
                    injector::MakeNOP(pattern.get_first(0), 8, true);
                    static auto LoadingScreenAnimationSpeed1 = safetyhook::create_mid(pattern.get_first(0), [](SafetyHookContext& regs)
                    {
                        regs.xmm2.f32[0] = 33.3f; // 66.6f --> 33.3f
                    });

                    pattern = hook::pattern("F3 0F 10 15 ? ? ? ? F3 0F 10 A4 37");
                    injector::MakeNOP(pattern.get_first(0), 8, true);
                    static auto LoadingScreenAnimationSpeed2 = safetyhook::create_mid(pattern.get_first(0), [](SafetyHookContext& regs)
                    {
                        regs.xmm2.f32[0] = 33.3f; // 66.6f --> 33.3f
                    });
                }
            }

            // CD/Busy spinner speed
            // Note:
            // This does not work for the spinner that shows up while loading saved games, as CTimer::fTimeStep does not work in menus.
            // It is not a big deal, because we skip that spinner so saves load faster. But if we ever want to restore it, that should be looked into.
            pattern = hook::pattern("F3 0F 58 05 ? ? ? ? 33 C0 A3");
            if (!pattern.empty())
            {
                static auto dword_E841A8 = *pattern.get_first<float*>(4);
                injector::MakeNOP(pattern.get_first(0), 8, true);
                static auto CDSpinnerSpeed = safetyhook::create_mid(pattern.get_first(0), [](SafetyHookContext& regs)
                {
                    regs.xmm0.f32[0] += *dword_E841A8 * *CTimer::fTimeStep / (1.0f / 30.0f);
                });
            }
            else
            {
                pattern = hook::pattern("F3 0F 58 15 ? ? ? ? 33 C0 F3 0F 11 15");
                static auto dword_DD6B68 = *pattern.get_first<float*>(4);
                injector::MakeNOP(pattern.get_first(0), 8, true);
                static auto CDSpinnerSpeed = safetyhook::create_mid(pattern.get_first(0), [](SafetyHookContext& regs)
                {
                    regs.xmm2.f32[0] += *dword_DD6B68 * *CTimer::fTimeStep / (1.0f / 30.0f);
                });
            }

            // Cop blips' speed
            pattern = hook::pattern("A1 ? ? ? ? 6B C0 15");
            if (!pattern.empty())
            {
                injector::MakeNOP(pattern.get_first(0), 5, true);
                static auto CopBlipsSpeed = safetyhook::create_mid(pattern.get_first(0), [](SafetyHookContext& regs)
                {
                    regs.eax = *CTimer::m_snTimeInMillisecondsPauseMode / (1000 / 30);
                });
            }

            // Radar zoom speed
            {
                // This skips a check that determines after how many frames the radar zooming is updated.
                // Normally it updates every 30 milliseconds, which visually makes the zooming always update at a "30hz" rate regardless of fps.
                pattern = find_pattern("0F 86 ? ? ? ? F3 0F 10 15 ? ? ? ? 0F 2E CA", "0F 86 ? ? ? ? F3 0F 10 0D ? ? ? ? 0F 2E C1");
                if (!pattern.empty())
                {
                    injector::MakeNOP(pattern.get_first(0), 6, true);
                }

                // Zoom in speed
                pattern = hook::pattern("F3 0F 58 15 ? ? ? ? 0F 2F CA EB");
                if (!pattern.empty())
                {
                    static auto dword_FE8B5C = *pattern.get_first<float*>(4);
                    injector::MakeNOP(pattern.get_first(0), 8, true);
                    static auto RadarZoomInSpeed = safetyhook::create_mid(pattern.get_first(0), [](SafetyHookContext& regs)
                    {
                        regs.xmm2.f32[0] += *dword_FE8B5C * *CTimer::fTimeStep / (1.0f / 30.0f);
                    });
                }
                else
                {
                    pattern = hook::pattern("F3 0F 58 0D ? ? ? ? 0F 2F C1 EB");
                    static auto dword_E52BF8 = *pattern.get_first<float*>(4);
                    injector::MakeNOP(pattern.get_first(0), 8, true);
                    static auto RadarZoomInSpeed = safetyhook::create_mid(pattern.get_first(0), [](SafetyHookContext& regs)
                    {
                        regs.xmm1.f32[0] += *dword_E52BF8 * *CTimer::fTimeStep / (1.0f / 30.0f);
                    });
                }

                // Zoom out speed
                pattern = hook::pattern("F3 0F 5C 15 ? ? ? ? 0F 2F D1 76 ? 0F 28 CA");
                if (!pattern.empty())
                {
                    static auto dword_FE8B5C = *pattern.get_first<float*>(4);
                    injector::MakeNOP(pattern.get_first(0), 8, true);
                    static auto RadarZoomOutSpeed = safetyhook::create_mid(pattern.get_first(0), [](SafetyHookContext& regs)
                    {
                        regs.xmm2.f32[0] -= *dword_FE8B5C * *CTimer::fTimeStep / (1.0f / 30.0f);
                    });
                }
                else
                {
                    pattern = hook::pattern("F3 0F 5C 0D ? ? ? ? 0F 2F C8 76 ? 0F 28 C1");
                    static auto dword_E52BF8 = *pattern.get_first<float*>(4);
                    injector::MakeNOP(pattern.get_first(0), 8, true);
                    static auto RadarZoomOutSpeed = safetyhook::create_mid(pattern.get_first(0), [](SafetyHookContext& regs)
                    {
                        regs.xmm1.f32[0] -= *dword_E52BF8 * *CTimer::fTimeStep / (1.0f / 30.0f);
                    });
                }
            }

            // Camera shake
            game_rand = (decltype(game_rand))injector::GetBranchDestination(find_pattern("E8 ? ? ? ? F3 0F 10 4C 24 ? F3 0F 5C 4C 24 ? F3 0F 10 5C 24", "E8 ? ? ? ? F3 0F 10 4C 24 ? F3 0F 59 4C 24 ? F3 0F 59 4C 24").get_first()).as_int();
            dword_11F7060 = *find_pattern("83 3D ? ? ? ? ? F3 0F 10 05 ? ? ? ? F3 0F 59 C1", "83 3D ? ? ? ? ? F3 0F 10 05 ? ? ? ? F3 0F 59 05").get_first<uint32_t*>(2);
            dword_12088B4 = *find_pattern("A1 ? ? ? ? 3B 05 ? ? ? ? 75 ? 83 3D ? ? ? ? ? 75 ? A1", "A1 ? ? ? ? 3B 05 ? ? ? ? 75 ? 83 3D ? ? ? ? ? 75 ? 8B 0D ? ? ? ? DB 05").get_first<uint32_t*>(1);
            dword_1037720 = *find_pattern("83 3D ? ? ? ? ? 75 ? A1 ? ? ? ? 66 0F 6E C0", "83 3D ? ? ? ? ? 75 ? 8B 0D ? ? ? ? DB 05").get_first<uint32_t*>(2);
            dword_11F704C = *find_pattern("A1 ? ? ? ? 66 0F 6E C0 F3 0F E6 C0 C1 E8 ? F2 0F 58 04 C5 ? ? ? ? 66 0F 5A C0 F3 0F 59 05 ? ? ? ? F3 0F 59 C1", "0D ? ? ? ? DB 05 ? ? ? ? 85 C9 7D ? D8 05 ? ? ? ? D8 0D").get_first<uint32_t*>(1);
            pattern = find_pattern("55 8B EC 83 E4 ? 83 EC ? 56 57 8B F9 F3 0F 10 05", "55 8B EC 83 E4 ? 0F 57 E4 F3 0F 10 1D");
            shCameraShake = safetyhook::create_inline(pattern.get_first(), OnFootCameraShake);

            // Native patches
            hbSET_CAM_FOV.fun = NativeOverride::Register(Natives::NativeHashes::SET_CAM_FOV, NATIVE_SET_CAM_FOV, "E8 ? ? ? ? 83 C4 08 C3", 30);
            hbSLIDE_OBJECT.fun = NativeOverride::Register(Natives::NativeHashes::SLIDE_OBJECT, NATIVE_SLIDE_OBJECT_1, "E8 ? ? ? ? 0F B6 C8", 107);
            if (!hbSLIDE_OBJECT.fun)
            {
                pattern = hook::pattern("55 8B EC 83 E4 F0 8B 45 08 8B 0D ? ? ? ? 81 EC ? ? ? ? 56 50");
                shNATIVE_SLIDE_OBJECT = safetyhook::create_inline(pattern.get_first(0), NATIVE_SLIDE_OBJECT_2);
            }
        };
    }
} FramerateVigilante;