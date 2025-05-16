module;

#include <common.hxx>

export module sniper;

import common;
import comvars;
import settings;

float __cdecl sub_8EFA20(uint8_t* a1)
{
    auto input = (float)((uint8_t)(a1[4] ^ a1[6]));

    static auto zm = FusionFixSettings.GetRef("PREF_ZOOMEDMOVEMENT");
    if (zm->get())
    {
        input = 127.5f;

        auto ptr = a1 - 0x2838;
        if (!*dwSniperInverted)
            ptr = a1 - 0x2818;

        auto dpad_down_pressed = *(uint8_t*)(ptr + 0x27EE) > 0;
        auto dpad_up_pressed = *(uint8_t*)(ptr + 0x27FE) > 0;

        if (dpad_up_pressed)
            input = 255.0f;
        else if (dpad_down_pressed)
            input = 0.0f;
    }

    float normalized_value = (input - 127.5f) * (1.0f / 127.5f);

    float clamped_value = -1.0f;
    if (normalized_value >= -1.0f)
    {
        clamped_value = 1.0f;
        if (normalized_value <= 1.0f)
        {
            clamped_value = normalized_value;
        }
    }

    const float threshold = 0.25f;
    const float denominator = 1.0f - threshold;

    float calculation_basis;

    if (clamped_value > threshold)
    {
        calculation_basis = clamped_value - threshold;
        return calculation_basis / denominator;
    }

    if (clamped_value < -threshold)
    {
        calculation_basis = clamped_value + threshold;
        return calculation_basis / denominator;
    }

    return 0.0f;
}

class Sniper
{
public:
    Sniper()
    {
        FusionFix::onInitEventAsync() += []()
        {
            auto pattern = find_pattern("0F 85 ? ? ? ? 84 DB 0F 85 ? ? ? ? 8B CE", "0F 85 ? ? ? ? 84 DB 0F 85 ? ? ? ? 8B CF");
            //injector::WriteMemory<uint16_t>(pattern.get_first(0), 0xE990, true); // jz -> jmp
            static uintptr_t loc_A75BB0 = (uintptr_t)find_pattern("83 3D ? ? ? ? ? 75 ? 8B 07 68", "83 3D ? ? ? ? ? 75 ? 8B 06 8B 50").get_first(0);
            struct DisableSniperWeaponTypeCheck
            {
                void operator()(injector::reg_pack& regs)
                {
                    static auto zm = FusionFixSettings.GetRef("PREF_ZOOMEDMOVEMENT");
                    if (zm->get())
                    {
                        *(uintptr_t*)(regs.esp - 4) = loc_A75BB0;
                    }
                }
            }; injector::MakeInline<DisableSniperWeaponTypeCheck>(pattern.get_first(0), pattern.get_first(6));

            // Swap aiming with controller
            pattern = find_pattern("E8 ? ? ? ? D9 5C 24 ? 8B 4C 24 ? F3 0F 10 54 24 ? 83 C4", "E8 ? ? ? ? D9 5C 24 ? F3 0F 10 4C 24 ? 83 C4 ? 0F 57 C0 0F 2F C1");
            injector::MakeCALL(pattern.get_first(0), sub_8EFA20, true);
        };
    }
} Sniper;