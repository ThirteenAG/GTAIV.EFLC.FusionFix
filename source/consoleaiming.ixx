module;

#include <common.hxx>

export module consoleaiming;

import common;
import comvars;

// The PC version of CCamAimWeapon::AimFree lost the console's stick response curve and rate smoothing for controller
// free aim. The camera still has the rate fields (+0x220, +0x224), they're just never written there.
class ConsoleAiming
{
public:
    ConsoleAiming()
    {
        FusionFix::onInitEventAsync() += []()
        {
            static bool legacy = false;
            auto pattern = hook::pattern("F3 0F 11 54 24 18 F3 0F 11 54 24 1C F3 0F 11 4C 24 10 84 C0");
            if (pattern.empty()) // 1.0.7.0, 1.0.8.0, 1.1.2.0, 1.1.3.0
            {
                legacy = true;
                pattern = hook::pattern("F3 0F 11 4C 24 14 F3 0F 11 44 24 10 74");
            }
            static auto CCamAimWeapon__AimFree_SmoothHook = safetyhook::create_mid(pattern.get_first(), [](SafetyHookContext& regs)
            {
                auto& pitchRate = *(float*)(regs.esi + 0x220);
                auto& yawRate = *(float*)(regs.esi + 0x224);
                // CControl: mouse input
                if (*(uint8_t*)(regs.edi + 0x3289))
                {
                    pitchRate = yawRate = 0.0f;
                    return;
                }

                auto& yaw = legacy ? regs.xmm1.f32[0] : regs.xmm2.f32[0];
                auto& pitch = legacy ? regs.xmm0.f32[0] : regs.xmm1.f32[0];
                // The rate keeps 30% per 1/50 s of game time
                const float retention = powf(0.3f, 50.0f * *CTimer::fTimeStep);
                const float response = 1.0f - retention;
                yaw = yawRate = yawRate * retention + yaw * response;
                pitch = pitchRate = pitchRate * retention + pitch * response;
            });

            pattern = hook::pattern("80 BF 8D 32 00 00 00 A1");
            static auto CCamAimWeapon__AimFree_ShapeHook = safetyhook::create_mid(pattern.get_first(), [](SafetyHookContext& regs)
            {
                if (*(uint8_t*)(regs.edi + 0x3289))
                    return;

                // Stick curve after the deadzone and inversion; the native boost check still uses the original magnitude
                const float magnitude = legacy ? regs.xmm0.f32[0] : regs.xmm1.f32[0];
                const float scale = sqrtf(magnitude);
                *(float*)(regs.esp + 0x14) *= scale;
                *(float*)(regs.esp + (legacy ? 0x10 : 0xC)) *= scale;
            });
        };
    }
} ConsoleAiming;
