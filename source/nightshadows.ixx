module;

#include <common.hxx>

export module nightshadows;

import common;
import comvars;
import natives;
import settings;

bool bHighResolutionNightShadows = false;
bool bIsDirLightShadows = false;

void* fnAE3310 = nullptr;
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
            Flags &= ~4; // Subtract dynamic shadows
        }

        return hbStoreStaticShadow.fun(a1, a2, Flags, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14, a15);
    }

    void __cdecl StoreStaticShadowNPC(int a1, int a2, uint32_t Flags, int a4, int a5, int a6, int a7, int a8, int a9, int a10, int a11, int a12, int a13, int a14, int a15)
    {
        // Disable the headlight shadows of NPC vehicles regardless of any condition, to avoid reaching patch 1.0.6.0 night shadow limits
        Flags &= ~4; // Subtract dynamic shadows

        return hbStoreStaticShadow.fun(a1, a2, Flags, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14, a15);
    }
}

static inline SafetyHookInline shsub_925DB0{};
static inline SafetyHookInline shsub_D77A00{};

// Lamppost shadows workaround 1
static int __cdecl sub_925DB0(int a1, int a2, int flags)
{
    if (!bExtraNightShadows)
    {
        if (!Natives::IsInteriorScene())
        {
            return -1;
        }
    }

    return shsub_925DB0.ccall<int>(a1, a2, flags);
}

// Lamppost shadows workaround 2
static void __fastcall sub_D77A00(void* _this, void* edx)
{
    if (!bExtraNightShadows)
    {
        if (!Natives::IsInteriorScene())
        {
            return;
        }
    }

    return shsub_D77A00.unsafe_fastcall(_this, edx);
}

int GetNightShadowQuality()
{
    static auto NightShadows = FusionFixSettings.GetRef("PREF_SHADOW_DENSITY");
    switch (NightShadows->get())
    {
        // MO_OFF / MO_MED / MO_HIGH / MO_VHIGH
        case 0: return 0;
        case 1: return 256  * (bHighResolutionNightShadows ? 2 : 1);
        case 2: return 512  * (bHighResolutionNightShadows ? 2 : 1);
        case 3: return 1024 * (bHighResolutionNightShadows ? 2 : 1);
        default: return 0;
    }
}

