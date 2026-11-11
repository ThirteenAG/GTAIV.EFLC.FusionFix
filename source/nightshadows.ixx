module;

#include <common.hxx>

export module nightshadows;

import common;
import comvars;
import natives;
import settings;

bool bHighResolutionNightShadows = false;

namespace CLights
{
    injector::hook_back<void(__cdecl*)(void*, rage::eLightType, uint32_t, rage::Vector3*, rage::Vector3*, rage::Vector3*, rage::Vector3*, float, uint32_t, uint32_t, float, float, float, int32_t, int32_t, int32_t)> hbAddSceneLight;

    void __cdecl AddVehicleLightPlayerDriving(void* LightAttr, rage::eLightType Type, uint32_t Flags, rage::Vector3* Direction, rage::Vector3* Tangent, rage::Vector3* Position, rage::Vector3* Color, float Intensity, uint32_t TextureHash, uint32_t TxdSlot, float Radius, float InnerConeAngle, float OuterConeAngle, int32_t InteriorID, int32_t RoomID, int32_t ShadowTrackingID)
    {
        // Disable the headlight shadows of the player's vehicle, if headlight shadows are off
        if (!bHeadlightShadows)
        {
            Flags &= ~rage::LIGHTFLAG_CAST_DYNAMIC_GEOM_SHADOWS;
        }

        return hbAddSceneLight.fun(LightAttr, Type, Flags, Direction, Tangent, Position, Color, Intensity, TextureHash, TxdSlot, Radius, InnerConeAngle, OuterConeAngle, InteriorID, RoomID, ShadowTrackingID);
    }

    void __cdecl AddVehicleLightNPC(void* LightAttr, rage::eLightType Type, uint32_t Flags, rage::Vector3* Direction, rage::Vector3* Tangent, rage::Vector3* Position, rage::Vector3* Color, float Intensity, uint32_t TextureHash, uint32_t TxdSlot, float Radius, float InnerConeAngle, float OuterConeAngle, int32_t InteriorID, int32_t RoomID, int32_t ShadowTrackingID)
    {
        // Disable the headlight shadows of NPC vehicles regardless of any condition, to avoid reaching patch 1.0.6.0 night shadow limits
        Flags &= ~rage::LIGHTFLAG_CAST_DYNAMIC_GEOM_SHADOWS;

        return hbAddSceneLight.fun(LightAttr, Type, Flags, Direction, Tangent, Position, Color, Intensity, TextureHash, TxdSlot, Radius, InnerConeAngle, OuterConeAngle, InteriorID, RoomID, ShadowTrackingID);
    }

