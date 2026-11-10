module;

#include <common.hxx>

export module nightshadows;

import common;
import comvars;
import natives;
import settings;

namespace CWarpShadow
{
    static SafetyHookInline shGetSize = {};
    static uint32_t GetSize()
    {
        uint32_t Size = 0;

        if (*CSettings::ms_NightShadows > 0)
        {
            Size = 128 << *CSettings::ms_NightShadows;

            if (Size > 1024)
                Size = 1024;

            if (bHighResolutionNightShadows)
                Size *= 2; // This *may cause memory leaks but there's probably nothing that can really be done about it
        }

        return Size;
    }
}

namespace CLights
{
    injector::hook_back<void(__cdecl*)(void*, eLightType, uint32_t, rage::Vector3*, rage::Vector3*, rage::Vector3*, rage::Vector3*, float, uint32_t, uint32_t, float, float, float, int, int, int)> hbAddSceneLight;

    void __cdecl AddSceneLight_AddVehicleLight_PlayerDriving(void* LightAttr, eLightType Type, uint32_t Flags, rage::Vector3* Direction, rage::Vector3* Tangent, rage::Vector3* Position, rage::Vector3* Color, float Intensity, uint32_t TextureHash, uint32_t TxdSlot, float Radius, float InnerConeAngle, float OuterConeAngle, int InteriorID, int RoomID, int ShadowTrackingID)
    {
        // Player headlights only have shadows off when headlight shadows are off, which ideally would be most of the time, due to other bugs.
        if (!bHeadlightShadows)
        {
            Flags &= ~LIGHTFLAG_CAST_DYNAMIC_GEOM_SHADOWS;
        }

        return hbAddSceneLight.fun(LightAttr, Type, Flags, Direction, Tangent, Position, Color, Intensity, TextureHash, TxdSlot, Radius, InnerConeAngle, OuterConeAngle, InteriorID, RoomID, ShadowTrackingID);
    }

    void __cdecl AddSceneLight_AddVehicleLight_NonPlayerDriving(void* LightAttr, eLightType Type, uint32_t Flags, rage::Vector3* Direction, rage::Vector3* Tangent, rage::Vector3* Position, rage::Vector3* Color, float Intensity, uint32_t TextureHash, uint32_t TxdSlot, float Radius, float InnerConeAngle, float OuterConeAngle, int InteriorID, int RoomID, int ShadowTrackingID)
    {
        // Non-player headlights should always have shadows off, due to heavy limitations.
        Flags &= ~LIGHTFLAG_CAST_DYNAMIC_GEOM_SHADOWS;

        return hbAddSceneLight.fun(LightAttr, Type, Flags, Direction, Tangent, Position, Color, Intensity, TextureHash, TxdSlot, Radius, InnerConeAngle, OuterConeAngle, InteriorID, RoomID, ShadowTrackingID);
    }

