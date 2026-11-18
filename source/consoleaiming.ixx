module;

#include <common.hxx>

export module consoleaiming;

import common;
import comvars;
import settings;

class ConsoleAiming
{
public:
    ConsoleAiming()
    {
        FusionFix::onInitEventAsync() += []()
        {
            SettingsReader reader;
            if (!reader.ReadInteger("MAIN", "ConsoleControllerAiming", 1))
                return;

            auto shape = find_pattern<2>("80 BF 8D 32 00 00 00 A1");
            auto smooth = find_pattern<2>(
                "F3 0F 11 54 24 18 F3 0F 11 54 24 1C F3 0F 11 4C 24 10 84 C0",
                "F3 0F 11 4C 24 14 F3 0F 11 44 24 10 74");
            if (shape.size() != 1 || smooth.size() != 1)
                return;

            // The legacy yaw store uses XMM1. Complete Edition uses XMM2.
            static const bool legacy = smooth.get_first<uint8_t>()[3] == 0x4C;
            static auto CCamAimWeapon__AimFree_ShapeHook = safetyhook::create_mid(shape.get_first(), [](SafetyHookContext& regs)
            {
                if (*(uint8_t*)(regs.edi + 0x3289))
                    return;

                // Apply the console curve after the deadzone and inversion.
                // The native boost check still uses the original stick magnitude.
                const float magnitude = legacy ? regs.xmm0.f32[0] : regs.xmm1.f32[0];
                const float scale = sqrtf(magnitude);
                *(float*)(regs.esp + 0x14) *= scale;
                *(float*)(regs.esp + (legacy ? 0x10 : 0xC)) *= scale;
            });
            if (!CCamAimWeapon__AimFree_ShapeHook)
                return;

            static auto CCamAimWeapon__AimFree_SmoothHook = safetyhook::create_mid(smooth.get_first(), [](SafetyHookContext& regs)
            {
                auto& pitchRate = *(float*)(regs.esi + 0x220);
                auto& yawRate = *(float*)(regs.esi + 0x224);
                if (*(uint8_t*)(regs.edi + 0x3289))
                {
                    // Clear the stored rates when switching to mouse input.
                    pitchRate = yawRate = 0.0f;
                    return;
                }

                auto& yaw = legacy ? regs.xmm1.f32[0] : regs.xmm2.f32[0];
                auto& pitch = legacy ? regs.xmm0.f32[0] : regs.xmm1.f32[0];
                // ms_AimWeaponFreeStickRateDown retains 30% per 1/50 second of game time.
                const float retention = powf(0.3f, 50.0f * *CTimer::fTimeStep);
                const float response = 1.0f - retention;
                yaw = yawRate = yawRate * retention + yaw * response;
                pitch = pitchRate = pitchRate * retention + pitch * response;
            });
            if (!CCamAimWeapon__AimFree_SmoothHook)
                CCamAimWeapon__AimFree_ShapeHook.reset();
        };
    }
} ConsoleAiming;
