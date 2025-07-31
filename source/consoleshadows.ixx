module;

#include <common.hxx>

export module consoleshadows;

import common;
import comvars;
import settings;

void* fnAE3310 = nullptr;

bool bVehicleNightShadows = false;
bool bIsDirLightShadows   = false;

int __cdecl sub_AE3310(int a1, int a2, int a3, int a4, int a5)
{
    if (bVehicleNightShadows && !bIsDirLightShadows)
    {
        injector::cstd<int(int, int, int, int, int)>::call(fnAE3310, a1, 0, 0, 0, a5);
    }

    return injector::cstd<int (int, int, int, int, int)>::call(fnAE3310, a1, a2, a3, a4, a5);
}

uint32_t* dword_17F58E4;
SafetyHookInline sh_grcSetRenderState{};

void __stdcall grcSetRenderStateHook()
{
    if (dword_17F58E4 && *dword_17F58E4)
    {
        return sh_grcSetRenderState.unsafe_stdcall();
    }
}

namespace CShadows
{
    injector::hook_back<void(__cdecl*)(int, int, uint32_t, int, int, int, int, int, int, int, int, int, int, int, int)> hbStoreStaticShadow;

    void __cdecl StoreStaticShadowPlayerDriving(int a1, int a2, uint32_t Flags, int a4, int a5, int a6, int a7, int a8, int a9, int a10, int a11, int a12, int a13, int a14, int a15)
    {
        // Disable the headlight shadows of the player's vehicle, if headlight shadows are off
        if (!bHeadlightShadows)
        {
            Flags &= ~4; // Dynamic shadow
        }

        return hbStoreStaticShadow.fun(a1, a2, Flags, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14, a15);
    }

    void __cdecl StoreStaticShadowNPC(int a1, int a2, uint32_t Flags, int a4, int a5, int a6, int a7, int a8, int a9, int a10, int a11, int a12, int a13, int a14, int a15)
    {
        // Disable the headlight shadows of NPC vehicles regardless of any condition, to avoid reaching patch 1.0.6.0 night shadow limits
        Flags &= ~4; // Dynamic shadow

        return hbStoreStaticShadow.fun(a1, a2, Flags, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14, a15);
    }
}

