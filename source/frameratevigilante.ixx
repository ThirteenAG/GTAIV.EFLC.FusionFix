module;

#include <common.hxx>

export module frameratevigilante;

import common;
import comvars;
import natives;
import settings;
import classext;

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

SafetyHookInline shProcessStaticCounter = {};
void __fastcall ProcessStaticCounter(void* _this, void* edx)
{
    auto PIExt = GetPedIntelligenceExt((uintptr_t)_this);
    if (!PIExt)
    {
        return shProcessStaticCounter.unsafe_fastcall(_this, edx);
    }

    PIExt->m_fTimeStepAccumulator += *CTimer::fTimeStep;
    if (PIExt->m_fTimeStepAccumulator >= (1.0f / 30.0f))
    {
        PIExt->m_fTimeStepAccumulator = 0.0f;
        return shProcessStaticCounter.unsafe_fastcall(_this, edx);
    }
}

int (__cdecl* game_rand)() = nullptr;
uint32_t* dword_11F7060 = nullptr;
uint32_t* dword_12088B4 = nullptr;
uint32_t* dword_1037720 = nullptr;
uint32_t* dword_11F704C = nullptr;
SafetyHookInline shOnFootCameraShake = {};
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

SafetyHookInline shHoodCameraBumping = {};
void __fastcall HoodCameraBumping(float* this_ptr, void*, float* vehicle, float* input_vector, float* out_offset, float a5)
{
    float real_dt = *CTimer::fTimeStep;

    constexpr float FIXED_RATE = 30.0f;
    constexpr float FIXED_DT = 1.0f / FIXED_RATE;

    static float accumulator = 0.0f;
    accumulator += real_dt;

    float* right_vec = *(float**)((char*)vehicle + 0x20);

    // Save state before physics update to use in interpolation
    static float last_x = 0.0f;
    static float last_y = 0.0f;
    static float last_z = 0.0f;

    if (accumulator >= FIXED_DT)
    {
        last_x = this_ptr[176];
        last_y = this_ptr[177];
        last_z = this_ptr[178];
    }

    while (accumulator >= FIXED_DT)
    {
        accumulator -= FIXED_DT;
        float dt = FIXED_DT;

        float outWorldPos[4];
        CPhysical::TransformOffsetToWorldSpace(vehicle, 0, outWorldPos, input_vector, 0, 0);

        float prev_ref_x = this_ptr[180];
        float prev_ref_y = this_ptr[181];
        float prev_ref_z = this_ptr[182];

        float tmp1 = prev_ref_y + this_ptr[186] * input_vector[0] - this_ptr[184] * input_vector[2];
        float tmp2 = prev_ref_x + this_ptr[185] * input_vector[2] - this_ptr[186] * input_vector[1];
        float tmp3 = prev_ref_z + this_ptr[184] * input_vector[1] - this_ptr[185] * input_vector[0];

        float dx = outWorldPos[1] - tmp1;
        float dy = outWorldPos[0] - tmp2;
        float dz = outWorldPos[2] - tmp3;

        float frame_time_mul = *(&*CTimer::fTimeStep + 1);
        dx *= frame_time_mul;
        dy *= frame_time_mul;
        dz *= frame_time_mul;

        float dot_a = right_vec[1] * dx + right_vec[0] * dy + right_vec[2] * dz; // lateral
        float dot_b = right_vec[5] * dx + right_vec[4] * dy + right_vec[6] * dz; // longitudinal

        float clamped_a = std::clamp(dot_a, -5.0f, 5.0f);
        float clamped_b = std::clamp(dot_b, -5.0f, 5.0f);

        float accum_x = this_ptr[172];
        float accum_y = this_ptr[173];
        float accum_z = this_ptr[174];

        float prev_x = this_ptr[176];
        float prev_y = this_ptr[177];
        float prev_z = this_ptr[178];

        // Impulse — correct original axis mapping
        accum_x += 0.025f * dt * clamped_b; // longitudinal
        accum_y += 0.025f * dt * clamped_a; // lateral

        // Linear decay
        accum_x -= prev_x * 7.0f * dt;
        accum_y -= prev_y * 3.0f * dt;
        accum_z -= prev_z * 0.0f * dt;

        // Exponential decay
        accum_x *= powf(0.017999999f, dt);
        accum_y *= powf(0.0099999998f, dt);
        accum_z *= powf(0.0f, dt);

        // Write back accumulators & integrated values
        this_ptr[172] = accum_x;
        this_ptr[173] = accum_y;
        this_ptr[174] = accum_z;

        this_ptr[176] = prev_x + accum_x;
        this_ptr[177] = prev_y + accum_y;
        this_ptr[178] = prev_z + accum_z;
    }

    // Interpolate between physics steps based on time remainder inside accumulator
    float alpha = accumulator / FIXED_DT;

    float interp_x = last_x + (this_ptr[176] - last_x) * alpha;
    float interp_y = last_y + (this_ptr[177] - last_y) * alpha;
    float interp_z = last_z + (this_ptr[178] - last_z) * alpha;

    out_offset[0] += interp_x * a5;
    out_offset[1] += interp_y * a5;
    out_offset[2] += interp_z * a5;

    // Matrix / reference position (must run every frame)
    float temp[4]{};
    auto get_matrix_func = (float* (__fastcall*)(void*, void*, float*))(*(uintptr_t*)(*(uintptr_t*)vehicle + 0xEC));
    float* ref = get_matrix_func(vehicle, 0, temp);

    this_ptr[180] = ref[0];
    this_ptr[181] = ref[1];
    this_ptr[182] = ref[2];
    this_ptr[183] = ref[3];

    // Angular velocity (must run every frame)
    float angvel[4];
    CPhysical::getAngularVelocity(vehicle, 0, angvel);

    this_ptr[184] = angvel[0];
    this_ptr[185] = angvel[1];
    this_ptr[186] = angvel[2];
    this_ptr[187] = angvel[3];
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

namespace CPhysics
{
    static constexpr int NUM_SLICES = 2;

    SafetyHookInline shUpdate = {};
    void __cdecl Update()
    {
        CPhysics::ScanForBuildings();
        CPhysics::UpdateRequestList();
        auto v0 = *CWorld::ms_listProcessControlPtrs;
        while (v0)
        {
            auto v1 = *(uintptr_t**)v0;
            v0 = *(uintptr_t*)(v0 + 4);
            if (v1)
            {
                auto v2 = (v1[10] >> 6) & 0xF;
                if (v2 > 1 && v2 < 5)
                    (*(void (__thiscall**)(uintptr_t*))(*v1 + 256))(v1);
            }
        }

        CPhysics::ResetNumPoolGameCollisions();

        float sliceDt = *CTimer::fTimeStep * (1.0f / (float)NUM_SLICES);

        if (bSpeedupSimRateCheat)
        {
            sliceDt *= 2.0f;
        }

        int NumTimeSlices = 0;
        do
        {
            CPhysics::PreSimUpdate(std::clamp(sliceDt, 1.0f / 150.0f, FLT_MAX), NumTimeSlices);
            CPhysics::SimUpdate(sliceDt);
            CPhysics::IterateOverManifolds();
            CPhysics::PostSimUpdate(NumTimeSlices, std::clamp(sliceDt, 1.0f / 150.0f, FLT_MAX));
            ++NumTimeSlices;
        } while (NumTimeSlices < NUM_SLICES);
    }
}

namespace rage
{
    namespace ptxEffectInst
    {
        SafetyHookInline shSetEvolutionTime = {};
        void __fastcall SetEvolutionTime(void* ptxEffectInst, void* edx, char* Key, float a3)
        {
            return shSetEvolutionTime.unsafe_fastcall(ptxEffectInst, edx, Key, a3 * (*CTimer::fTimeStep / (1.0f / 30.0f)));
        }
    }
}

namespace CWater
{
    SafetyHookInline shAddToDynamicWaterSpeed = {};
    void __cdecl AddToDynamicWaterSpeed(int a1, int a2, float a3, char a4)
    {
        return shAddToDynamicWaterSpeed.unsafe_ccall(a1, a2, a3 * (*CTimer::fTimeStep / (1.0f / 30.0f)), a4);
    }
}

class FramerateVigilante
{
public:
    FramerateVigilante()
    {
        FusionFix::onInitEventAsync() += []()
        {
            // Timestep clamp adjustment in CTimer::Initialise, fixes game speedup past 300fps, but not slowdown below 15fps as its kind of unnecessary
            auto pattern = hook::pattern("E8 ? ? ? ? FF 74 24 ? E8 ? ? ? ? E8");
            if (!pattern.empty())
            {
                injector::WriteMemory<float>(injector::GetBranchDestination(pattern.get_first(0)).as_int() + 6, 1.0f / 3000.0f, true);
            }
            else
            {
                static float dword_EDF6CC = 1.0f / 3000.0f;
                pattern = hook::pattern("E8 ? ? ? ? 8B 44 24 ? 50 E8 ? ? ? ? E8");
                injector::WriteMemory(injector::GetBranchDestination(pattern.get_first(0)).as_int() + 4, &dword_EDF6CC, true);
            }

            // Check 30FPS accumulator before calling CPedIntelligence::ProcessStaticCounter, which increments task attempt counter.
            // Some CTasks check this attempt counter against a hardcoded limit of 30.
            // At higher framerates these attempts occur faster, causing them to hit the limit early and abort the task.
            // (eg. causing NPCs to shove cars instead of walking around them)
            {
                // Hook ProcessStaticCounter to check/add to accumulator
                pattern = find_pattern("E8 ? ? ? ? 8B 4F ? 6A ? 8B 89 ? ? ? ? E8 ? ? ? ? 8D 9F", "E8 ? ? ? ? 8B 46 ? 8B 88 ? ? ? ? 6A");
                shProcessStaticCounter = safetyhook::create_inline(injector::GetBranchDestination(pattern.get_first()).as_int(), ProcessStaticCounter);
            }

            // CTaskComplexClimbLadder, change heading angle threshold passed to TaskAchieveHeading to match the threshold checked by CTaskSimpleSlideToCoord
            // Fixes wrong order of tasks being completed at high FPS, causing stall when trying to climb ladder.
            {
                pattern = hook::pattern("F3 0F 10 05 ? ? ? ? 83 EC ? 8B C8 F3 0F 11 44 24 ? F3 0F 10 44 24 ? C7 44 24 04 00 00 00 40");
                injector::MakeNOP(pattern.get_first(0), 8, true);
                static auto ClimbLadderThresholdValue = safetyhook::create_mid(pattern.get_first(0), [](SafetyHookContext& regs)
                {
                    regs.xmm0.f32[0] = 0.1f; // 0.02f -> 0.1f to match CTaskSimpleSlideToCoord
                });
            }

            // Handbrake Cam force
            pattern = find_pattern("E8 ? ? ? ? D9 5C 24 7C F3 0F 10 4C 24", "E8 ? ? ? ? D9 5C 24 70 F3 0F 10 44 24 ? F3 0F 58 86");
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

            // Physics
            pattern = hook::pattern("51 56 E8 ? ? ? ? E8");
            CPhysics::shUpdate = safetyhook::create_inline(pattern.get_first(0), CPhysics::Update);

            // Heli rotor break time
            {
                // Rear rotors
                pattern = hook::pattern("F3 0F 59 15 ? ? ? ? F3 0F 58 D0 F3 0F 10 87");
                if (!pattern.empty())
                {
                    // This is not effective for some reason
                    static auto dword_FE8830 = *pattern.get_first<float*>(4);
                    injector::MakeNOP(pattern.get_first(0), 8, true);
                    static auto CHeli_ApplyCollisionInternalHook1 = safetyhook::create_mid(pattern.get_first(0), [](SafetyHookContext& regs)
                    {
                        regs.xmm2.f32[0] *= *dword_FE8830 * *CTimer::fTimeStep / (1.0f / 30.0f);
                    });
                }
                else
                {
                    // Needs better approach to patch, doesn't work
                    pattern = hook::pattern("D8 0D ? ? ? ? DE C1 D8 4C 24 ? D9 5C 24 ? F3 0F 5C 44 24 ? 0F 2F C8 F3 0F 11 86 ? ? ? ? 72");
                    static auto dword_D74010 = *pattern.get_first<float*>(2);
                    injector::MakeNOP(pattern.get_first(0), 6, true);
                    struct CHeli_ApplyCollisionInternalHook1
                    {
                        void operator()(injector::reg_pack& regs)
                        {
                            float RearRotorBreakTime = *dword_D74010 * *CTimer::fTimeStep / (1.0f / 30.0f);
                            _asm {fmul dword ptr [RearRotorBreakTime]};
                        }
                    }; injector::MakeInline<CHeli_ApplyCollisionInternalHook1>(pattern.get_first(0), pattern.get_first(6));
                }

                // Main rotors
                pattern = hook::pattern("F3 0F 59 05 ? ? ? ? F3 0F 58 D0 F3 0F 10 87");
                if (!pattern.empty())
                {
                    // Works fine
                    static auto dword_FE8B08 = *pattern.get_first<float*>(4);
                    injector::MakeNOP(pattern.get_first(0), 8, true);
                    static auto CHeli_ApplyCollisionInternalHook2 = safetyhook::create_mid(pattern.get_first(0), [](SafetyHookContext& regs)
                    {
                        regs.xmm0.f32[0] *= *dword_FE8B08 * *CTimer::fTimeStep / (1.0f / 30.0f);
                    });
                }
                else
                {
                    // Needs better approach to patch, doesn't work
                    pattern = hook::pattern("D8 0D ? ? ? ? DE C1 D8 4C 24 ? D9 5C 24 ? F3 0F 5C 44 24 ? 0F 2F C8 F3 0F 11 86 ? ? ? ? 0F 82");
                    static auto dword_DB3010 = *pattern.get_first<float*>(2);
                    injector::MakeNOP(pattern.get_first(0), 6, true);
                    struct CHeli_ApplyCollisionInternalHook2
                    {
                        void operator()(injector::reg_pack& regs)
                        {
                            float MainRotorBreakTime = *dword_DB3010 * *CTimer::fTimeStep / (1.0f / 30.0f);
                            _asm {fmul dword ptr [MainRotorBreakTime]};
                        }
                    }; injector::MakeInline<CHeli_ApplyCollisionInternalHook2>(pattern.get_first(0), pattern.get_first(6));
                }
            }

            // Heli downwash effect
            {
                pattern = find_pattern("56 8B F1 8B 46 ? 85 C0 74 ? 8B 88 ? ? ? ? 85 C9 74 ? FF 74 24 ? E8 ? ? ? ? 85 C0 78 ? F3 0F 10 44 24",
                                       "56 8B F1 8B 46 ? 85 C0 74 ? 8B 80 ? ? ? ? 85 C0 74 ? 8B 4C 24 ? E8 ? ? ? ? 85 C0 7C ? F3 0F 10 44 24");
                rage::ptxEffectInst::shSetEvolutionTime = safetyhook::create_inline(pattern.get_first(0), rage::ptxEffectInst::SetEvolutionTime);

                pattern = find_pattern("83 3D ? ? ? ? ? 74 ? A1 ? ? ? ? 3B 05 ? ? ? ? 75 ? 83 3D ? ? ? ? ? 74 ? 33 C0 EB ? B8 ? ? ? ? 3A 44 24",
                                       "B8 ? ? ? ? 39 05 ? ? ? ? 74 ? 8B 0D ? ? ? ? 3B 0D ? ? ? ? 75 ? 83 3D ? ? ? ? ? 74 ? 33 C0 3A 44 24");
                CWater::shAddToDynamicWaterSpeed = safetyhook::create_inline(pattern.get_first(0), CWater::AddToDynamicWaterSpeed);

                pattern = hook::pattern("33 D2 F7 F7 85 D2 75");
                if (!pattern.empty())
                {
                    static auto CRippleManager_RegisterHook = safetyhook::create_mid(pattern.get_first(0), [](SafetyHookContext& regs)
                    {
                        float RippleScale = std::min(*CTimer::fTimeStep * 30.0f, 1.0f);
                        regs.edi = std::max((int)((float)regs.edi / RippleScale), 1);
                    });
                }
                else
                {
                    pattern = hook::pattern("33 D2 F7 F1 85 D2 75");
                    static auto CRippleManager_RegisterHook = safetyhook::create_mid(pattern.get_first(0), [](SafetyHookContext& regs)
                    {
                        float RippleScale = std::min(*CTimer::fTimeStep * 30.0f, 1.0f);
                        regs.ecx = std::max((int)((float)regs.ecx / RippleScale), 1);
                    });
                }
            }

            // Heli rotor speed
            {
                pattern = hook::pattern("F3 0F 59 05 ? ? ? ? F3 0F 59 C4 F3 0F 5C C8");
                if (!pattern.empty())
                {
                    static auto dword_1046AF0 = *pattern.get_first<float*>(4);
                    injector::MakeNOP(pattern.get_first(0), 8, true);
                    static auto HeliRotorSpeed1 = safetyhook::create_mid(pattern.get_first(0), [](SafetyHookContext& regs)
                    {
                        regs.xmm0.f32[0] *= *dword_1046AF0 * *CTimer::fTimeStep / (1.0f / 30.0f);
                    });
                }
                else
                {
                    pattern = hook::pattern("F3 0F 59 15 ? ? ? ? F3 0F 59 D4");
                    static auto dword_F46598 = *pattern.get_first<float*>(4);
                    injector::MakeNOP(pattern.get_first(0), 8, true);
                    static auto HeliRotorSpeed1 = safetyhook::create_mid(pattern.get_first(0), [](SafetyHookContext& regs)
                    {
                        regs.xmm2.f32[0] *= *dword_F46598 * *CTimer::fTimeStep / (1.0f / 30.0f);
                    });
                }

                pattern = hook::pattern("F3 0F 59 1D ? ? ? ? F3 0F 10 87 ? ? ? ? F3 0F 59 DC");
                if (!pattern.empty())
                {
                    static auto dword_1046AF4 = *pattern.get_first<float*>(4);
                    injector::MakeNOP(pattern.get_first(0), 8, true);
                    static auto HeliRotorSpeed2 = safetyhook::create_mid(pattern.get_first(0), [](SafetyHookContext& regs)
                    {
                        regs.xmm3.f32[0] *= *dword_1046AF4 * *CTimer::fTimeStep / (1.0f / 30.0f);
                    });
                }
                else
                {
                    pattern = hook::pattern("F3 0F 59 0D ? ? ? ? F3 0F 10 86 ? ? ? ? F3 0F 59 CC");
                    static auto dword_F46594 = *pattern.get_first<float*>(4);
                    injector::MakeNOP(pattern.get_first(0), 8, true);
                    static auto HeliRotorSpeed2 = safetyhook::create_mid(pattern.get_first(0), [](SafetyHookContext& regs)
                    {
                        regs.xmm1.f32[0] *= *dword_F46594 * *CTimer::fTimeStep / (1.0f / 30.0f);
                    });
                }
            }

            // Heli blinkers' speed
            pattern = hook::pattern("03 0D ? ? ? ? F3 0F 10 0D");
            if (!pattern.empty())
            {
                injector::MakeNOP(pattern.get_first(0), 6, true);
                static auto HeliBlinkersSpeed = safetyhook::create_mid(pattern.get_first(0), [](SafetyHookContext& regs)
                {
                    regs.ecx += *CTimer::m_snTimeInMilliseconds / (1000 / 30);
                });
            }
            else
            {
                pattern = hook::pattern("8B 15 ? ? ? ? F3 0F 10 15 ? ? ? ? F3 0F 10 0D");
                injector::MakeNOP(pattern.get_first(0), 6, true);
                static auto HeliBlinkersSpeed = safetyhook::create_mid(pattern.get_first(0), [](SafetyHookContext& regs)
                {
                    regs.edx = *CTimer::m_snTimeInMilliseconds / (1000 / 30);
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
                        regs.xmm0.f32[0] *= (1000.0f / 30.0f);
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
                        regs.xmm0.f32[0] *= (1000.0f / 30.0f);
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
                        regs.xmm1.f32[0] += *dword_E81598 * *(float*)(regs.esp + 0x34) * (1000.0f / 30.0f);
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
                        regs.xmm0.f32[0] += *flt_DEF584 * *(float*)(regs.esp + 0x44) * (1000.0f / 30.0f);
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

            // On foot camera shake
            game_rand = (decltype(game_rand))injector::GetBranchDestination(find_pattern("E8 ? ? ? ? F3 0F 10 4C 24 ? F3 0F 5C 4C 24 ? F3 0F 10 5C 24", "E8 ? ? ? ? F3 0F 10 4C 24 ? F3 0F 59 4C 24 ? F3 0F 59 4C 24").get_first()).as_int();
            dword_11F7060 = *find_pattern("83 3D ? ? ? ? ? F3 0F 10 05 ? ? ? ? F3 0F 59 C1", "83 3D ? ? ? ? ? F3 0F 10 05 ? ? ? ? F3 0F 59 05").get_first<uint32_t*>(2);
            dword_12088B4 = *find_pattern("A1 ? ? ? ? 3B 05 ? ? ? ? 75 ? 83 3D ? ? ? ? ? 75 ? A1", "A1 ? ? ? ? 3B 05 ? ? ? ? 75 ? 83 3D ? ? ? ? ? 75 ? 8B 0D ? ? ? ? DB 05").get_first<uint32_t*>(1);
            dword_1037720 = *find_pattern("83 3D ? ? ? ? ? 75 ? A1 ? ? ? ? 66 0F 6E C0", "83 3D ? ? ? ? ? 75 ? 8B 0D ? ? ? ? DB 05").get_first<uint32_t*>(2);
            dword_11F704C = *find_pattern("A1 ? ? ? ? 66 0F 6E C0 F3 0F E6 C0 C1 E8 ? F2 0F 58 04 C5 ? ? ? ? 66 0F 5A C0 F3 0F 59 05 ? ? ? ? F3 0F 59 C1", "0D ? ? ? ? DB 05 ? ? ? ? 85 C9 7D ? D8 05 ? ? ? ? D8 0D").get_first<uint32_t*>(1);
            pattern = find_pattern("55 8B EC 83 E4 ? 83 EC ? 56 57 8B F9 F3 0F 10 05", "55 8B EC 83 E4 ? 0F 57 E4 F3 0F 10 1D");
            shOnFootCameraShake = safetyhook::create_inline(pattern.get_first(), OnFootCameraShake);

            // Hood camera bumping
            pattern = find_pattern("55 8B EC 83 E4 F0 83 EC 28 F3 0F 10 05 ? ? ? ? 56 8B 75 ? 57 6A 00", "55 8B EC 83 E4 F0 83 EC 24 F3 0F 10 05 ? ? ? ? 53 8B 5D ? 56 57 8B 7D");
            shHoodCameraBumping = safetyhook::create_inline(pattern.get_first(), HoodCameraBumping);


            // Native patches
            hbSET_CAM_FOV.fun = NativeOverride::Register(Natives::NativeHashes::SET_CAM_FOV, NATIVE_SET_CAM_FOV, "E8 ? ? ? ? 83 C4 08 C3", 30);
            hbSLIDE_OBJECT.fun = NativeOverride::Register(Natives::NativeHashes::SLIDE_OBJECT, NATIVE_SLIDE_OBJECT_1, "E8 ? ? ? ? 0F B6 C8", 107);
            if (!hbSLIDE_OBJECT.fun)
            {
                pattern = hook::pattern("55 8B EC 83 E4 F0 8B 45 08 8B 0D ? ? ? ? 81 EC ? ? ? ? 56 50");
                shNATIVE_SLIDE_OBJECT = safetyhook::create_inline(pattern.get_first(0), NATIVE_SLIDE_OBJECT_2);
            }


            // Drunk steering bias feedback loop fix
            // In ProcessControlInputs, the drunk steering bias (m_fSteerBias) is added to
            // the smoothed steering value and stored back to m_fSteer. This creates a
            // feedback loop where the bias accumulates through the exponential smoothing
            // filter each frame, causing far more steering drift than intended.
            // Fix: After m_fSteerAngle is computed from the biased m_fSteer, subtract the
            // bias from m_fSteer so the next frame's smoothing uses the unbiased value.
            // CAutomobile adds the full bias; CBike adds bias * 0.5.
            {
                static ptrdiff_t SteerOffset;
                static ptrdiff_t SteerBiasOffset;

                static auto getVehicleFromModRM = [](uint8_t modrm, SafetyHookContext& regs) -> uintptr_t
                {
                    switch (modrm & 7)
                    {
                        case 0: return regs.eax;
                        case 1: return regs.ecx;
                        case 2: return regs.edx;
                        case 3: return regs.ebx;
                        case 6: return regs.esi;
                        case 7: return regs.edi;
                        default: return 0;
                    }
                };

                // CAutomobile: movss [reg+SteerAngleOffset], xmm? ; test byte ptr [reg+F0h], 40h
                auto pattern = find_pattern(
                    "F3 0F 11 ? 88 10 00 00 F6 ? F0 00 00 00 40",
                    "F3 0F 11 ? D8 10 00 00 F6 ? F0 00 00 00 40"
                );
                if (!pattern.empty())
                {
                    if (!hook::pattern("F3 0F 11 ? 88 10 00 00 F6 ? F0 00 00 00 40").count_hint(1).empty())
                    {
                        SteerOffset = 0x1080;
                        SteerBiasOffset = 0x1084;
                    }
                    else
                    {
                        SteerOffset = 0x10D0;
                        SteerBiasOffset = 0x10D4;
                    }

                    static uint8_t autoVehicleReg = *(uint8_t*)((uintptr_t)pattern.get_first(3)) & 7;

                    // Mid-hook at the test byte instruction (offset +8), after steer angle is stored
                    static auto AutoDrunkSteerBiasHook = safetyhook::create_mid(
                        (uint8_t*)pattern.get_first(8),
                        [](SafetyHookContext& regs)
                        {
                            uintptr_t vehicle = getVehicleFromModRM(autoVehicleReg, regs);
                            if (!vehicle) return;

                            float bias = *(float*)(vehicle + SteerBiasOffset);
                            if (bias != 0.0f)
                            {
                                float& steer = *(float*)(vehicle + SteerOffset);
                                steer -= bias;
                                if (steer > 1.0f) steer = 1.0f;
                                else if (steer < -1.0f) steer = -1.0f;
                            }
                        });
                }

                // CBike: movss [reg+SteerAngleOffset], xmm? ; cmp byte ptr [CFrontEnd::bActive], 0
                // CBike adds bias * 0.5 instead of the full bias.
                auto pattern2 = find_pattern(
                    "F3 0F 11 ? 88 10 00 00 80 3D",
                    "F3 0F 11 ? D8 10 00 00 80 3D"
                );
                if (!pattern2.empty())
                {
                    if (SteerOffset == 0)
                    {
                        if (!hook::pattern("F3 0F 11 ? 88 10 00 00 80 3D").count_hint(1).empty())
                        {
                            SteerOffset = 0x1080;
                            SteerBiasOffset = 0x1084;
                        }
                        else
                        {
                            SteerOffset = 0x10D0;
                            SteerBiasOffset = 0x10D4;
                        }
                    }

                    static uint8_t bikeVehicleReg = *(uint8_t*)((uintptr_t)pattern2.get_first(3)) & 7;

                    // Mid-hook at the cmp instruction (offset +8), after steer angle is stored
                    static auto BikeDrunkSteerBiasHook = safetyhook::create_mid(
                        (uint8_t*)pattern2.get_first(8),
                        [](SafetyHookContext& regs)
                        {
                            uintptr_t vehicle = getVehicleFromModRM(bikeVehicleReg, regs);
                            if (!vehicle) return;

                            float bias = *(float*)(vehicle + SteerBiasOffset);
                            if (bias != 0.0f)
                            {
                                float& steer = *(float*)(vehicle + SteerOffset);
                                steer -= bias * 0.5f;
                                if (steer > 1.0f) steer = 1.0f;
                                else if (steer < -1.0f) steer = -1.0f;
                            }
                        });
                }
            }
        };
    }
} FramerateVigilante;
