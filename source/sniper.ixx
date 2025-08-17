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
            static auto loc_A75BB0 = resolve_next_displacement(pattern.get_first(0)).value();
            struct DisableSniperWeaponTypeCheck
            {
                void operator()(injector::reg_pack& regs)
                {
                    static auto zm = FusionFixSettings.GetRef("PREF_ZOOMEDMOVEMENT");
                    if (zm->get() || (!zm->get() && *(uint32_t*)(regs.eax + 4) != 6))
                    {
                        force_return_address(loc_A75BB0);
                    }
                }
            }; injector::MakeInline<DisableSniperWeaponTypeCheck>(pattern.get_first(0), pattern.get_first(6));

            // Swap aiming with controller
            pattern = find_pattern("E8 ? ? ? ? D9 5C 24 ? 8B 4C 24 ? F3 0F 10 54 24 ? 83 C4", "E8 ? ? ? ? D9 5C 24 ? F3 0F 10 4C 24 ? 83 C4 ? 0F 57 C0 0F 2F C1");
            injector::MakeCALL(pattern.get_first(0), sub_8EFA20, true);

            // Extended sniper controls
            static auto OverrideSniperFlags = [](SafetyHookContext& regs)
            {
                static auto esc = FusionFixSettings.GetRef("PREF_EXTENDEDSNIPERCONTROLS");
                if (esc->get())
                {
                    if (regs.eax & 8)
                    {
                        if (!bZoomingWithSniperNow && !bInSniperScope)
                        {
                            regs.eax ^= 8;
                        }
                    }
                }
            };

            pattern = find_pattern("8B 40 ? C1 E8 ? A8 ? 8B 44 24 ? 74");
            if (!pattern.empty())
            {
                static auto SniperAimHook1 = safetyhook::create_mid(pattern.get_first(3), [](SafetyHookContext& regs) { OverrideSniperFlags(regs); });
            }

            pattern = find_pattern("8B 40 ? C1 E8 ? A8 ? 75 ? F6 85");
            if (!pattern.empty())
            {
                static auto SniperAimHook2 = safetyhook::create_mid(pattern.get_first(3), [](SafetyHookContext& regs) { OverrideSniperFlags(regs); });
            }

            pattern = find_pattern("8B 43 ? C1 E8 ? A8 ? 74 ? 80 7C 24");
            if (!pattern.empty())
            {
                static auto CrosshairHook = safetyhook::create_mid(pattern.get_first(3), [](SafetyHookContext& regs) { OverrideSniperFlags(regs); });
            }

            pattern = find_pattern("0F 85 ? ? ? ? 8B 87 ? ? ? ? 83 C0 ? 53");
            if (!pattern.empty())
            {
                static auto loc_A293B8 = resolve_displacement(pattern.get_first(0)).value();
                static auto AimZoomStateHook = safetyhook::create_mid(loc_A293B8, [](SafetyHookContext& regs)
                {
                    static auto esc = FusionFixSettings.GetRef("PREF_EXTENDEDSNIPERCONTROLS");
                    if (esc->get())
                    {
                        auto pPed = CPlayer::getLocalPlayerPed();
                        if (pPed)
                        {
                            auto m_WeaponData = CWeaponData::getWeaponData(pPed + 0x2B0, 0);
                            auto weaponType = CWeapon::getWeaponByType(m_WeaponData ? *(int*)(m_WeaponData + 0x18) : 0);

                            if ((*(uint32_t*)(weaponType + 0x20) & 8) != 0)
                            {
                                *(uint8_t*)(regs.esi + 0x200) &= 0xFE;
                            }
                        }
                    }
                });
            }
        };
    }
} Sniper;