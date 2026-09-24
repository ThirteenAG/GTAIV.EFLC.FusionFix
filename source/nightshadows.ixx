module;

#include <common.hxx>
#include "StableHeadlightSelector.hpp"
#include "ShadowAdapterCE.hpp"
#include "ShadowAllocationCE.hpp"
#include "ShadowAllocationPass.hpp"
#include "ShadowFloatingPointState.hpp"
#include "ShadowLightGeometry.hpp"
#include "HeadlightCasterPolicy.hpp"
#include "ShadowCasterCE.hpp"
#include "ShadowGuardDiagnostics.hpp"
#include <fstream>
#include <atomic>
#include <intrin.h>

export module nightshadows;

import common;
import comvars;
import natives;
import settings;

bool bHighResolutionNightShadows = false;

namespace CShadows
{
    // CE's submission adapter takes 16 stack words. Its final word is an opaque
    // stable key (including pointer+1 values), not a dereferenceable light object.
    // Official FusionFix preserved it through a tail jump. Our nontrivial
    // wrappers must explicitly forward it; no upstream flicker cause is claimed.
    using SubmitHeadlight = void(__cdecl*)(int, int, uint32_t, int, int, int, int, int,
                                          int, int, int, int, int, int, int, int);
    injector::hook_back<SubmitHeadlight> hbStoreStaticShadow;
    static uint32_t* pFrameCounter = nullptr;
    static std::atomic<uint32_t> drivingBeamAccepted{0}, drivingBeamRejected{0};

    struct StableHeadlightShadow
    {
        std::mutex stateMutex;
        fusionfix::shadows::StableHeadlightSelector selector;
        fusionfix::shadows::Vec3 playerPosition{};
        uintptr_t occupiedVehicle = 0;
        uint32_t frame = 0;
        bool hasFrame = false;
        bool playerValid = false;

        bool PrepareFrame()
        {
            if (!bHeadlightShadows || !pFrameCounter || !CTimer::m_snTimeInMilliseconds ||
                !CPlayer::getLocalPlayerPed || !CPlayer::findPlayerCar)
            {
                selector.Reset();
                hasFrame = false;
                return false;
            }

            // Use the game's verified frame counter. A timer tick does not
            // identify a frame when time is paused, slowed or reset.
            const uint32_t nextFrame = *pFrameCounter;
            if (hasFrame && nextFrame == frame)
                return playerValid;
            frame = nextFrame;
            hasFrame = true;
            playerValid = false;

            const uintptr_t ped = CPlayer::getLocalPlayerPed();
            if (!ped)
            {
                selector.Reset();
                return false;
            }
            const auto matrix = *reinterpret_cast<const float* const*>(ped + 0x20);
            if (!matrix)
            {
                selector.Reset();
                return false;
            }
            playerPosition = {matrix[12], matrix[13], matrix[14]};
            if (!std::isfinite(playerPosition.x) || !std::isfinite(playerPosition.y) ||
                !std::isfinite(playerPosition.z))
            {
                selector.Reset();
                return false;
            }
            const uintptr_t car = CPlayer::findPlayerCar();
            occupiedVehicle = car;
            selector.BeginFrame({nextFrame, static_cast<uint32_t>(*CTimer::m_snTimeInMilliseconds),
                                 ped, car != 0, car});
            playerValid = true;
            return true;
        }