class NightShadows
{
public:
    NightShadows()
    {
        FusionFix::onInitEventAsync() += []()
        {
            CIniReader iniReader("");

            // [NIGHTSHADOWS]
            bHighResolutionNightShadows = iniReader.ReadInteger("SHADOWS", "HighResolutionNightShadows", 0) != 0;

            // Make the night shadow options adjust the night shadow resolution
            {
                auto pattern = find_pattern("8B 0D ? ? ? ? 85 C9 7E 1B", "8B 0D ? ? ? ? 33 C0 85 C9 7E 1B");
                static auto shsub_925E70 = safetyhook::create_inline(pattern.get_first(0), GetNightShadowQuality);
            }

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
                if (!pattern.count_hint(2).empty())
                {
                    static auto IsDirLightShadowsHook = safetyhook::create_mid(pattern.count_hint(2).get(1).get<void*>(0), [](SafetyHookContext& regs)
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

                pattern = hook::pattern("83 F8 03 75 14 F6 86");
                if (!pattern.empty())
                {
                    static auto loc_AE3867 = resolve_next_displacement(pattern.get_first(14)).value();
                    static auto loc_AE374F = resolve_next_displacement(pattern.get_first(3)).value();
                    struct ShadowsHook
                    {
                        void operator()(injector::reg_pack& regs)
                        {
                            if (bHeadlightShadows && bVehicleNightShadows)
                            {
                                static auto checkPassengersAndCar = [](uintptr_t car, uintptr_t checkAgainst)
                                {
                                    if (!car || !checkAgainst)
                                        return false;

                                    if (!*(uint8_t*)(car + 0xF15)) // Lights off
                                        return false;

                                    auto m_nVehicleType = *(uint32_t*)(car + 0x1304);
                                    if (m_nVehicleType == VEHICLETYPE_AUTOMOBILE)
                                    {
                                        if (*(uint8_t*)(car + 0x1190) != 0 && *(uint8_t*)(car + 0x1191) != 0) // Headlights damaged
                                            return false;
                                    }
                                    else if (m_nVehicleType == VEHICLETYPE_BIKE)
                                    {
                                        if (*(uint8_t*)(car + 0x1190) != 0 || *(uint8_t*)(car + 0x1191) != 0) // Headlight damaged
                                            return false;
                                    }

                                    auto passengers = (uintptr_t*)(car + 0xF50); // m_pDriver followed by m_pPassengers[8]

                                    for (size_t i = 0; i < 9; i++)
                                    {
                                        if (checkAgainst == passengers[i])
                                            return true;
                                    }

                                    if (checkAgainst == car)
                                        return true;

                                    return false;
                                };

                                // Disable the shadow of the player's vehicle, along with the shadows of the player/peds in that vehicle, if headlight shadows and vehicle night shadows are on (to avoid both interfering witch each other)
                                if (checkPassengersAndCar(CPlayer::findPlayerCar(), regs.esi) && !(*(char*)(regs.esp + 0x0B)))
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
                    pattern = hook::pattern("83 F8 03 75 17 F6 86");
                    static auto loc_AE3867 = resolve_next_displacement(pattern.get_first(14)).value();
                    static auto loc_AE374F = resolve_next_displacement(pattern.get_first(3)).value();
                    struct ShadowsHook
                    {
                        void operator()(injector::reg_pack& regs)
                        {
                            if (bHeadlightShadows && bVehicleNightShadows)
                            {
                                static auto checkPassengersAndCar = [](uintptr_t car, uintptr_t checkAgainst)
                                {
                                    if (!car || !checkAgainst)
                                        return false;

                                    if (!*(uint8_t*)(car + 0xF65)) // Lights off
                                        return false;

                                    auto m_nVehicleType = *(uint32_t*)(car + 0x1350);
                                    if (m_nVehicleType == VEHICLETYPE_AUTOMOBILE)
                                    {
                                        if (*(uint8_t*)(car + 0x11E0) != 0 && *(uint8_t*)(car + 0x11E1) != 0) // Headlights damaged
                                            return false;
                                    }
                                    else if (m_nVehicleType == VEHICLETYPE_BIKE)
                                    {
                                        if (*(uint8_t*)(car + 0x11E0) != 0 || *(uint8_t*)(car + 0x11E1) != 0) // Headlight damaged
                                            return false;
                                    }

                                    auto passengers = (uintptr_t*)(car + 0xFA0); // m_pDriver followed by m_pPassengers[8]

                                    for (size_t i = 0; i < 9; i++)
                                    {
                                        if (checkAgainst == passengers[i])
                                            return true;
                                    }

                                    if (checkAgainst == car)
                                        return true;

                                    return false;
                                };

                                // Disable the shadow of the player's vehicle, along with the shadows of the player/peds in that vehicle, if headlight shadows and vehicle night shadows are on (to avoid both interfering witch each other)
                                if (checkPassengersAndCar(CPlayer::findPlayerCar(), regs.esi) && !(*(char*)(regs.esp + 0x0F)))
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

            // Lamppost shadows
            {
                // Lamppost shadows workaround 1
                auto pattern = hook::pattern("80 3D ? ? ? ? ? 75 04 83 C8 FF");
                shsub_925DB0 = safetyhook::create_inline(pattern.get_first(), sub_925DB0);

                // Lamppost shadows workaround 2
                pattern = find_pattern("83 EC 3C 80 3D ? ? ? ? ? 56 8B F1", "83 EC 3C 53 33 DB");
                shsub_D77A00 = safetyhook::create_inline(pattern.get_first(0), sub_D77A00);

                // This code tests every light source for a custom flag.
                // When Extra Night Shadows is disabled, any light that holds that custom flag has its static and dynamic shadow bits removed if present.
                // This is mainly of use for us to allow disabling the shadows of light models from lamppost.img, for parity with consoles.
                // NOTE:
                // Currently, this code's effects are overridden by a workaround for an ugly bug,
                // which forces us to disable all exterior/outdoors night shadows. Because of that, this hook does not currently produce visible changes.
                pattern = hook::pattern("8B 55 20 F6 C1 06");
                if (!pattern.empty())
                {
                    static auto FlagsHook = safetyhook::create_mid(pattern.get_first(0), [](SafetyHookContext& regs)
                    {
                        if (!bExtraNightShadows)
                        {
                            if ((*(uint32_t*)(regs.edi + 0x4C) & 0x8000000) != 0) // Check if flag 134217728 exists
                            {
                                regs.ecx &= ~2; // Subtract static shadows
                                regs.ecx &= ~4; // Subtract dynamic shadows
                                *(uint32_t*)(regs.esp + 0x18) = regs.ecx;
                            }
                        }
                    });
                }
                else
                {
                    pattern = hook::pattern("E8 ? ? ? ? 0F B6 46 ? F3 0F 10 44 24");
                    static auto FlagsHook = safetyhook::create_mid(pattern.get_first(0), [](SafetyHookContext& regs)
                    {
                        if (!bExtraNightShadows)
                        {
                            if ((*(uint32_t*)(regs.esi + 0x4C) & 0x8000000) != 0) // Check if flag 134217728 exists
                            {
                                regs.ebx &= ~2; // Subtract static shadows
                                regs.ebx &= ~4; // Subtract dynamic shadows
                            }
                        }
                    });
                }
            }

            // Multiply the car/bike bottom static shadow texture intensity while headlight shadows and vehicle night shadows are active (To compensate for the player's car lacking a shadow)
            {
                auto pattern = hook::pattern("C7 44 24 ? ? ? ? ? F3 0F 11 14 24 50");
                if (!pattern.empty())
                {
                    static auto CarStaticShadowIntensityHook = safetyhook::create_mid(pattern.get_first(0), [](SafetyHookContext& regs)
                    {
                        if (bHeadlightShadows)
                        {
                            regs.xmm2.f32[0] *= 3.0f;
                        }
                    });
                }
                else
                {
                    pattern = hook::pattern("D9 1C 24 8D 4C 24 34 51 8B 4D 0C 52 51 50 6A 00 6A 03 6A 00 E8 ? ? ? ? 83 C4 40 8B E5 5D C3 CC");
                    static auto CarStaticShadowIntensityHook = safetyhook::create_mid(pattern.count(2).get(0).get<void*>(7), [](SafetyHookContext& regs)
                    {
                        if (bHeadlightShadows)
                        {
                            *(float*)regs.esp *= 3.0f;
                        }
                    });
                }
            }
        };
    }
} NightShadows;