    void __cdecl AddSceneLight_AddVehicleAmbientOccluder(void* LightAttr, eLightType Type, uint32_t Flags, rage::Vector3* Direction, rage::Vector3* Tangent, rage::Vector3* Position, rage::Vector3* Color, float Intensity, uint32_t TextureHash, uint32_t TxdSlot, float Radius, float InnerConeAngle, float OuterConeAngle, int InteriorID, int RoomID, int ShadowTrackingID)
    {
        // Multiply the intensity of vehicle bottom textures by 3x (Matches what vanilla PC shaders do) while headlight shadows are active (To compensate for cars not receiving night shadows).
        // TODO: In the future this will probably not be needed, however it might be better to make a .cfg option to be able to control the intensity manually still? Since some visual mods have to edit the textures to boost the intensity, and its more efficient to just do this in the code.
        if (bHeadlightShadows)
            Intensity *= 3.0f;

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

            // Make "Night Shadows" adjust warped shadow resolution instead of doing nothing
            // Normally warped shadow resolution is half the resolution of cascaded shadows.
            {
                auto pattern = hook::pattern("8B 0D ? ? ? ? 85 C9 7E ? 83 3D");
                if (!pattern.empty()) // 1.2.0.32, 1.2.0.43, 1.2.0.59
                {
                    CSettings::ms_NightShadows = *pattern.get_first<int32_t*>(12);
                    CWarpShadow::shGetSize = safetyhook::create_inline(pattern.get_first(0), CWarpShadow::GetSize);
                }
                else // 1.0.7.0, 1.0.8.0, 1.1.2.0, 1.1.3.0
                {
                    pattern = hook::pattern("8B 0D ? ? ? ? 33 C0 85 C9 7E ? 39 05");
                    CSettings::ms_NightShadows = *pattern.get_first<int32_t*>(14);
                    CWarpShadow::shGetSize = safetyhook::create_inline(pattern.get_first(0), CWarpShadow::GetSize);
                }
            }

            // Vehicle night shadows
            {
                // Get the render list ID to pass into the vehicle shadow draw lists
                static int32_t nRenderListID;
                auto pattern = find_pattern("51 53 8B 5C 24 ? 55 56 B0", "51 53 56 8B 74 24 ? 33 DB");
                static auto CRenderer__AddToDrawListShadowMapEntities_Hook1 = safetyhook::create_mid(pattern.get_first(0), [](SafetyHookContext& regs)
                {
                    nRenderListID = *(int32_t*)(regs.esp + 0x04);
                });

                // Allows vehicles to receive shadows from artificial light sources, this was disabled and in later patches entirely removed on PC to "make room" for headlight shadows
                pattern = find_pattern("74 ? 53 6A ? 6A ? 6A ? 55 E8 ? ? ? ? 53", "74 ? 56 53 53 53");
                static auto CRenderer__AddToDrawListShadowMapEntities_Hook2 = safetyhook::create_mid(pattern.get_first(0), [](SafetyHookContext& regs)
                {
                    // Register a new technique for vehicle shadows that was added to vehicle shaders (https://github.com/Parallellines0451/GTAIV.EFLC.FusionShaders/pull/54).
                    // Shader code for vehicle shadows used to exist before patch 1.0.6.0, but was removed after.
                    int32_t nLocalShadowTechID = rage::grmShaderFx::RegisterTechniqueGroup("wd_local");

                    if (bVehicleNightShadows && regs.eflags & 0x0040)
                    {
                        auto PushForcedTechnique = new CShaderFx_PushForcedTechnique(nLocalShadowTechID);
                        if (PushForcedTechnique)
                            PushForcedTechnique->Append();

                        CRenderer::AddToDrawListEntityListShadow(nRenderListID, 0, 0, 0, -1);
                        CRenderer::AddToDrawListEntityListShadow(nRenderListID, 1, 0, 0, -1);

                        auto PopForcedTechnique = new CShaderFx_PopForcedTechnique();
                        if (PopForcedTechnique)
                            PopForcedTechnique->Append();
                    }
                });
            }

            // Headlight shadows
            {
                // Control which headlights get shadows or not
                // The way GTAV does it is player headlights get shadows, any other headlights not.
                // This is very good handling for IV because the amount of shadows that can be drawn is very scarce (8 shadows).
                // Vanilla PC IV draws shadows for every headlight unconditionally, which means the limit is almost 100% of the time hit, which results in heavy "flickering" of shadows.
                // This is normally of course on top of every other issue with night shadows, especially their performance penalty.
                {
                    auto pattern = hook::pattern("68 04 05 00 00 6A 02 6A 00");
                    if (!pattern.count(2).empty())
                    {
                        CLights::hbAddSceneLight.fun = injector::MakeCALL(pattern.count(2).get(0).get<void*>(9), CLights::AddSceneLight_AddVehicleLight_PlayerDriving).get();
                        CLights::hbAddSceneLight.fun = injector::MakeCALL(pattern.count(2).get(1).get<void*>(9), CLights::AddSceneLight_AddVehicleLight_PlayerDriving).get();
                    }

                    pattern = hook::pattern("68 04 01 00 00 6A 02 6A 00");
                    if (!pattern.count(2).empty())
                    {
                        CLights::hbAddSceneLight.fun = injector::MakeCALL(pattern.count(2).get(0).get<void*>(9), CLights::AddSceneLight_AddVehicleLight_NonPlayerDriving).get();
                        CLights::hbAddSceneLight.fun = injector::MakeCALL(pattern.count(2).get(1).get<void*>(9), CLights::AddSceneLight_AddVehicleLight_NonPlayerDriving).get();
                    }
                }

                // TODO: Remove and replace with a shadow exclusion list implementation where the vehicle casting shadows from headlights is hidden from the shadow's draw list
                auto pattern = hook::pattern("83 F8 03 75 14 F6 86");
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
                                if (checkPassengersAndCar(CWorld::FindLocalPlayerVehicle(), regs.esi) && !(*(char*)(regs.esp + 0x0B)))
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
                                if (checkPassengersAndCar(CWorld::FindLocalPlayerVehicle(), regs.esi) && !(*(char*)(regs.esp + 0x0F)))
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
            // TODO: - Fix the lights culling bug properly, removing the two workarounds
            //       - Convert console lampposts for the original north vision:tm:, replace embedded textures with PC ones, add 0x200000 flag to all lights
            //       - Change the flags hook to check for 0x200000, since its the first unused flag, and append static and dynamic shadows instead of subtracting them (More consistent and cleaner this way)
            //       - Add lampposts to the shadow exclusion list that will be done in the future, to get rid of their self shadows in a more efficient way
            //         (If actually feasible/possible, otherwise the emissive tweak can just be redone for the console lampposts to maintain the same result, its not a big deal)
            {
                // Lamppost shadows workaround 1
                auto pattern = hook::pattern("80 3D ? ? ? ? ? 75 04 83 C8 FF");
                shsub_925DB0 = safetyhook::create_inline(pattern.get_first(0), sub_925DB0);

                // Lamppost shadows workaround 2
                pattern = find_pattern("83 EC 3C 80 3D ? ? ? ? ? 56 8B F1", "83 EC 3C 53 33 DB");
                shsub_D77A00 = safetyhook::create_inline(pattern.get_first(0), sub_D77A00);

                // This code tests every light attribute attached to a model for a custom flag.
                // When Extra Night Shadows is disabled, any light that holds said custom flag has its static and dynamic shadow bits removed if present.
                // This is mainly of use for us to allow disabling the shadows of light models from lamppost.img, for parity with consoles.
                // NOTE:
                // Currently, this code's effects are overridden by a workaround for an ugly bug (See hooks above),
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
                                regs.ecx &= ~2; // Subtract static shadows
                                regs.ecx &= ~4; // Subtract dynamic shadows
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
                                regs.ebx &= ~2; // Subtract static shadows
                                regs.ebx &= ~4; // Subtract dynamic shadows
                            }
                        }
                    });
                }
            }

            // Vehicle bottom textures
            {
                auto pattern = hook::pattern("6A ? 6A ? 6A ? E8 ? ? ? ? 83 C4 ? 8B E5");
                CLights::hbAddSceneLight.fun = injector::MakeCALL(pattern.count(2).get(0).get<void*>(6), CLights::AddSceneLight_AddVehicleAmbientOccluder).get();
                CLights::hbAddSceneLight.fun = injector::MakeCALL(pattern.count(2).get(1).get<void*>(6), CLights::AddSceneLight_AddVehicleAmbientOccluder).get();
            }
        };
    }
} NightShadows;