        bool ShouldCast(int directionAddress, int positionAddress, int stableKey)
        {
            // Keep policy bookkeeping coherent if render submissions are made
            // on more than one thread. The engine call occurs after unlock.
            const std::lock_guard<std::mutex> lock(stateMutex);
            if (!PrepareFrame() || !positionAddress || !stableKey)
                return false;
            // Audited ABCC50/ABCCD0: argument 4 is direction, argument 6 position.
            // Copy values now; these vectors may live on the caller's stack.
            const auto position = reinterpret_cast<const float*>(positionAddress);
            const fusionfix::shadows::Vec3 lightPosition{position[0], position[1], position[2]};
            fusionfix::shadows::Vec3 forward{};
            if (directionAddress)
            {
                const auto direction = reinterpret_cast<const float*>(directionAddress);
                forward = {direction[0], direction[1], direction[2]};
            }
            const auto geometry = fusionfix::shadows::EvaluateGeometry(
                playerPosition, lightPosition, directionAddress ? &forward : nullptr);
            const auto identity = static_cast<uintptr_t>(static_cast<uint32_t>(stableKey));
            const bool playerHeadlight = fusionfix::shadows::ce::IsVehicleBeam(identity, occupiedVehicle);
            const bool accepted = selector.Consider({identity, geometry, playerHeadlight});
            if (playerHeadlight)
            {
                if (accepted) ++drivingBeamAccepted;
                else ++drivingBeamRejected;
            }
            return accepted;
        }
    };
    static StableHeadlightShadow gStableHeadlightShadow;

    void __cdecl StoreStaticShadowPlayerDriving(int a1, int a2, uint32_t flags,
        int direction, int tangent, int position, int a7, int a8, int a9, int a10,
        int a11, int a12, int a13, int a14, int a15, int stableKey)
    {
        if (!gStableHeadlightShadow.ShouldCast(direction, position, stableKey))
            flags &= ~4u;
        hbStoreStaticShadow.fun(a1, a2, flags, direction, tangent, position,
                                a7, a8, a9, a10, a11, a12, a13, a14, a15, stableKey);
    }

    void __cdecl StoreStaticShadowNPC(int a1, int a2, uint32_t flags,
        int direction, int tangent, int position, int a7, int a8, int a9, int a10,
        int a11, int a12, int a13, int a14, int a15, int stableKey)
    {
        if (!gStableHeadlightShadow.ShouldCast(direction, position, stableKey))
            flags &= ~4u;
        hbStoreStaticShadow.fun(a1, a2, flags, direction, tangent, position,
                                a7, a8, a9, a10, a11, a12, a13, a14, a15, stableKey);
    }

    static bool ValidateAdapter()
    {
        const auto image = reinterpret_cast<const uint8_t*>(GetModuleHandleW(nullptr));
        const auto dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(image);
        if (!image || dos->e_magic != IMAGE_DOS_SIGNATURE || dos->e_lfanew < 0 ||
            dos->e_lfanew > 0x100000)
            return false;
        const auto nt = reinterpret_cast<const IMAGE_NT_HEADERS32*>(image + dos->e_lfanew);
        if (!fusionfix::shadows::ce::ValidateMappedImage(
                image, nt->OptionalHeader.SizeOfImage, reinterpret_cast<uintptr_t>(image)))
            return false;
        pFrameCounter = reinterpret_cast<uint32_t*>(
            reinterpret_cast<uintptr_t>(image) + fusionfix::shadows::ce::FrameCounterRva);
        hbStoreStaticShadow.fun = reinterpret_cast<SubmitHeadlight>(
            reinterpret_cast<uintptr_t>(image) + fusionfix::shadows::ce::SubmitRva);
        return true;
    }
}

#include "ShadowAllocationRuntime.inl"
#include "ShadowCasterRuntime.inl"
#include "NightShadowAdmissionRuntime.inl"

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

    const fusionfix::shadows::caster::Scope scope(OwnHeadlightCaster::context,
                                                OwnHeadlightCaster::Capture(_this));
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

// Menu-only status query. Keep the original warning when the guarded fix is
// unavailable, disabled, or running in observation mode.
export bool IsPlayerNightShadowFixActive() noexcept
{
    return PlayerShadowAllocation::ready.load(std::memory_order_acquire) &&
        PlayerShadowAllocation::publicationEnabled &&
        !PlayerShadowAllocation::unsupportedThread.load(std::memory_order_relaxed) &&
        OwnHeadlightCaster::enabled.load(std::memory_order_acquire) &&
        bExtraNightShadows && bHeadlightShadows && bVehicleNightShadows;
}

