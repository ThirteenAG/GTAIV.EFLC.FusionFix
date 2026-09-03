module;

#include <common.hxx>
#include <cmath>

export module brakelights;

import common;
import comvars;

static constexpr float fStoppedSpeed = 0.5f; // Matches the game's own pedal split threshold
static constexpr float fIdleThrottle = 0.01f;

static float GetForwardSpeed(uintptr_t pVehicle)
{
    float vecSpeed[4]{};
    float vecOffset[4]{};

    CPhysical::GetLocalSpeed((void*)pVehicle, nullptr, vecSpeed, vecOffset, 0, 0);

    auto matrix = *(float**)(pVehicle + 0x20);

    return vecSpeed[0] * matrix[4] + vecSpeed[1] * matrix[5] + vecSpeed[2] * matrix[6];
}

class BrakeLights
{
public:
    BrakeLights()
    {
        FusionFix::onInitEventAsync() += []()
        {
            CIniReader iniReader("");

            // [MISC]
            auto bBrakeLightsWhenStopped = iniReader.ReadInteger("MISC", "BrakeLightsWhenStopped", 0) != 0;

            if (bBrakeLightsWhenStopped)
            {
                auto pattern = find_pattern("F3 0F 10 86 ? ? ? ? 0F 2F 05 ? ? ? ? 77 ? 80 FE", "F3 0F 10 83 ? ? ? ? 0F 2F 05 ? ? ? ? 77 ? 80 7C 24");
                static auto reg = *pattern.get_first<uint8_t>(3);

                static ptrdiff_t nBrakePedal = 0;
                static ptrdiff_t nGasPedal = 0;
                static ptrdiff_t nDriver = 0;

                if (reg == 0x86)
                {
                    nBrakePedal = *pattern.get_first<uint32_t>(4);
                    nGasPedal = nBrakePedal - sizeof(float);
                    nDriver = *pattern.get_first<uint32_t>(28);
                }
                else
                {
                    nBrakePedal = *pattern.get_first<uint32_t>(4);
                    nGasPedal = nBrakePedal - sizeof(float);
                    nDriver = *pattern.get_first<uint32_t>(33);
                }

                static auto CVehicle__DoVehicleLights_Hook = safetyhook::create_mid(pattern.get_first(8), [](SafetyHookContext& regs)
                {
                    // Already braking, vanilla handles it
                    if (regs.xmm0.f32[0] > 0.1f)
                        return;

                    auto pVehicle = (reg != 0x83) ? static_cast<uintptr_t>(regs.esi) : static_cast<uintptr_t>(regs.ebx);
                    auto pDriver = *(uintptr_t*)(pVehicle + nDriver);

                    // AI vehicles already light theirs correctly
                    if (!pDriver || pDriver != CPlayer::getLocalPlayerPed())
                        return;

                    // Pulling away
                    if (*(float*)(pVehicle + nGasPedal) > fIdleThrottle)
                        return;

                    // Still rolling, or reversing under power
                    if (std::fabs(GetForwardSpeed(pVehicle)) >= fStoppedSpeed)
                        return;

                    regs.xmm0.f32[0] = 1.0f;
                });
            }
        };
    }
} BrakeLights;