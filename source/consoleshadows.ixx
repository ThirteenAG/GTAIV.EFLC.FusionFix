module;

#include <common.hxx>

export module consoleshadows;

import common;
import settings;
import comvars;
import natives;

void* fnAE3DE0 = nullptr;
void* fnAE3310 = nullptr;
bool bVehicleNightShadows = false;
bool bIsDirLightShadows = false;
int __cdecl sub_AE3310(int a1, int a2, int a3, int a4, int a5)
{
    if (bVehicleNightShadows && !bIsDirLightShadows)
        injector::cstd<int(int, int, int, int, int)>::call(fnAE3310, a1, 0, 0, 0, a5);

    return injector::cstd<int (int, int, int, int, int)>::call(fnAE3310, a1, a2, a3, a4, a5);
}

SafetyHookInline sh_grcSetRendersState{};
uint32_t* dword_17F58E4;
void __stdcall grcSetRenderStateHook()
{
    if (dword_17F58E4 && *dword_17F58E4)
    {
        return sh_grcSetRendersState.unsafe_stdcall();
    }
}

namespace CShadows
{
    injector::hook_back<void(__cdecl*)(int, int, int, int, int, int, int, int, int, int, int, int, int, int, int)> hbStoreStaticShadow;

    void __cdecl StoreStaticShadowPlayerDriving(int a1, int a2, int a3, int a4, int a5, int a6, int a7, int a8, int a9, int a10, int a11, int a12, int a13, int a14, int a15)
    {
        if (!bHeadlightShadows)
        {
            a3 &= ~3;
            a3 &= ~4;
        }

        return hbStoreStaticShadow.fun(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14, a15);
    }