class NightShadows
{
public:
    NightShadows()
    {
        // Registered before game callbacks start, independent of async init.
        FusionFix::onGameProcessEvent() += []() { ShadowDiagnostics::Write(); };
        FusionFix::onInitEventAsync() += []()
        {
            // This experimental adapter has only been audited for CE 1.2.0.59.
            // Fail before any night-shadow patch if a version or hook differs.
            if (!CShadows::ValidateAdapter())
            {
                // Preserve the existing FusionFix workaround on other game
                // layouts even when this experimental CE adapter is disabled.
                NightShadowAdmission::Install();
                OutputDebugStringW(L"FusionFix experimental shadows: CE adapter validation failed; night-shadow hooks skipped.\n");
                return;
            }

            CIniReader iniReader("");

            // Validate BEFORE allocator and caster installation alter guarded bytes.
            const auto image = reinterpret_cast<const uint8_t*>(GetModuleHandleW(nullptr));
            const bool casterGuard = fusionfix::shadows::ce::casterguard::Validate(
                image, fusionfix::shadows::ce::ImageSize, reinterpret_cast<uintptr_t>(image));
            const int casterMode = iniReader.ReadInteger("SHADOWS", "ExperimentalOwnHeadlightCasterFix", 0);
            // Preserve the exact startup mismatch before our own hooks modify
            // any guarded range. This remains diagnostic only: no guard bypass.
            ShadowDiagnostics::startupGuardDetails = fusionfix::shadows::ce::diagnostics::Describe(
                image, fusionfix::shadows::ce::ImageSize, reinterpret_cast<uintptr_t>(image));
            OwnHeadlightCaster::base = reinterpret_cast<uintptr_t>(image);
            // Publication happens after all hooks are installed below.
            ShadowDiagnostics::guardPassed = casterGuard;
            ShadowDiagnostics::casterMode = casterMode;
            ShadowDiagnostics::path = iniReader.GetIniPath().parent_path() / "GTAIV-shadow-candidate18.log";

            // [NIGHTSHADOWS]
            bHighResolutionNightShadows = iniReader.ReadInteger("SHADOWS", "HighResolutionNightShadows", 0) != 0;

            // Offline-reviewed prototype; never turn this on silently for an
            // existing installation. A later controlled launch must opt in.
            const int allocationMode = iniReader.ReadInteger("SHADOWS", "ExperimentalPlayerShadowAllocation", 0);
            ShadowDiagnostics::allocationMode = allocationMode;
            // 0=off, 1=observe private output only, 2=experimental publication.
            if (allocationMode == 1 || allocationMode == 2)
            {
                if (!PlayerShadowAllocation::Install(allocationMode == 2))
                    OutputDebugStringW(L"FusionFix experimental shadows: allocation adapter unavailable; original engine selection retained.\n");
            }

            // This official workaround edits a guarded instruction. It used to
            // race these checks from fixes.ixx's separate async initializer.
            ShadowDiagnostics::admissionInstalled = NightShadowAdmission::Install();

            // Make the night shadow options adjust the night shadow resolution
            {
                auto pattern = find_pattern("8B 0D ? ? ? ? 85 C9 7E 1B", "8B 0D ? ? ? ? 33 C0 85 C9 7E 1B");
                static auto shsub_925E70 = safetyhook::create_inline(pattern.get_first(0), GetNightShadowQuality);
            }

            // Vehicle night shadows
            // Allows rendering dynamic shadows of vehicles from artificial light sources
            {
                static int32_t nRenderListIndex;
                auto pattern = hook::pattern("E8 ? ? ? ? 83 C4 08 57 56");
                static auto ListIndexHook = safetyhook::create_mid(injector::GetBranchDestination(pattern.get_first(0)).get<void>(), [](SafetyHookContext& regs)
                {
                    nRenderListIndex = *(int32_t*)(regs.esp + 0x04);
                });

                pattern = find_pattern("A1 ? ? ? ? 83 EC 40 56", "A1 ? ? ? ? 83 EC 40 85 C0");
                auto registerTechniqueGroup = (int32_t(__cdecl*)(const char*))pattern.get_first(0);
                static int32_t nLocalShadowTechId = registerTechniqueGroup("wd_local");

                pattern = find_pattern("E8 ? ? ? ? 6A 00 6A 14 E8 ? ? ? ? 8B F8 83 C4 44", "E8 ? ? ? ? 53 6A 14 E8 ? ? ? ? 83 C4 44");
                static void (__cdecl* AddToDrawListEntityListShadow)(int32_t, int32_t, int32_t, int32_t, int32_t) = injector::GetBranchDestination(pattern.get_first(0)).get();

                pattern = find_pattern("74 ? 53 6A ? 6A ? 6A ? 55 E8 ? ? ? ? 53", "74 ? 56 53 53 53");
                static auto VehicleShadowsHook = safetyhook::create_mid(pattern.get_first(0), [](SafetyHookContext& regs)
                {
                    if(bVehicleNightShadows && regs.eflags & 0x0040)
                    {
                        auto pushTech = new CShaderFx_PushForcedTechnique(nLocalShadowTechId);
                        if(pushTech)
                            pushTech->Append();

                        AddToDrawListEntityListShadow(nRenderListIndex, 0, 0, 0, -1);
                        AddToDrawListEntityListShadow(nRenderListIndex, 1, 0, 0, -1);

                        auto popTech = new CShaderFx_PopForcedTechnique();
                        if(popTech)
                            popTech->Append();
                    }
                });
            }

            // Headlight shadows
            {
                const uintptr_t image = reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
                for (size_t i = 0; i < fusionfix::shadows::ce::CallRvas.size(); ++i)
                {
                    const auto wrapper = i < 2 ? CShadows::StoreStaticShadowPlayerDriving :
                                                  CShadows::StoreStaticShadowNPC;
                    injector::MakeCALL(image + fusionfix::shadows::ce::CallRvas[i], wrapper);
                }

                auto pattern = hook::pattern("83 F8 03 75 14 F6 86");
                if (!pattern.empty())
                {
                    static auto loc_AE3867 = resolve_next_displacement(pattern.get_first(14)).value();
                    static auto loc_AE374F = resolve_next_displacement(pattern.get_first(3)).value();
                    struct ShadowsHook
                    {
                        void operator()(injector::reg_pack& regs)
                        {
                            // Exclude the occupied car/occupants only in their own
                            // immediate headlight pass; retain their lamp shadows.
                            if (OwnHeadlightCaster::Exclude(regs.esi, regs.eax,
                                    !*reinterpret_cast<const uint8_t*>(regs.esp + 0x0B)))
                                return_to(loc_AE3867);

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
                            // Preserve the occupied car in lamp shadow passes.
                            // Self-headlight occlusion still needs an in-game check.

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

            // Keep the original contact-shadow compensation only when actual vehicle night shadows are disabled.
            {
                auto pattern = hook::pattern("C7 44 24 ? ? ? ? ? F3 0F 11 14 24 50");
                if (!pattern.empty())
                {
                    static auto CarStaticShadowIntensityHook = safetyhook::create_mid(pattern.get_first(0), [](SafetyHookContext& regs)
                    {
                        if (bHeadlightShadows && !bVehicleNightShadows)
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
                        if (bHeadlightShadows && !bVehicleNightShadows)
                        {
                            *(float*)regs.esp *= 3.0f;
                        }
                    });
                }
            }
            OwnHeadlightCaster::enabled.store(casterGuard && casterMode == 1 &&
                static_cast<bool>(shsub_D77A00), std::memory_order_release);
            ShadowDiagnostics::ready.store(
                iniReader.ReadInteger("SHADOWS", "ExperimentalShadowDiagnostics", 0) != 0,
                std::memory_order_release);
        };
    }
} NightShadows;
