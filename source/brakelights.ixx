module;

#include <common.hxx>
#include <cmath>

export module brakelights;

import common;
import comvars;

static constexpr float fStoppedSpeed = 0.5f; // matches the games own pedal split threshold
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
            if (!iniReader.ReadInteger("MISC", "BrakeLightsWhenStopped", 1))
                return;

            auto pattern = find_pattern("F3 0F 10 86 ? ? 00 00 0F 2F 05 ? ? ? ? 77 ? 80 FE 01 75 ? 84 D2 75 ? 83 BE ? ? 00 00 00 74 ? F6 86 ? ? 00 00 08");
            if (pattern.empty() || !CPlayer::getLocalPlayerPed || !CPhysical::GetLocalSpeed)
                return;

            static const ptrdiff_t nBrakePedal = *pattern.get_first<uint32_t>(4);
            static const ptrdiff_t nGasPedal = nBrakePedal - sizeof(float);
            static const ptrdiff_t nDriver = *pattern.get_first<uint32_t>(28);

            static auto BrakeLightTestHook = safetyhook::create_mid(pattern.get_first(8), [](SafetyHookContext& regs)
            {
                if (regs.xmm0.f32[0] > 0.1f)
                    return; // already braking, vanilla handles it

                auto pVehicle = static_cast<uintptr_t>(regs.esi);
                auto pDriver = *(uintptr_t*)(pVehicle + nDriver);
                if (!pDriver || pDriver != CPlayer::getLocalPlayerPed())
                    return; // AI vehicles already light theirs correctly

                if (*(float*)(pVehicle + nGasPedal) > fIdleThrottle)
                    return; // pulling away

                if (std::fabs(GetForwardSpeed(pVehicle)) >= fStoppedSpeed)
                    return; // still rolling, or reversing under power

                regs.xmm0.f32[0] = 1.0f;
            });
        };
    }
} BrakeLights;