    void __cdecl StoreStaticShadowNPC(int a1, int a2, int a3, int a4, int a5, int a6, int a7, int a8, int a9, int a10, int a11, int a12, int a13, int a14, int a15)
    {
        a3 &= ~3;
        a3 &= ~4;

        return hbStoreStaticShadow.fun(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14, a15);
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
            bVehicleNightShadows = iniReader.ReadInteger("NIGHTSHADOWS", "VehicleNightShadows", 1) != 0;

            // Render dynamic shadows casted by vehicles from point lights. Bugs: Tire shadows, vehicle damage/dents/deformation shadows are missing
            {
                auto pattern = find_pattern("E8 ? ? ? ? 6A 00 6A 14 E8 ? ? ? ? 8B F8 83 C4 44", "E8 ? ? ? ? 53 6A 14 E8 ? ? ? ? 83 C4 44");
                fnAE3310 = injector::GetBranchDestination(pattern.get_first(0)).get();
                injector::MakeCALL(pattern.get_first(0), sub_AE3310, true);

                // Limit rendering to "night shadows" only, fixes vehicle damage not being reflected on directional light shadows (aka. the sun)
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

            // Fix crash caused by the patches below.
            {
                auto pattern = find_pattern("89 35 ? ? ? ? E8 ? ? ? ? 5E C2 04 00", "89 1D ? ? ? ? 8B 07 8B 50 10 8B CF 89 1D");
                dword_17F58E4 = *pattern.get_first<uint32_t*>(2);
                pattern = find_pattern("51 53 55 8B 2D ? ? ? ? 56 0F B7 5D 1C 33 F6 85 DB 7E 3A 57 EB 09", "53 56 57 8B 3D ? ? ? ? 0F B7 5F 1C 33 F6 85 DB 7E 41 55 EB 0A");
                sh_grcSetRendersState = safetyhook::create_inline(pattern.get_first(0), grcSetRenderStateHook);
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
            
                //FusionFixSettings.SetCallback("PREF_HEADLIGHTSHADOWS", [](int32_t value)
                //{
                //    bHeadlightShadows = value;
                //});
                bHeadlightShadows = FusionFixSettings("PREF_HEADLIGHTSHADOWS");

                NativeOverride::RegisterPhoneCheat("1111111111", []
                {
                    bHeadlightShadows = !bHeadlightShadows;
                    FusionFixSettings.Set("PREF_HEADLIGHTSHADOWS", bHeadlightShadows);

                    if (bHeadlightShadows)
                        Natives::PrintHelp((char*)"CHEAT1");
                    else
                        Natives::PrintHelp((char*)"CHEAT2");
                });

                pattern = hook::pattern("E8 ? ? ? ? 85 C0 74 29 6A 00");
                if (!pattern.empty())
                {
                    static auto getLocalPlayerPed = (int (*)())injector::GetBranchDestination(pattern.get_first(0)).as_int();
                    static auto FindPlayerCar = (int (*)())injector::GetBranchDestination(pattern.get_first(11)).as_int();

                    static auto loc_AE3867 = (uintptr_t)hook::get_pattern("8B 74 24 14 FF 44 24 10");
                    static auto loc_AE374F = (uintptr_t)hook::get_pattern("C6 44 24 ? ? 83 F8 04 75 12");

                    pattern = hook::pattern("83 F8 03 75 14 F6 86");
                    struct ShadowsHook
                    {
                        void operator()(injector::reg_pack& regs)
                        {
                            if (bHeadlightShadows && bVehicleNightShadows)
                            {
                                auto car = FindPlayerCar();

                                // Disable player/car shadows
                                if (regs.esi && (regs.esi == car || (regs.esi == getLocalPlayerPed() && car && *(uint32_t*)(car + 0xFA0))) && !(*(char*)(regs.esp + 0x0B)))
                                {
                                    *(uintptr_t*)(regs.esp - 4) = loc_AE3867;
                                    return;
                                }
                            }

                            // Enable player/ped shadows while in vehicles
                            if (!bHeadlightShadows && bVehicleNightShadows)
                            {
                                *(uintptr_t*)(regs.esp - 4) = loc_AE374F;
                                return;
                            }

                            if (regs.eax == 3 && (*(uint8_t*)(regs.esi + 620) & 4) != 0 && !(*(char*)(regs.esp + 0x0B)))
                            {
                                *(uintptr_t*)(regs.esp - 4) = loc_AE3867;
                            }
                        }
                    }; injector::MakeInline<ShadowsHook>(pattern.get_first(0), pattern.get_first(25));
                }
                else
                {
                    pattern = hook::pattern("E8 ? ? ? ? 85 C0 74 2A 53 E8");

                    static auto getLocalPlayerPed = (int (*)())injector::GetBranchDestination(pattern.get_first(0)).as_int();
                    static auto FindPlayerCar = (int (*)())injector::GetBranchDestination(pattern.get_first(11)).as_int();

                    static auto loc_AE3867 = (uintptr_t)hook::get_pattern("8B 4D 0C 8B 44 24 10 0F B7 54 CB");
                    static auto loc_AE374F = (uintptr_t)hook::get_pattern("8B 4D 14 83 F8 04 C6 44 24 ? ? 75 0F 8B 86");

                    pattern = hook::pattern("83 F8 03 75 17 F6 86");
                    struct ShadowsHook
                    {
                        void operator()(injector::reg_pack& regs)
                        {
                            if (bHeadlightShadows && bVehicleNightShadows)
                            {
                                auto car = FindPlayerCar();

                                // Disable player/car shadows
                                if (regs.esi && (regs.esi == car || (regs.esi == getLocalPlayerPed() && car && *(uint32_t*)(car + 0xFA0))) && !(*(char*)(regs.esp + 0x0F)))
                                {
                                    *(uintptr_t*)(regs.esp - 4) = loc_AE3867;
                                    return;
                                }
                            }

                            // Enable player/ped shadows while in vehicles
                            if (!bHeadlightShadows && bVehicleNightShadows)
                            {
                                *(uintptr_t*)(regs.esp - 4) = loc_AE374F;
                                return;
                            }

                            if (regs.eax == 3 && (*(uint8_t*)(regs.esi + 620) & 4) != 0 && !(*(char*)(regs.esp + 0x0F)))
                            {
                                *(uintptr_t*)(regs.esp - 4) = loc_AE3867;
                            }
                        }
                    }; injector::MakeInline<ShadowsHook>(pattern.get_first(0), pattern.get_first(25));
                }
            }
        };
    }
} ConsoleShadows;