class ConsoleShadows
{
public:
    ConsoleShadows()
    {
        FusionFix::onInitEventAsync() += []()
        {
            CIniReader iniReader("");

            // [NIGHTSHADOWS]
            bVehicleNightShadows = iniReader.ReadInteger("NIGHTSHADOWS", "VehicleNightShadows", 1) != 0;

            // Vehicle night shadows
            // Allows rendering dynamic shadows of vehicles from artificial light sources
            // Bugs: Vehicle tires and damage/dents/deformation do not get included in the shadow maps
            {
                auto pattern = find_pattern("E8 ? ? ? ? 6A 00 6A 14 E8 ? ? ? ? 8B F8 83 C4 44", "E8 ? ? ? ? 53 6A 14 E8 ? ? ? ? 83 C4 44");
                fnAE3310 = injector::GetBranchDestination(pattern.get_first(0)).get();
                injector::MakeCALL(pattern.get_first(0), sub_AE3310, true);

                pattern = find_pattern("89 35 ? ? ? ? E8 ? ? ? ? 5E C2 04 00", "89 1D ? ? ? ? 8B 07 8B 50 10 8B CF 89 1D");
                dword_17F58E4 = *pattern.get_first<uint32_t*>(2);

                pattern = find_pattern("51 53 55 8B 2D ? ? ? ? 56 0F B7 5D 1C 33 F6 85 DB 7E 3A 57 EB 09", "53 56 57 8B 3D ? ? ? ? 0F B7 5F 1C 33 F6 85 DB 7E 41 55 EB 0A");
                sh_grcSetRenderState = safetyhook::create_inline(pattern.get_first(0), grcSetRenderStateHook);

                // Limit rendering to night shadows only, fixes vehicle damage not being reflected on directional light shadows (aka. those from the sun/moon/thunderbolts)
                pattern = hook::pattern("E8 ? ? ? ? 8B F0 83 C4 08 85 F6 74 17 8B 0D ? ? ? ? 8B 11 FF 52 28");
                if (!pattern.count(2).empty())
                {
                    static auto IsDirLightShadowsHook = safetyhook::create_mid(pattern.count(2).get(1).get<void*>(0), [](SafetyHookContext& regs)
                    {
                        bIsDirLightShadows = *(bool*)(regs.esp + 0x1C - 0x04);
                    });
                }
                else
                {
                    pattern = hook::pattern("57 53 6A 0C E8 ? ? ? ? 8B F8 83 C4 08 3B FB 74 17");
                    static auto IsDirLightShadowsHook = safetyhook::create_mid(pattern.get_first(0), [](SafetyHookContext& regs)
                    {
                        bIsDirLightShadows = *(bool*)(regs.esp + 0x0C - 0x01);
                    });
                }
            }

            // Headlight shadows
            {
                auto pattern = hook::pattern("68 04 05 00 00 6A 02 6A 00");
                if (!pattern.count(2).empty())
                {
                    CShadows::hbStoreStaticShadow.fun = injector::MakeCALL(pattern.count(2).get(0).get<void*>(9), CShadows::StoreStaticShadowPlayerDriving).get();
                    CShadows::hbStoreStaticShadow.fun = injector::MakeCALL(pattern.count(2).get(1).get<void*>(9), CShadows::StoreStaticShadowPlayerDriving).get();
                }

                pattern = hook::pattern("68 04 01 00 00 6A 02 6A 00");
                if (!pattern.count(2).empty())
                {
                    CShadows::hbStoreStaticShadow.fun = injector::MakeCALL(pattern.count(2).get(0).get<void*>(9), CShadows::StoreStaticShadowNPC).get();
                    CShadows::hbStoreStaticShadow.fun = injector::MakeCALL(pattern.count(2).get(1).get<void*>(9), CShadows::StoreStaticShadowNPC).get();
                }

                pattern = hook::pattern("E8 ? ? ? ? 85 C0 74 29 6A 00");
                if (!pattern.empty())
                {
                    static auto getLocalPlayerPed = (int (*)())injector::GetBranchDestination(pattern.get_first(0)).as_int();
                    static auto FindPlayerCar = (int (*)())injector::GetBranchDestination(pattern.get_first(11)).as_int();

                    pattern = hook::pattern("83 F8 03 75 14 F6 86");
                    static auto loc_AE3867 = resolve_next_displacement(pattern.get_first(14)).value();
                    static auto loc_AE374F = resolve_next_displacement(pattern.get_first(3)).value();
                    struct ShadowsHook
                    {
                        void operator()(injector::reg_pack& regs)
                        {
                            if (bHeadlightShadows && bVehicleNightShadows)
                            {
                                auto car = FindPlayerCar();

                                // Disable the shadow of the player's vehicle, along with the shadows of the player/peds in that vehicle, if headlight shadows and vehicle night shadows are on (to avoid both interfering witch each other)
                                if (regs.esi && (regs.esi == car || (regs.esi == getLocalPlayerPed() && car && *(uint32_t*)(car + 0xFA0))) && !(*(char*)(regs.esp + 0x0B)))
                                {
                                    return_to(loc_AE3867);
                                }
                            }

                            // Enable shadows of the player/peds while in vehicles, if vehicle night shadows are on and if headlight shadows are off
                            if (!bHeadlightShadows && bVehicleNightShadows)
                            {
                                return_to(loc_AE374F);
                            }

                            if (!bVehicleNightShadows)
                            {
                                if (regs.eax == 3 && (*(uint8_t*)(regs.esi + 620) & 4) != 0 && !(*(char*)(regs.esp + 0x0B)))
                                {
                                    force_return_address(loc_AE3867);
                                }
                            }
                        }
                    }; injector::MakeInline<ShadowsHook>(pattern.get_first(0), pattern.get_first(25));
                }
                else
                {
                    pattern = hook::pattern("E8 ? ? ? ? 85 C0 74 2A 53 E8");

                    static auto getLocalPlayerPed = (int (*)())injector::GetBranchDestination(pattern.get_first(0)).as_int();
                    static auto FindPlayerCar = (int (*)())injector::GetBranchDestination(pattern.get_first(10)).as_int();

                    pattern = hook::pattern("83 F8 03 75 17 F6 86");
                    static auto loc_AE3867 = resolve_next_displacement(pattern.get_first(14)).value();
                    static auto loc_AE374F = resolve_next_displacement(pattern.get_first(3)).value();
                    struct ShadowsHook
                    {
                        void operator()(injector::reg_pack& regs)
                        {
                            if (bHeadlightShadows && bVehicleNightShadows)
                            {
                                auto car = FindPlayerCar();

                                // Disable the shadow of the player's vehicle, along with the shadows of the player/peds in that vehicle, if headlight shadows and vehicle night shadows are on (to avoid both interfering witch each other)
                                if (regs.esi && (regs.esi == car || (regs.esi == getLocalPlayerPed() && car && *(uint32_t*)(car + 0xFA0))) && !(*(char*)(regs.esp + 0x0F)))
                                {
                                    return_to(loc_AE3867);
                                }
                            }

                            // Enable shadows of the player/peds while in vehicles, if vehicle night shadows are on and if headlight shadows are off
                            if (!bHeadlightShadows && bVehicleNightShadows)
                            {
                                return_to(loc_AE374F);
                            }

                            if (!bVehicleNightShadows)
                            {
                                if (regs.eax == 3 && (*(uint8_t*)(regs.esi + 620) & 4) != 0 && !(*(char*)(regs.esp + 0x0F)))
                                {
                                    force_return_address(loc_AE3867);
                                }
                            }
                        }
                    }; injector::MakeInline<ShadowsHook>(pattern.get_first(0), pattern.get_first(25));
                }
            }
        };
    }
} ConsoleShadows;