    void __cdecl AddVehicleAmbientOccluder(void* LightAttr, rage::eLightType Type, uint32_t Flags, rage::Vector3* Direction, rage::Vector3* Tangent, rage::Vector3* Position, rage::Vector3* Color, float Intensity, uint32_t TextureHash, uint32_t TxdSlot, float Radius, float InnerConeAngle, float OuterConeAngle, int32_t InteriorID, int32_t RoomID, int32_t ShadowTrackingID)
    {
        // Multiply the car/bike bottom static shadow texture intensity while headlight shadows are active (To compensate for the player's car lacking a shadow)
        if (bHeadlightShadows)
        {
            Intensity *= 3.0f;
        }

        return hbAddSceneLight.fun(LightAttr, Type, Flags, Direction, Tangent, Position, Color, Intensity, TextureHash, TxdSlot, Radius, InnerConeAngle, OuterConeAngle, InteriorID, RoomID, ShadowTrackingID);
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
            SettingsReader reader;

            // [NIGHTSHADOWS]
            bHighResolutionNightShadows = reader.ReadInteger("SHADOWS", "HighResolutionNightShadows", 0) != 0;

            // Make the night shadow options adjust the night shadow resolution (Half the cascaded shadow resolution otherwise)
            {
                auto pattern = find_pattern("8B 0D ? ? ? ? 85 C9 7E 1B", "8B 0D ? ? ? ? 33 C0 85 C9 7E 1B");
                static auto shsub_925E70 = safetyhook::create_inline(pattern.get_first(0), GetNightShadowQuality);
            }

            // Vehicle night shadows
            // Allows rendering dynamic shadows of vehicles from artificial light sources
            {
                static int32_t nRenderListIndex;
                auto pattern = find_pattern("51 53 8B 5C 24 ? 55 56 B0", "51 53 56 8B 74 24 ? 33 DB");
                static auto ListIndexHook = safetyhook::create_mid(pattern.get_first(0), [](SafetyHookContext& regs)
                {
                    nRenderListIndex = *(int32_t*)(regs.esp + 0x04);
                });

                // Vehicle shader technique that receives shadows from local lights
                static int32_t nLocalShadowTechId = rage::grmShaderFx::RegisterTechniqueGroup("wd_local");

                pattern = find_pattern("74 ? 53 6A ? 6A ? 6A ? 55 E8 ? ? ? ? 53", "74 ? 56 53 53 53");
                static auto VehicleShadowsHook = safetyhook::create_mid(pattern.get_first(0), [](SafetyHookContext& regs)
                {
                    if(bVehicleNightShadows && regs.eflags & 0x0040)
                    {
                        auto pushTech = new CShaderFx_PushForcedTechnique(nLocalShadowTechId);
                        if(pushTech)
                            pushTech->Append();

                        CRenderer::AddToDrawListEntityListShadow(nRenderListIndex, 0, 0, 0, -1);
                        CRenderer::AddToDrawListEntityListShadow(nRenderListIndex, 1, 0, 0, -1);

                        auto popTech = new CShaderFx_PopForcedTechnique();
                        if(popTech)
                            popTech->Append();
                    }
                });
            }

            // Headlight shadows
            {
                // Spot light, LIGHTFLAG_USE_VEHICLE_TWIN | LIGHTFLAG_VEHICLE | LIGHTFLAG_CAST_DYNAMIC_GEOM_SHADOWS
                auto pattern = hook::pattern("68 04 05 00 00 6A 02 6A 00");
                CLights::hbAddSceneLight.fun = injector::MakeCALL(pattern.count(2).get(0).get<void*>(9), CLights::AddVehicleLightPlayerDriving).get();
                CLights::hbAddSceneLight.fun = injector::MakeCALL(pattern.count(2).get(1).get<void*>(9), CLights::AddVehicleLightPlayerDriving).get();

                // Spot light, LIGHTFLAG_VEHICLE | LIGHTFLAG_CAST_DYNAMIC_GEOM_SHADOWS
                pattern = hook::pattern("68 04 01 00 00 6A 02 6A 00");
                CLights::hbAddSceneLight.fun = injector::MakeCALL(pattern.count(2).get(0).get<void*>(9), CLights::AddVehicleLightNPC).get();
                CLights::hbAddSceneLight.fun = injector::MakeCALL(pattern.count(2).get(1).get<void*>(9), CLights::AddVehicleLightNPC).get();

                pattern = hook::pattern("83 F8 03 75 14 F6 86");
                if (!pattern.empty()) // 1.2.0.32, 1.2.0.43, 1.2.0.59
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
                else // 1.0.7.0, 1.0.8.0, 1.1.2.0, 1.1.3.0
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
                shsub_925DB0 = safetyhook::create_inline(pattern.get_first(0), sub_925DB0);

                // Lamppost shadows workaround 2
                pattern = find_pattern("83 EC 3C 80 3D ? ? ? ? ? 56 8B F1", "83 EC 3C 53 33 DB");
                shsub_D77A00 = safetyhook::create_inline(pattern.get_first(0), sub_D77A00);

                // This code tests every light attribute attached to a model for a custom flag.
                // When Extra Night Shadows is disabled, any light that holds that custom flag has its static and dynamic shadow bits removed if present.
                // This is mainly of use for us to allow disabling the shadows of light models from lamppost.img, for parity with consoles.
                // NOTE:
                // Currently, this code's effects are overridden by a workaround for an ugly bug (See the two hooks above),
                // which forces us to disable all exterior/outdoors night shadows. Because of that, this hook does not currently produce visible changes.
                pattern = hook::pattern("8B 55 20 F6 C1 06");
                if (!pattern.empty()) // 1.2.0.32, 1.2.0.43, 1.2.0.59
                {
                    static auto FlagsHook = safetyhook::create_mid(pattern.get_first(0), [](SafetyHookContext& regs)
                    {
                        if (!bExtraNightShadows)
                        {
                            if ((*(uint32_t*)(regs.edi + 0x4C) & 0x8000000) != 0) // Check if flag 134217728 exists
                            {
                                regs.ecx &= ~(rage::LIGHTFLAG_CAST_STATIC_GEOM_SHADOWS | rage::LIGHTFLAG_CAST_DYNAMIC_GEOM_SHADOWS);
                                *(uint32_t*)(regs.esp + 0x18) = regs.ecx;
                            }
                        }
                    });
                }
                else // 1.0.7.0, 1.0.8.0, 1.1.2.0, 1.1.3.0
                {
                    pattern = hook::pattern("E8 ? ? ? ? 0F B6 46 ? F3 0F 10 44 24");
                    static auto FlagsHook = safetyhook::create_mid(pattern.get_first(0), [](SafetyHookContext& regs)
                    {
                        if (!bExtraNightShadows)
                        {
                            if ((*(uint32_t*)(regs.esi + 0x4C) & 0x8000000) != 0) // Check if flag 134217728 exists
                            {
                                regs.ebx &= ~(rage::LIGHTFLAG_CAST_STATIC_GEOM_SHADOWS | rage::LIGHTFLAG_CAST_DYNAMIC_GEOM_SHADOWS);
                            }
                        }
                    });
                }
            }

            // Car/bike bottom static shadow textures (Vehicles and cutscene vehicles)
            {
                auto pattern = hook::pattern("6A 00 6A 03 6A 00 E8 ? ? ? ? 83 C4 40 8B E5");
                CLights::hbAddSceneLight.fun = injector::MakeCALL(pattern.count(2).get(0).get<void*>(6), CLights::AddVehicleAmbientOccluder).get();
                CLights::hbAddSceneLight.fun = injector::MakeCALL(pattern.count(2).get(1).get<void*>(6), CLights::AddVehicleAmbientOccluder).get();
            }
        };
    }
} NightShadows;