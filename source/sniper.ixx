module;

#include <common.hxx>

export module sniper;

import common;
import comvars;
import settings;
import natives;

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

bool bWantsSniperScope = false;
bool cancelInitiated = false;
injector::hook_back<char(__cdecl*)(char)> hbsub_A72820;
char __cdecl sub_A72820(char a1)
{
    static auto esc = FusionFixSettings.GetRef("PREF_EXTENDEDSNIPERCONTROLS");
    if (esc->get())
    {
        static auto cancel = false;
        constexpr auto INPUT_JUMP = 2;
        if (Natives::IsControlJustPressed(0, INPUT_JUMP))
        {
            auto pPed = CPlayer::getLocalPlayerPed();
            if (pPed)
            {
                auto m_WeaponData = CWeaponData::getWeaponData(pPed + 0x2B0, 0);
                auto weaponType = CWeapon::getWeaponByType(m_WeaponData ? *(int*)(m_WeaponData + 0x18) : 0);

                if ((*(uint32_t*)(weaponType + 0x20) & 8) != 0)
                {
                    if (CPed::ComparePedTasks(pPed, TaskID::TaskSimpleAimGun, -TaskID::TaskComplexNewUseCover))
                    {
                        if (bInSniperScope)
                        {
                            bWantsSniperScope = false;
                            cancel = true;
                            cancelInitiated = true;
                        }
                        else
                            bWantsSniperScope = true;
                    }
                }
            }
        }

        if (cancel && bInSniperScope)
            return false;
        else if (!bInSniperScope)
            cancel = false;
    }

    return hbsub_A72820.fun(a1);
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
            static auto OverrideSniperFlags = [](uintptr_t& reg)
            {
                static auto esc = FusionFixSettings.GetRef("PREF_EXTENDEDSNIPERCONTROLS");
                if (esc->get())
                {
                    if (reg & 8)
                    {
                        if (!bWantsSniperScope || cancelInitiated)
                        {
                            reg ^= 8; // Remove scope

                            if (!bWantsSniperScope)
                                cancelInitiated = false;
                        }
                    }
                }
            };

            pattern = hook::pattern("8B 40 ? C1 E8 ? A8 ? 8B 44 24 ? 74");
            if (!pattern.empty())
                static auto SniperAimHook1 = safetyhook::create_mid(pattern.get_first(3), [](SafetyHookContext& regs) { OverrideSniperFlags(regs.eax); });
            else
            {
                pattern = hook::pattern("8B 42 20 8B 6C 24 20 C1 E8 03 A8 01 74 10 85 ED 74 0C 80 7C 24");
                static auto SniperAimHook1 = safetyhook::create_mid(pattern.get_first(7), [](SafetyHookContext& regs) { OverrideSniperFlags(regs.eax); });
            }

            pattern = find_pattern("8B 40 ? C1 E8 ? A8 ? 75 ? F6 85", "8B 40 20 C1 E8 03 A8 01 75 08 84 97 ? ? ? ? 74");
            if (!pattern.empty())
                static auto SniperAimHook2 = safetyhook::create_mid(pattern.get_first(3), [](SafetyHookContext& regs) { OverrideSniperFlags(regs.eax); });

            pattern = hook::pattern("8B 43 ? C1 E8 ? A8 ? 74 ? 80 7C 24");
            if (!pattern.empty())
                static auto CrosshairHook = safetyhook::create_mid(pattern.get_first(3), [](SafetyHookContext& regs) { OverrideSniperFlags(regs.eax); });
            else
            {
                pattern = hook::pattern("8B 4B 20 C1 E9 03 F6 C1 01 74 07 80 7C 24 ? ? 74 08");
                static auto CrosshairHook = safetyhook::create_mid(pattern.get_first(3), [](SafetyHookContext& regs) { OverrideSniperFlags(regs.ecx); });
            }

            // Toggle
            pattern = hook::pattern("E8 ? ? ? ? 83 C4 ? 84 C0 75 ? 38 05 ? ? ? ? 0F 84");
            if (!pattern.empty())
                hbsub_A72820.fun = injector::MakeCALL(pattern.get_first(), sub_A72820).get();

            pattern = find_pattern("E8 ? ? ? ? 83 C4 ? 84 C0 75 ? 8A 87", "E8 ? ? ? ? 83 C4 04 84 C0 75 17 8A 8E ? ? ? ? 80 E1 0F 80 F9 02");
            if (!pattern.empty())
                hbsub_A72820.fun = injector::MakeCALL(pattern.get_first(), sub_A72820).get();

            // Recoil
            pattern = find_pattern("F3 0F 59 05 ? ? ? ? F3 0F 10 64 24 ? F3 0F 10 54 24", "D8 0D ? ? ? ? F3 0F 10 5C 24 ? F3 0F 10 64 24 ? F3 0F 10 6C 24");
            if (!pattern.empty())
            {
                static auto RecoilHook = safetyhook::create_mid(pattern.get_first(0), [](SafetyHookContext& regs)
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
                                regs.xmm1.f32[0] = 0.1f;
                            }
                        }
                    }
                });
            }
        };
    }
} Sniper;