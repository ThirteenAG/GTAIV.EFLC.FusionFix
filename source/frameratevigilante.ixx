module;

#include <common.hxx>

export module frameratevigilante;

import common;
import comvars;
import settings;
import natives;

injector::hook_back<double(__fastcall*)(void* _this, void* edx, void* a2, void* a3)> hbsub_A18510;
double __fastcall sub_A18510(void* _this, void* edx, void* a2, void* a3)
{
    return hbsub_A18510.fun(_this, edx, a2, a3) * (*CTimer::fTimeStep / (1.0f / 30.0f));
}

int (__cdecl *game_rand)() = nullptr;
uint32_t* dword_11F7060 = nullptr;
uint32_t* dword_12088B4 = nullptr;
uint32_t* dword_1037720 = nullptr;
uint32_t* dword_11F704C = nullptr;
SafetyHookInline shCameraShake = {};
void __fastcall CameraShake(float* shakeData, void* edx, float multiplier)
{
    static auto cs = FusionFixSettings.GetRef("PREF_CAMERASHAKE");
    if (!cs->get())
    {
        float finalRot[] = { 0.0f, 0.0f, 0.0f };
        return Matrix34::fromEulersXYZ(shakeData, 0, finalRot);
    }

    float& rotX = shakeData[16];
    float& rotY = shakeData[17];
    float& rotZ = shakeData[18];

    float maxX = shakeData[20];
    float maxY = shakeData[21];
    float maxZ = shakeData[22];

    float intensityX = shakeData[24];
    float intensityY = shakeData[25];
    float intensityZ = shakeData[26];

    float& velX = shakeData[28];
    float& velY = shakeData[29];
    float& velZ = shakeData[30];

    float dampX = shakeData[32];
    float dampY = shakeData[33];
    float dampZ = shakeData[34];

    float minShake = shakeData[36];
    float maxShake = shakeData[37];

    float impulseFreq = shakeData[38];
    float impulseAmplitude = shakeData[39];

    // Get the actual time passed since the last frame
    float deltaTime = *CTimer::fTimeStep;
    if (*dword_11F7060 == 1 || *dword_12088B4 != -1 || *dword_1037720 == 18)
    {
        // Alternative timing path
        deltaTime = (*dword_11F704C * 0.001f);
    }

    // Create a frame-rate independent scaling factor.
    float timeScale = deltaTime * 30.0f;

    // Compute per-axis shake range based on rotation
    float rangeX = fabsf(rotX / maxX) * (maxShake - minShake) + minShake;
    float rangeY = fabsf(rotY / maxY) * (maxShake - minShake) + minShake;
    float rangeZ = fabsf(rotZ / maxZ) * (maxShake - minShake) + minShake;

    // Apply damping if rotation & velocity have the same sign
    if ((rotX > 0.0f && velX > 0.0f) || (rotX < 0.0f && velX < 0.0f))
        rangeX *= dampX;
    if ((rotY > 0.0f && velY > 0.0f) || (rotY < 0.0f && velY < 0.0f))
        rangeY *= dampY;
    if ((rotZ > 0.0f && velZ > 0.0f) || (rotZ < 0.0f && velZ < 0.0f))
        rangeZ *= dampZ;

    // Generate 3 random factors
    float randX = game_rand() * (1.0f / 32768.0f);
    float randY = game_rand() * (1.0f / 32768.0f);
    float randZ = game_rand() * (1.0f / 32768.0f);

    // Calculate shake forces (these are impulses, so they should not be scaled by time here)
    float forceX = randX * intensityX * rangeX;
    float forceY = randY * intensityY * rangeY;
    float forceZ = randZ * intensityZ * rangeZ;

    // Reverse direction if rotation is positive
    if (rotX > 0.0f) forceX = -forceX;
    if (rotY > 0.0f) forceY = -forceY;
    if (rotZ > 0.0f) forceZ = -forceZ;

    // Update velocities. Scale the applied force by timeScale.
    velX += forceX * timeScale;
    velY += forceY * timeScale;
    velZ += forceZ * timeScale;

    // Impulse probability
    // The original code's logic was flawed for different frame rates.
    // A time-based probability is much more reliable.
    float impulseChance = deltaTime * impulseFreq;
    float randVal = (game_rand() & 0xFFFF) * (1.0f / 32768.0f);

    // If random check passes, add impulse. Scale the impulse by timeScale as well.
    if (randVal < impulseChance)
    {
        float randImpulseX = ((game_rand() * (1.0f / 32768.0f) * 2.0f) - 1.0f) * impulseAmplitude;
        float randImpulseY = ((game_rand() * (1.0f / 32768.0f) * 2.0f) - 1.0f) * impulseAmplitude;
        float randImpulseZ = ((game_rand() * (1.0f / 32768.0f) * 2.0f) - 1.0f) * impulseAmplitude;

        velX += randImpulseX * timeScale;
        velY += randImpulseY * timeScale;
        velZ += randImpulseZ * timeScale;
    }

    // Integrate velocities into rotation. Also use timeScale.
    rotX += velX * timeScale;
    rotY += velY * timeScale;
    rotZ += velZ * timeScale;

    // Clamp rotations
    if (rotX < -maxX) rotX = -maxX;
    if (rotX > maxX) rotX = maxX;
    if (rotY < -maxY) rotY = -maxY;
    if (rotY > maxY) rotY = maxY;
    if (rotZ < -maxZ) rotZ = -maxZ;
    if (rotZ > maxZ) rotZ = maxZ;

    // Apply final multiplier
    float finalRot[] = {
        rotX * multiplier,
        rotY * multiplier,
        rotZ * multiplier
    };

    // Update matrix
    Matrix34::fromEulersXYZ(shakeData, 0, finalRot);
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
        float time_scaled_increment = desired_increment * (*CTimer::fTimeStep * 30.0f);
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
bool __cdecl NATIVE_SLIDE_OBJECT(Object object, float toX, float toY, float toZ, float speedX, float speedY, float speedZ, char collision)
{
    float delta = *CTimer::fTimeStep * 30.0f;
    return hbSLIDE_OBJECT.fun(object, toX, toY, toZ, speedX * delta, speedY * delta, speedZ * delta, collision);
}

class FramerateVigilante
{
public:
    FramerateVigilante()
    {
        FusionFix::onInitEventAsync() += []()
        {
            // Handbrake Cam (test)
            auto pattern = find_pattern("E8 ? ? ? ? D9 5C 24 7C F3 0F 10 4C 24", "E8 ? ? ? ? D9 5C 24 70 F3 0F 10 44 24 ? F3 0F 58 86");
            hbsub_A18510.fun = injector::MakeCALL(pattern.get_first(0), sub_A18510).get();

            // Bikes (By Sergeanur)
            pattern = hook::pattern("F3 0F 10 45 ? 51 8B CF F3 0F 11 04 24 E8 ? ? ? ? 8A 8F");
            if (!pattern.empty())
            {
                struct FramerateVigilanteHook1
                {
                    void operator()(injector::reg_pack& regs)
                    {
                        float f = std::clamp(*(float*)(regs.ebp + 0x08), 1.0f / 150.0f, FLT_MAX);
                        *(float*)(regs.ebp + 0x08) = f;
                        regs.xmm0.f32[0] = f;
                    }
                }; injector::MakeInline<FramerateVigilanteHook1>(pattern.get_first(0));
            }
            else
            {
                pattern = hook::pattern("8B BE ? ? ? ? 33 C9 85 FF 7E 47 8A 5D 0C 33 D2 8D A4 24 ? ? ? ? 3B CF 7D 0A 8B 86 ? ? ? ? 03 C2 EB 02 33 C0 F6 80 ? ? ? ? ? 74 11 83 B8 ? ? ? ? ? 74 08 84 DB 0F 85 ? ? ? ? 83 C1 01 81 C2 ? ? ? ? 3B 8E ? ? ? ? 7C C5 D9 45 08 51 8B CE D9 1C 24 E8 ? ? ? ? 8A 86");
                struct FramerateVigilanteHook1
                {
                    void operator()(injector::reg_pack& regs)
                    {
                        regs.edi = *(uint32_t*)(regs.esi + 0xFD4);
                        *(float*)(regs.ebp + 0x08) = std::clamp(*(float*)(regs.ebp + 0x08), 1.0f / 150.0f, FLT_MAX);
                    }
                }; injector::MakeInline<FramerateVigilanteHook1>(pattern.get_first(0), pattern.get_first(6));
            }

            pattern = hook::pattern("F3 0F 10 05 ? ? ? ? F3 0F 58 C1 F3 0F 11 05 ? ? ? ? EB 36");
            if (!pattern.empty())
            {
                static auto f1032790 = *pattern.get_first<float*>(4);
                struct LoadingTextSpeed
                {
                    void operator()(injector::reg_pack& regs)
                    {
                        regs.xmm0.f32[0] = *f1032790 * *CTimer::fTimeStep;
                        regs.xmm0.f32[0] += regs.xmm1.f32[0];
                    }
                }; injector::MakeInline<LoadingTextSpeed>(pattern.get_first(0), pattern.get_first(12));
            }

            pattern = hook::pattern("F3 0F 58 0D ? ? ? ? 0F 5B C0 F3 0F 11 0D");
            if (!pattern.empty())
            {
                struct LoadingTextSparks
                {
                    void operator()(injector::reg_pack& regs)
                    {
                        regs.xmm1.f32[0] += 0.085f * *CTimer::fTimeStep;
                    }
                }; injector::MakeInline<LoadingTextSparks>(pattern.get_first(0), pattern.get_first(8));
            }
            else
            {
                pattern = hook::pattern("F3 0F 58 05 ? ? ? ? F3 0F 2A 0D");
                struct LoadingTextSparks
                {
                    void operator()(injector::reg_pack& regs)
                    {
                        regs.xmm0.f32[0] += 0.085f * *CTimer::fTimeStep;
                    }
                }; injector::MakeInline<LoadingTextSparks>(pattern.get_first(0), pattern.get_first(8));
            }

            // CD/busy spinner
            pattern = hook::pattern("F3 0F 58 05 ? ? ? ? 33 C0 A3 ? ? ? ? F3 0F 11 05");
            if (!pattern.empty())
            {
                struct CDSpinnerHook
                {
                    void operator()(injector::reg_pack& regs)
                    {
                        regs.xmm0.f32[0] += *CTimer::fTimeStep * 5.0f;
                    }
                }; injector::MakeInline<CDSpinnerHook>(pattern.get_first(0), pattern.get_first(8));
            }
            
            // Cop blips
            pattern = hook::pattern("F3 0F 10 4C 24 ? 0F 28 C1 F3 0F 59 C2");
            if (!pattern.empty())
            {
                static int CustomFrameCounter = 0;

                static auto CounterHook = safetyhook::create_mid(pattern.get_first(), [](SafetyHookContext& regs)
                {
                    static float accumulator = 0.0f;
                    accumulator += (*CTimer::fTimeStep / (1.0f / 30.0f));
                    int increment = static_cast<int>(accumulator);
                    CustomFrameCounter += increment;
                    accumulator -= increment;
                });

                pattern = hook::pattern("A1 ? ? ? ? 6B C0 15");
                if (!pattern.empty())
                    injector::WriteMemory(pattern.get_first(1), &CustomFrameCounter, true);
            }

            // Camera Shake
            game_rand = (decltype(game_rand))injector::GetBranchDestination(find_pattern("E8 ? ? ? ? F3 0F 10 4C 24 ? F3 0F 5C 4C 24 ? F3 0F 10 5C 24", "E8 ? ? ? ? F3 0F 10 4C 24 ? F3 0F 59 4C 24 ? F3 0F 59 4C 24").get_first()).as_int();
            dword_11F7060 = *find_pattern("83 3D ? ? ? ? ? F3 0F 10 05 ? ? ? ? F3 0F 59 C1", "83 3D ? ? ? ? ? F3 0F 10 05 ? ? ? ? F3 0F 59 05").get_first<uint32_t*>(2);
            dword_12088B4 = *find_pattern("A1 ? ? ? ? 3B 05 ? ? ? ? 75 ? 83 3D ? ? ? ? ? 75 ? A1", "A1 ? ? ? ? 3B 05 ? ? ? ? 75 ? 83 3D ? ? ? ? ? 75 ? 8B 0D ? ? ? ? DB 05").get_first<uint32_t*>(1);
            dword_1037720 = *find_pattern("83 3D ? ? ? ? ? 75 ? A1 ? ? ? ? 66 0F 6E C0", "83 3D ? ? ? ? ? 75 ? 8B 0D ? ? ? ? DB 05").get_first<uint32_t*>(2);
            dword_11F704C = *find_pattern("A1 ? ? ? ? 66 0F 6E C0 F3 0F E6 C0 C1 E8 ? F2 0F 58 04 C5 ? ? ? ? 66 0F 5A C0 F3 0F 59 05 ? ? ? ? F3 0F 59 C1", "0D ? ? ? ? DB 05 ? ? ? ? 85 C9 7D ? D8 05 ? ? ? ? D8 0D").get_first<uint32_t*>(1);
            pattern = find_pattern("55 8B EC 83 E4 ? 83 EC ? 56 57 8B F9 F3 0F 10 05", "55 8B EC 83 E4 ? 0F 57 E4 F3 0F 10 1D");
            shCameraShake = safetyhook::create_inline(pattern.get_first(), CameraShake);

            // Natives
            hbSET_CAM_FOV.fun = NativeOverride::Register(Natives::NativeHashes::SET_CAM_FOV, NATIVE_SET_CAM_FOV, "E8 ? ? ? ? 83 C4 08 C3", 30);
            hbSLIDE_OBJECT.fun = NativeOverride::Register(Natives::NativeHashes::SLIDE_OBJECT, NATIVE_SLIDE_OBJECT, "E8 ? ? ? ? 0F B6 C8", 107);
        };
    }
} FramerateVigilante;