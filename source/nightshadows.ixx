module;

#include <common.hxx>
#include <unordered_map>
#include <chrono>
#include <cmath>

export module nightshadows;

import common;
import comvars;
import settings;
import cutscenecam;

// Night shadows: the shadows of local lights (streetlights, headlights, interior lights).
// The game renders seven of them per frame and keeps a small cache of static ones. Which lights get them is decided by
// the light's size on screen, visible lights first; a light that has a shadow keeps it unless another one is clearly
// more important, the player's headlights always get one and traffic headlights near the camera compete with the
// streetlights like any other light. A vehicle keeps its own shadow while its headlights cast theirs, and what is right around a light's
// bulb (its fixture) casts no shadow from it, so a lamppost shadows the street with its pole but not with its lamp head.

// Current-view light recovery before native shadow selection.
namespace ShadowCuts
{
    constexpr int32_t MaxLights = 640;
    bool installed = false;
    bool haveCamera = false;
    uint32_t cutFrame = 0;
    bool haveCut = false;
    float previousDirection[3] = {};
    float previousPosition[3] = {};
    uintptr_t previousLeaf = 0;
    uintptr_t previousViewport = 0;
    int32_t previousMode = -1;
    bool previousLookBehind = false;
    bool pendingLookCut = false;
    uint32_t lookEventFrame = 0;
    uintptr_t* pViewports = nullptr;
    rage::CLightSource** pPreviousLights = nullptr;
    int32_t* pPreviousLightCount = nullptr;
    void** pOccludeSystem = nullptr;
    uintptr_t* pMainPhase = nullptr;
    int32_t* pOccluderCount = nullptr;
    int32_t* pOccluderWalk = nullptr;
    int32_t* pPreviousOccluderWalk = nullptr;
    void (__cdecl* COcclusion__Update)() = nullptr;
    void (__cdecl* COcclusion__TellRageAboutNearbyOccluders)(int32_t) = nullptr;
    int32_t (__thiscall* rmOccluderSystem__CreateViewer)(void*) = nullptr;
    void (__thiscall* rmOccluderSystem__SetPvsView)(void*, int32_t, void*, void*) = nullptr;
    void (__thiscall* rmOccluderSystem__SetRenderView)(void*, int32_t, void*, void*, void*, void*) = nullptr;
    void (__thiscall* rmOccluderSystem__SetOrthogonal)(void*, int32_t, bool) = nullptr;
    bool (__thiscall* rmOccluderSystem__IsSphereOccluded)(void*, int32_t, const void*, float, bool, void*) = nullptr;
    int32_t cutViewer = -1;
    bool occlusionReady = false;
    bool refreshingOccluders = false;
    uintptr_t preparedMainPhase = 0, preparedViewport = 0;
    uint32_t preparedViewFrame = 0;
    std::array<uint8_t, sizeof(rage::grcViewport)> preparedView = {};

    struct LightRandomSample
    {
        uintptr_t source, effect;
        int values[2];
        uint32_t valid;
    };

    std::array<LightRandomSample, MaxLights> lightRandomSamples;
    int32_t randomSampleCount = 0;
    thread_local int32_t randomSample = -1;
    alignas(16) std::array<rage::CLightSource, MaxLights> lights;
    int32_t lightCount = 0;
    SafetyHookInline shAddSceneLightOrdered;
    std::vector<SafetyHookMid> hooks;
    SafetyHookInline shMainUpdateViewport;
    SafetyHookInline shProcessOne2dEffectLight;

    bool IsPreparedCut()
    {
        // Keep normal selection if the cut's current-view preparation failed.
        return installed && haveCut && occlusionReady && *CTimer::m_frameCount == cutFrame;
    }

    bool IsRecovering()
    {
        // Even if early preparation failed, the ordinary list catches up on the next update.
        // Suppress temporal admission fading without extending the original cut's deadline.
        return installed && haveCut && uint32_t(*CTimer::m_frameCount - cutFrame) <= 1;
    }

    void __fastcall UpdateMainViewport(void* phase, void*, void* viewport)
    {
        // The portal tracker is stateful. Consume the early update once, only if its accepted view still matches.
        auto reused = installed && preparedMainPhase == uintptr_t(phase) && preparedViewport == uintptr_t(viewport)
            && preparedViewFrame == *CTimer::m_frameCount
            && memcmp(preparedView.data(), (uint8_t*)viewport + 16, preparedView.size()) == 0;
        if (!reused)
            shMainUpdateViewport.thiscall<void>(phase, viewport);
        if (preparedMainPhase == uintptr_t(phase))
            preparedMainPhase = 0;
    }

    bool PrepareCutOcclusion(uintptr_t viewport)
    {
        occlusionReady = false;
        preparedMainPhase = 0;
        auto phase = *pMainPhase;
        auto system = *pOccludeSystem;
        if (!phase || !system || cutViewer < 0 || cutViewer >= ((int32_t*)system)[1])
            return false;

        // SetRenderView requires an invertible camera matrix. Never leave a previous view's occluders active.
        auto matrix = (float*)(viewport + 0x50);
        auto determinant = matrix[0] * (matrix[5] * matrix[10] - matrix[6] * matrix[9])
            - matrix[1] * (matrix[4] * matrix[10] - matrix[6] * matrix[8])
            + matrix[2] * (matrix[4] * matrix[9] - matrix[5] * matrix[8]);
        if (!std::isfinite(determinant) || std::abs(determinant) < 0.0001f)
            return false;

        shMainUpdateViewport.thiscall<void>((void*)phase, (void*)viewport);
        preparedMainPhase = phase;
        preparedViewport = viewport;
        preparedViewFrame = *CTimer::m_frameCount;
        std::copy_n((uint8_t*)viewport + 16, preparedView.size(), preparedView.begin());

        // A cut can cross the native incremental search's entire region. Complete one map-bounded pass and sort
        // its nearby candidates now; keep the native predicates and 64-occluder viewer capacity.
        *pOccluderWalk = *pPreviousOccluderWalk = -1;
        refreshingOccluders = true;
        COcclusion__Update();
        refreshingOccluders = false;

        auto previousPhase = *CRenderPhase::sm_pCurrent;
        *CRenderPhase::sm_pCurrent = phase;
        auto view = (void*)(phase + 176);
        auto camera = (uint8_t*)phase + 240;
        rmOccluderSystem__SetPvsView(system, cutViewer, view, camera);
        rmOccluderSystem__SetRenderView(system, cutViewer, view, camera + 48, camera + 32, camera);
        rmOccluderSystem__SetOrthogonal(system, cutViewer, false);
        COcclusion__TellRageAboutNearbyOccluders(cutViewer);
        *CRenderPhase::sm_pCurrent = previousPhase;
        occlusionReady = true;

        return true;
    }

    bool IsLightOccluded(const rage::CLightSource& light)
    {
        // Retain AddSceneLight's cutscene/interior exceptions. The camera inside the light is always important.
        if (!IsPreparedCut() || (light.mFlags & 0x280) || light.mRoomIndex != -1 || !(light.mFlags & 0x140))
            return false;
        auto camera = previousPosition;
        auto dx = light.mPosition.x - camera[0], dy = light.mPosition.y - camera[1], dz = light.mPosition.z - camera[2];
        if (dx * dx + dy * dy + dz * dz <= light.mRadius * light.mRadius)
            return false;
        return rmOccluderSystem__IsSphereOccluded(*pOccludeSystem, cutViewer, &light.mPosition, light.mRadius, false, nullptr);
    }

    void GatherEntityLights(uint8_t* entity)
    {
        auto model = EntityLights::ModelInfo(uintptr_t(entity));
        auto count = model ? EntityLights::CBaseModelInfo__GetNum2dEffects((void*)model) : 0;
        if (!count)
            return;

        // Replace this loaded producer's outgoing-shot records even when its lights are now off or culled.
        auto kept = 0;
        for (int32_t i = 0; i < lightCount; i++)
        {
            auto id = uintptr_t(lights[i].mShadowTrackingID);
            if (id < uintptr_t(entity) || id - uintptr_t(entity) >= count)
                lights[kept++] = lights[i];
        }
        lightCount = kept;

        float fade;
        if (EntityLights::GetLightFade(entity, true, fade))
            EntityLights::CEntity__ProcessLightsForEntity(entity, fade);
    }

    void __cdecl ProcessOne2dEffectLight(void* effect, void* matrix, float fade, float volumeFade, float coronaFade,
        uint16_t seed, uintptr_t source, int32_t interior, int32_t room, int32_t texture, bool force, bool statics, bool dynamics)
    {
        auto previousSample = randomSample;
        randomSample = -1;
        if (IsPreparedCut())
        {
            auto attributes = ((uint8_t* (__thiscall*)(void*))(*(uintptr_t**)effect)[3])(effect);
            auto flags = *(uint32_t*)(attributes + 76);
            if (EntityLights::gatheringShadows && !((statics && (flags & 0x4000)) || (dynamics && (flags & 0x8000))))
            {
                randomSample = previousSample;
                return;
            }
            if ((flags & 0x80) || attributes[88] == 15)
            {
                for (int32_t i = 0; i < randomSampleCount; i++)
                    if (lightRandomSamples[i].source == source && lightRandomSamples[i].effect == uintptr_t(effect))
                        randomSample = i;
                if (EntityLights::gatheringShadows && randomSample == -1)
                {
                    if (randomSampleCount == MaxLights)
                    {
                        randomSample = previousSample;
                        return;
                    }
                    randomSample = randomSampleCount++;
                    lightRandomSamples[randomSample] = { source, uintptr_t(effect), {}, 0 };
                }
            }
        }
        shProcessOne2dEffectLight.ccall<void>(effect, matrix, fade, volumeFade, coronaFade, seed, source,
            interior, room, texture, force, statics, dynamics);
        randomSample = previousSample;
    }

    int LightRandom(int32_t index)
    {
        auto& sample = lightRandomSamples[randomSample];
        if (!(sample.valid & (1u << index)))
        {
            sample.values[index] = game_rand();
            sample.valid |= 1u << index;
        }
        return sample.values[index];
    }

    void __cdecl AddSceneLightOrdered(const rage::CLightSource* light, float distance)
    {
        constexpr auto ShadowFlags = rage::LIGHTFLAG_CAST_STATIC_GEOM_SHADOWS | rage::LIGHTFLAG_CAST_DYNAMIC_GEOM_SHADOWS;
        if (!EntityLights::gatheringShadows)
        {
            shAddSceneLightOrdered.ccall<void>(light, distance);
            return;
        }

        if (!light->mShadowTrackingID || !(light->mFlags & ShadowFlags))
            return;

        for (int32_t i = 0; i < lightCount; i++)
        {
            if (lights[i].mShadowTrackingID == light->mShadowTrackingID)
            {
                lights[i] = *light;
                return;
            }
        }
        if (lightCount == MaxLights)
            return;
        lights[lightCount++] = *light;
    }

    bool BeginFrame()
    {
        if (!installed)
            return false;

        auto viewport = pViewports[0] ? pViewports[pViewports[0]] : 0;
        if (!viewport)
        {
            haveCamera = false;
            pendingLookCut = false;
            CutsceneCuts::Reset();
            return false;
        }
        auto camera = (const float*)(viewport + 0x50);
        float distanceSquared = 0.0f;
        double rawDot = 0.0, directionLength2 = 0.0, previousDirectionLength2 = 0.0;
        for (int32_t i = 0; i < 3; i++)
        {
            auto delta = camera[12 + i] - previousPosition[i];
            distanceSquared += delta * delta;
            auto direction = double(camera[8 + i]);
            auto oldDirection = double(previousDirection[i]);
            rawDot += direction * oldDirection;
            directionLength2 += direction * direction;
            previousDirectionLength2 += oldDirection * oldDirection;
        }
        // grcViewport stores the supplied camera basis. Compare directions independently of its scale;
        // a short or degenerate axis must not look like a turn on every frame.
        auto directionValid = std::isfinite(rawDot) && std::isfinite(directionLength2)
            && std::isfinite(previousDirectionLength2) && directionLength2 > 1.0e-12 && previousDirectionLength2 > 1.0e-12;
        auto dot = directionValid ? std::clamp(rawDot / std::sqrt(directionLength2) / std::sqrt(previousDirectionLength2), -1.0, 1.0) : 1.0;
        auto moved = haveCamera && std::isfinite(distanceSquared) && distanceSquared > 25.0f * 25.0f;
        // Sample the accepted hierarchy after CCamSys::Update, not the input button or a provisional evaluation.
        auto leaf = CutsceneCuts::FinalLeaf();
        // CCam::GetCamType is virtual slot 10 on PC, as used by CCamInterface::GetFinalEffectiveMode.
        auto mode = leaf ? ((int32_t (__thiscall*)(void*))(*(uintptr_t**)leaf)[10])((void*)leaf) : -1;
        auto lookBehind = CutsceneCuts::IsLookingBehind();
        auto state = *CCutsceneManager::ms_State;
        auto gameplay = mode == 1 || mode == 2; // CCamFollowPed / CCamFollowVehicle
        auto preparing = !gameplay && state != 0 && (state < 8 || state > 11);
        auto ownerChanged = haveCamera && leaf && (leaf != previousLeaf || mode != previousMode);
        auto lookChanged = haveCamera && lookBehind != previousLookBehind;
        // Reject negligible camera handoffs and recover on look-behind state changes. A large angle
        // alone is not a cut: preparation cameras and continuous motion must not repeatedly reset shadows.
        auto changedView = (std::isfinite(distanceSquared) && distanceSquared >= 0.83666f * 0.83666f)
            || (directionValid && dot <= 0.9903);
        // The native look state and leaf can switch before CCamFinal publishes the turned view.
        // Allow two following frames for that view, then discard the event instead of accepting unrelated motion.
        auto lookExpired = pendingLookCut && (uint32_t(*CTimer::m_frameCount - lookEventFrame) > 2
            || !gameplay || viewport != previousViewport || (ownerChanged && !lookChanged));
        if (lookExpired)
            pendingLookCut = false;
        if (gameplay && lookChanged)
        {
            pendingLookCut = true;
            lookEventFrame = *CTimer::m_frameCount;
        }
        auto lookCut = pendingLookCut && changedView;
        auto ownerCut = !preparing && ownerChanged && changedView;
        auto teleport = !preparing && leaf && moved;
        // Keep native and accepted authored cuts independent of geometric/state filtering. FusionFix may roll
        // back a provisional animated-camera evaluation; DetectCut only consumes the accepted phase.
        auto animationCut = CutsceneCuts::DetectCut();
        auto cut = !haveCamera || animationCut || lookCut || ownerCut || teleport;
        std::copy_n(camera + 8, 3, previousDirection);
        std::copy_n(camera + 12, 3, previousPosition);
        previousLeaf = leaf;
        previousViewport = viewport;
        previousMode = mode;
        previousLookBehind = lookBehind;
        haveCamera = true;
        if (!cut)
            return false;

        pendingLookCut = false;
        cutFrame = *CTimer::m_frameCount;
        haveCut = true;
        occlusionReady = false;
        randomSampleCount = 0;
        lightCount = 0;
        static auto nightShadows = FusionFixSettings.GetRef("PREF_SHADOW_DENSITY");
        if (!nightShadows->get())
            return false;

        // Retain headlights and other producers. The outgoing shot's cutscene lights must be replaced, not kept
        // alongside the current shot. Fresh world lights replace old entries by tracking ID.
        auto count = std::clamp(*pPreviousLightCount, 0, MaxLights);
        for (int32_t i = 0; i < count; i++)
        {
            auto& light = (*pPreviousLights)[i];
            if (light.mFlags & rage::LIGHTFLAG_CUTSCENE)
                continue;
            if (light.mShadowTrackingID && (light.mFlags & 6))
                lights[lightCount++] = light;
        }

        if (!PrepareCutOcclusion(viewport))
            return false;
        EntityLights::gatheringShadows = true;
        CutsceneCuts::SubmitLights();
        for (auto pool : { *EntityLights::pBuildingPool, *EntityLights::pAnimatedBuildingPool, *EntityLights::pDummyPool, CObject::GetObjectsPool() })
        {
            if (!pool)
                continue;
            for (int32_t i = 0; i < pool->GetSize(); i++)
            {
                if (auto entity = (uint8_t*)pool->GetSlot(i))
                    GatherEntityLights(entity);
            }
        }
        EntityLights::gatheringShadows = false;

        return true;
    }

    // Validate the complete CE or legacy layout before installing any recovery hooks.
    bool Install(uintptr_t* viewports)
    {
        auto lightVisibility = hook::pattern("8B 56 08 8B 3D ? ? ? ? 85 D7 75 ? 8B 46 20");
        auto lightLoop = hook::pattern("32 C0 88 44 24 17 0F BF 46 2E 33 FF 8B 04 85");
        auto lightType = hook::pattern("3C 12 0F 85 ? ? ? ? 8B 07 8B CF 8B 40 14 FF D0");
        auto lightEnd = hook::pattern("8A 44 24 17 84 C0 74 18 8B 46 0C F7 D0 85 46 08 75 0E 8B 4E 34");
        auto corona1 = hook::pattern("E8 ? ? ? ? 83 C4 3C E9 ? ? ? ? A1 ? ? ? ? 25 FF 01 00 00");
        auto corona2 = hook::pattern("E8 ? ? ? ? F3 0F 10 4C 24 50 8A 44 24 4E 83 C4 3C E9");
        auto lightOcclusion = hook::pattern("F7 C6 40 01 00 00 74 39 A1 ? ? ? ? F3 0F 10 44 24 10 8B 80 F8 08 00 00");
        auto ordered = hook::pattern("56 8B 35 ? ? ? ? 8D 46 01 3D 80 02 00 00 7D 31 FF 74 24 08");
        auto reset = hook::pattern("8B 0D ? ? ? ? 33 C0 41 83 F9 03 0F 44 C8 A1 ? ? ? ? A3 ? ? ? ? 8B C1 69 C0 00 40 01 00");

        auto occlusionInit = hook::pattern("68 60 96 00 00 E8 ? ? ? ? 83 C4 04 85 C0 74 09 8B C8 E8 ? ? ? ? EB 02 33 C0 56");
        auto mainView = hook::pattern("53 8B 5C 24 08 56 57 8B F9 8D 73 10 56 8D 8F B0 00 00 00 E8 ? ? ? ?");
        auto occlusionUpdate = hook::pattern("55 8B EC 83 E4 F0 B8 28 10 00 00 E8 ? ? ? ? A1 ? ? ? ? 33 C4 89 84 24 24 10 00 00 8B 0D ? ? ? ?");
        auto beforeRender = hook::pattern("8B 4C 24 04 55 8B 6C 24 0C 57 8B 7C 24 14 C7 07 FF FF FF FF C7 45 00 FF FF FF FF C7 01 FF FF FF FF A1 ? ? ?");
        auto nearbyOccluders = hook::pattern("55 8B EC 83 E4 F0 8B 4D 08 A1 ? ? ? ? 69 C9 80 55 00 00 81 EC 78 01 00 00 03 08 56 57");
        auto mapOccluders = hook::pattern("C7 05 ? ? ? ? 00 00 00 00 C7 05 ? ? ? ? 00 00 00 00 C7 05 ? ? ? ? FF FF FF FF C7 05 ? ? ? ? FF FF FF FF C7 05 ? ? ? ? FF FF FF FF C7 05 ? ? ? ? FF FF FF FF C3");
        auto occluderBudget = hook::pattern("83 F8 64 7D 11 F3 0F 10 05 ? ? ? ? 83 FE FF");
        auto occluderSort = hook::pattern("A1 ? ? ? ? 24 07 C1 F9 03 3C 02 0F 85 75 00 00 00 83 F9 40");
        auto effectLight = hook::pattern("55 8B EC 83 E4 F0 81 EC 88 00 00 00 56 8B 75 08 32 C0 57 88 44 24 12 8D 44 24 18 50 FF 75 30 C7 44 24 20 00 00 80 3F");
        auto lightRandom1 = hook::pattern("E8 ? ? ? ? 66 0F 6E D0 0F 5B D2 F3 0F 59 15 ? ? ? ?");
        auto lightRandom2 = hook::pattern("E8 ? ? ? ? F3 0F 10 25 ? ? ? ? F3 0F 10 54 24 14 83 E0 1F");
        static bool legacy = EntityLights::legacy;
        if (!EntityLights::ready)
            return false;
        if (legacy) // 1.0.7.0, 1.0.8.0
        {
            lightVisibility = hook::pattern("8B 4F 08 8B 15 ? ? ? ? 85 CA 75 ? 8B 47 20");
            lightLoop = hook::pattern("0F BF 4F 2E 8B 34 8D ? ? ? ? 33 DB 8B CE C6 44 24 1B 00 89 74 24 28 89 5C 24 1C");
            lightType = hook::pattern("3C 12 0F 85 ? ? ? ? 8B 16 8B 42 14 8B CE FF D0");
            lightEnd = hook::pattern("80 7C 24 1B 00 74 ? 8B 47 0C F7 D0 85 47 08 75 ? 8B 7F 34 85 FF 74 ? 6A 00 8B CF E8 ? ? ? ?");
            corona1 = hook::pattern("E8 ? ? ? ? 83 C4 3C E9 ? ? ? ? 83 CF FF 39 3D ? ? ? ?");
            corona2 = hook::pattern("E8 ? ? ? ? 83 C4 3C EB ? 0F 57 DB 80 3D ? ? ? ? 00");
            lightOcclusion = hook::pattern("F7 C3 40 01 00 00 74 ? 8B 0D ? ? ? ? D9 44 24 0C 8B 81 F8 08 00 00");
            ordered = hook::pattern("56 8B 35 ? ? ? ? 8D 46 01 3D 80 02 00 00 7D ? 8B 4C 24 08 51");
            reset = hook::pattern("A1 ? ? ? ? 83 C0 01 33 C9 83 F8 03 89 0D ? ? ? ? A3 ? ? ? ?");

            occlusionInit = hook::pattern("56 68 60 96 00 00 E8 ? ? ? ? 83 C4 04 85 C0 74 ? 8B C8 E8 ? ? ? ? EB ? 33 C0");
            mainView = hook::pattern("53 56 57 8B 7C 24 10 8B F1 8D 5F 10 53 8D 8E B0 00 00 00 E8 ? ? ? ?");
            occlusionUpdate = hook::pattern("55 8B EC 83 E4 F0 B8 34 10 00 00 E8 ? ? ? ? F3 0F 10 2D ? ? ? ?");
            beforeRender = hook::pattern("8B 44 24 04 53 8B 5C 24 0C 55 8B 6C 24 14 C7 45 00 FF FF FF FF C7 03 FF FF FF FF C7 00 FF FF FF FF");
            nearbyOccluders = hook::pattern("55 8B EC 83 E4 F0 A1 ? ? ? ? 81 EC 14 02 00 00 53 8B 5D 08 69 DB 80 55 00 00 03 18 56 57");
            mapOccluders = hook::pattern("33 C0 A3 ? ? ? ? A3 ? ? ? ? 83 C8 FF A3 ? ? ? ? A3 ? ? ? ? A3 ? ? ? ? A3 ? ? ? ? C3");
            occluderBudget = hook::pattern("83 F8 64 8B F1 89 44 24 0C 7D ? 83 F9 FF 0F 85 ? ? ? ?");
            occluderSort = hook::pattern("80 E2 07 C1 F8 03 80 FA 02 75 ? 83 F8 40 7E ? 83 F8 64 7C ?");
            effectLight = hook::pattern("55 8B EC 83 E4 F0 83 EC 54 8B 4D 30 F3 0F 10 05 ? ? ? ? 53 56 8B 75 08 57 8D 44 24 14 50 51");
            lightRandom1 = hook::pattern("E8 ? ? ? ? D9 5C 24 1C 0F 57 DB 83 C4 08 EB ?");
            lightRandom2 = hook::pattern("E8 ? ? ? ? F3 0F 10 2D ? ? ? ? 0F 57 DB 83 E0 1F F3 0F 2A C0");
        }

        for (auto pattern : { &lightVisibility, &lightLoop, &lightType, &lightEnd, &corona1, &corona2, &lightOcclusion,
            &ordered, &reset, &occlusionInit, &mainView, &occlusionUpdate, &beforeRender, &nearbyOccluders, &mapOccluders,
            &occluderBudget, &occluderSort, &effectLight, &lightRandom1, &lightRandom2 })
            if (pattern->size() != 1)
                return false;

        auto resetAddress = reset.get_first<uint8_t>();
        auto pvsCall = legacy ? 0x9D : 0xED;
        auto renderViewCall = legacy ? 0xBD : 0x10F;
        auto orthogonalCall = legacy ? 0xD5 : 0x128;
        auto sphereCall = legacy ? 0x2F : 0x31;
        auto occludedBranch = legacy ? 0x36 : 0x38;
        auto random1 = lightRandom1.get_first<uint8_t>();
        // Legacy keeps the first rand call inside CRandom::GetRandomNumberInRange. Hook that call so its
        // native x87 return and rounding remain intact; CE inlines the range arithmetic in the effect.
        if (legacy)
        {
            auto range = injector::GetBranchDestination(random1).get<uint8_t>();
            if (range[0] != 0x51 || range[1] != 0xE8
                || memcmp(range + 6, "\x89\x04\x24\xDB\x04\x24\xD8\x0D", 8) != 0)
                return false;
            random1 = range + 1;
        }
        if (resetAddress[legacy ? 0x81 : 0x7D] != 0xA3
            || memcmp(occlusionInit.get_first(0x1D), "\x6A\x0C\x8B\xC8\xA3", 5) != 0
            || *occlusionInit.get_first<uint8_t>(0x26) != 0xE8 || *occlusionInit.get_first<uint8_t>(0x36) != 0xE8
            || memcmp(occlusionInit.get_first(0x48), "\x6A\x70", 2) != 0
            || *beforeRender.get_first<uint8_t>(pvsCall) != 0xE8 || *beforeRender.get_first<uint8_t>(renderViewCall) != 0xE8
            || *beforeRender.get_first<uint8_t>(orthogonalCall) != 0xE8 || *lightOcclusion.get_first<uint8_t>(sphereCall) != 0xE8
            || memcmp(lightOcclusion.get_first(occludedBranch), "\x0F\x84", 2) != 0
            || injector::GetBranchDestination(random1) != injector::GetBranchDestination(lightRandom2.get_first())
            || injector::GetBranchDestination(random1).as_int() != uintptr_t(game_rand)
            || uintptr_t(*nearbyOccluders.get_first<uintptr_t*>(legacy ? 0x27 : 0x29)) != uintptr_t(CRenderPhase::sm_pCurrent))
            return false;

        pViewports = viewports;
        pPreviousLights = *(rage::CLightSource***)(resetAddress + (legacy ? 0x29 : 21));
        pPreviousLightCount = *(int32_t**)(resetAddress + (legacy ? 0x82 : 0x7E));
        pOccludeSystem = *occlusionInit.get_first<void**>(0x22);
        pMainPhase = *lightOcclusion.get_first<uintptr_t*>(legacy ? 10 : 9);
        pOccluderCount = *mapOccluders.get_first<int32_t*>(legacy ? 3 : 2);
        pOccluderWalk = *occlusionUpdate.get_first<int32_t*>(legacy ? 0x32 : 0x20);
        pPreviousOccluderWalk = *occlusionUpdate.get_first<int32_t*>(legacy ? 0x2B : 0x3F);
        COcclusion__Update = (decltype(COcclusion__Update))occlusionUpdate.get_first();
        COcclusion__TellRageAboutNearbyOccluders = (decltype(COcclusion__TellRageAboutNearbyOccluders))nearbyOccluders.get_first();
        rmOccluderSystem__CreateViewer = (decltype(rmOccluderSystem__CreateViewer))injector::GetBranchDestination(occlusionInit.get_first(0x36)).as_int();
        rmOccluderSystem__SetPvsView = (decltype(rmOccluderSystem__SetPvsView))injector::GetBranchDestination(beforeRender.get_first(pvsCall)).as_int();
        rmOccluderSystem__SetRenderView = (decltype(rmOccluderSystem__SetRenderView))injector::GetBranchDestination(beforeRender.get_first(renderViewCall)).as_int();
        rmOccluderSystem__SetOrthogonal = (decltype(rmOccluderSystem__SetOrthogonal))injector::GetBranchDestination(beforeRender.get_first(orthogonalCall)).as_int();
        rmOccluderSystem__IsSphereOccluded = (decltype(rmOccluderSystem__IsSphereOccluded))injector::GetBranchDestination(lightOcclusion.get_first(sphereCall)).as_int();
        if (!CutsceneCuts::Init(legacy))
            return false;

        // A separate occlusion work view, not a shadow-map slot. Keep the twelve native phase IDs unchanged.
        hooks.emplace_back(safetyhook::create_mid(occlusionInit.get_first(0x26), [](SafetyHookContext& regs)
        {
            if (installed)
                *(int32_t*)regs.esp = 13;
        }));
        hooks.emplace_back(safetyhook::create_mid(occlusionInit.get_first(0x48), [](SafetyHookContext&)
        {
            cutViewer = installed ? rmOccluderSystem__CreateViewer(*pOccludeSystem) : -1;
        }));
        static uintptr_t continueOccluderWalk = uintptr_t(occluderBudget.get_first(legacy ? 11 : 5));
        hooks.emplace_back(safetyhook::create_mid(occluderBudget.get_first(legacy ? 9 : 0), [](SafetyHookContext& regs)
        {
            if (refreshingOccluders)
            {
                if (regs.eax < uint32_t(std::clamp(*pOccluderCount, 0, 32767)))
                    regs.eip = continueOccluderWalk;
            }
        }));
        hooks.emplace_back(safetyhook::create_mid(occluderSort.get_first(legacy ? 6 : 10), [](SafetyHookContext& regs)
        {
            if (refreshingOccluders)
            {
                // Native partial sort; do not change the global frame counter.
                auto& value = legacy ? regs.edx : regs.eax;
                value = (value & ~0xFFu) | 2;
            }
        }));
        static uintptr_t afterRandom1 = uintptr_t(random1 + 5);
        static uintptr_t afterRandom2 = uintptr_t(lightRandom2.get_first(5));
        hooks.emplace_back(safetyhook::create_mid(random1, [](SafetyHookContext& regs)
        {
            if (randomSample >= 0)
            {
                regs.eax = LightRandom(0);
                regs.eip = afterRandom1;
            }
        }));
        hooks.emplace_back(safetyhook::create_mid(lightRandom2.get_first(), [](SafetyHookContext& regs)
        {
            if (randomSample >= 0)
            {
                regs.eax = LightRandom(1);
                regs.eip = afterRandom2;
            }
        }));

        // CEntity::ProcessLightsForEntity: the previous view's pre-render/occlusion mask must not reject new-view
        // candidates. CLights::AddSceneLight still tests each light against the current frustum and room.
        static uintptr_t lightLoopAddress = uintptr_t(lightLoop.get_first());
        hooks.emplace_back(safetyhook::create_mid(lightVisibility.get_first(), [](SafetyHookContext& regs)
        {
            if (EntityLights::gatheringShadows)
                regs.eip = lightLoopAddress;
        }));
        static uintptr_t nextEffect = uintptr_t(lightType.get_first(8)) + *lightType.get_first<int32_t>(4);
        hooks.emplace_back(safetyhook::create_mid(lightType.get_first(), [](SafetyHookContext& regs)
        {
            if (EntityLights::gatheringShadows)
                regs.eip = nextEffect; // Type 18 is a light shaft, not a scene light
        }));
        static uintptr_t afterDraw = uintptr_t(lightEnd.get_first(legacy ? 33 : 32));
        hooks.emplace_back(safetyhook::create_mid(lightEnd.get_first(), [](SafetyHookContext& regs)
        {
            if (EntityLights::gatheringShadows)
                regs.eip = afterDraw; // Leave CGtaDrawable::AfterDraw to the real pre-render
        }));

        // ProcessOne2dEffectLight: leave both Coronas::RegisterCorona calls to the real pre-render.
        // Both calls use caller-cleaned arguments.
        static uintptr_t afterCorona1 = uintptr_t(corona1.get_first(5));
        static uintptr_t afterCorona2 = uintptr_t(corona2.get_first(5));
        hooks.emplace_back(safetyhook::create_mid(corona1.get_first(), [](SafetyHookContext& regs)
        {
            if (EntityLights::gatheringShadows)
                regs.eip = afterCorona1;
        }));
        hooks.emplace_back(safetyhook::create_mid(corona2.get_first(), [](SafetyHookContext& regs)
        {
            if (EntityLights::gatheringShadows)
                regs.eip = afterCorona2;
        }));

        // The light's native exception gates already ran. Query the fresh viewer without touching the old phase ID.
        static uintptr_t afterOcclusion = uintptr_t(lightOcclusion.get_first(8)) + *lightOcclusion.get_first<int8_t>(7);
        static uintptr_t occludedExit = uintptr_t(lightOcclusion.get_first(occludedBranch + 6)) + *lightOcclusion.get_first<int32_t>(occludedBranch + 2);
        hooks.emplace_back(safetyhook::create_mid(lightOcclusion.get_first(8), [](SafetyHookContext& regs)
        {
            if (EntityLights::gatheringShadows)
            {
                auto occluded = occlusionReady && rmOccluderSystem__IsSphereOccluded(*pOccludeSystem, cutViewer,
                    (const void*)(legacy ? regs.esi : regs.edi), *(float*)(regs.esp + (legacy ? 0x0C : 0x10)), false, nullptr);
                regs.eip = occluded ? occludedExit : afterOcclusion;
            }
        }));
        shMainUpdateViewport = safetyhook::create_inline(mainView.get_first(), UpdateMainViewport);
        shProcessOne2dEffectLight = safetyhook::create_inline(effectLight.get_first(), ProcessOne2dEffectLight);
        shAddSceneLightOrdered = safetyhook::create_inline(ordered.get_first(), AddSceneLightOrdered);
        installed = shMainUpdateViewport && shProcessOne2dEffectLight && shAddSceneLightOrdered
            && std::all_of(hooks.begin(), hooks.end(), [](const auto& hook) { return bool(hook); });
        if (!installed)
        {
            hooks.clear();
            shMainUpdateViewport = {};
            shProcessOne2dEffectLight = {};
            shAddSceneLightOrdered = {};
            CutsceneCuts::RemoveHook();
        }
        return installed;
    }
}

namespace NightShadows
{
    constexpr int32_t SlotCount = 8; // Slot 0 renders the static cache, 1 to 7 are the per-frame shadows
    constexpr int32_t SlotStride = 0x110;
    constexpr int32_t SlotTrackingToEnabled = -0x0B;
    constexpr int32_t SlotTrackingToPosition = -0x38;

    // The shadow atlas, which the lighting reads every shadow from. The per-frame shadows fill its lower half, four by
    // two tiles one and a half times the size of the cached ones; the cached shadows fill its upper half, six by three
    // tiles, sixteen of them used (the game used eight). The game's own tile size is the cached shadows' size.
    constexpr int32_t CacheCount = 16;
    constexpr int32_t CacheColumns = 6;
    constexpr int32_t CacheStride = 0x100;
    int32_t* pCacheTileSize = nullptr;
    uintptr_t pCache = 0;
    uintptr_t pSlotTileSizes = 0;

    int32_t CacheTileSize()
    {
        return *pCacheTileSize;
    }

    int32_t SlotTileSize()
    {
        return CacheTileSize() * 3 / 2;
    }

    int32_t AtlasSize()
    {
        return CacheTileSize() * CacheColumns;
    }

    void SetUpAtlasLayout()
    {
        for (int32_t slot = 0; slot < SlotCount; slot++)
            *(int32_t*)(pSlotTileSizes + slot * SlotStride) = SlotTileSize();

        for (int32_t i = 0; i < CacheCount; i++)
        {
            auto entry = pCache + i * CacheStride;
            *(int32_t*)(entry + 0x04) = CacheTileSize();
            *(float*)(entry + 0xC0) = float(i % CacheColumns) / CacheColumns;
            *(float*)(entry + 0xC4) = float(i / CacheColumns) / CacheColumns;
            *(float*)(entry + 0xC8) = 0.0f;
            *(float*)(entry + 0xCC) = 1.0f / CacheColumns;
        }
    }

    // Tracking ID of the light each slot renders, the update copy
    uint8_t* pSlotTrackingIDs = nullptr;
    // The scene's viewports, [0] is the index of the current one
    uintptr_t* pViewports = nullptr;
    uint32_t nVehicleOccupantsOffset = 0;

    enum eStreetlightShadows { Off, Scenery, SceneryAndPeople, SceneryPeopleAndVehicles };

    int32_t StreetlightShadows()
    {
        static auto setting = FusionFixSettings.GetRef("PREF_STREETLIGHTSHADOWS");
        return setting->get();
    }

    // Set where the game keeps sixteen cached shadows; with eight, streetlights keep their per-frame shadows only, as
    // there's no room in the cache for them
    bool bStreetlightsCached = false;

    uint32_t StreetlightFlags(uint32_t flags)
    {
        constexpr uint32_t ShadowFlags = rage::LIGHTFLAG_CAST_STATIC_GEOM_SHADOWS | rage::LIGHTFLAG_CAST_DYNAMIC_GEOM_SHADOWS;
        switch (StreetlightShadows())
        {
            case Off:
                return flags & ~ShadowFlags;
            case Scenery:
                return bStreetlightsCached ? (flags & ~ShadowFlags) | rage::LIGHTFLAG_CAST_STATIC_GEOM_SHADOWS : flags;
            default:
                return bStreetlightsCached ? flags | ShadowFlags : flags;
        }
    }

    // Outdoor lights by tracking ID, noted as their priority is computed
    constexpr uint32_t OutdoorLightTableSize = 4096;
    uint32_t outdoorLights[OutdoorLightTableSize] = {};

    uint32_t& OutdoorLightEntry(uint32_t trackingID)
    {
        return outdoorLights[(trackingID ^ (trackingID >> 10)) % OutdoorLightTableSize];
    }

    void SetOutdoorLight(uint32_t trackingID, bool outdoor)
    {
        auto& entry = OutdoorLightEntry(trackingID);
        if (outdoor)
            entry = trackingID;
        else if (entry == trackingID)
            entry = 0;
    }

    bool IsOutdoorLight(uint32_t trackingID)
    {
        return trackingID && OutdoorLightEntry(trackingID) == trackingID;
    }

    uint32_t SlotTrackingID(int32_t slot)
    {
        return *(uint32_t*)(pSlotTrackingIDs + slot * SlotStride);
    }

    bool SlotEnabled(int32_t slot)
    {
        return *(pSlotTrackingIDs + slot * SlotStride + SlotTrackingToEnabled) != 0;
    }

    bool HadShadowLastFrame(uint32_t trackingID)
    {
        for (int32_t slot = 1; slot < SlotCount; slot++)
        {
            if (SlotEnabled(slot) && SlotTrackingID(slot) == trackingID)
                return true;
        }
        return false;
    }

    bool IsSphereVisible(const rage::Vector3& center, float radius)
    {
        auto viewport = pViewports[pViewports[0]];
        if (!viewport)
            return true;

        // rage::grcViewport::mFrustumClipPlanes, inside is positive
        auto planes = (const float*)(viewport + 0x10 + 0x300);
        for (int32_t i = 0; i < 6; i++, planes += 4)
        {
            if (planes[0] * center.x + planes[1] * center.y + planes[2] * center.z + planes[3] + radius < 0.0f)
                return false;
        }
        return true;
    }

    // Traffic headlights cast shadows this close to the camera
    constexpr float TrafficHeadlightRange = 25.0f;

    // The camera position, noted while the shadows are picked: the viewport list is empty when the vehicles add
    // their lights
    float cameraPosition[3] = {};

    void NoteCameraPosition()
    {
        if (auto viewport = pViewports[0] ? pViewports[pViewports[0]] : 0)
        {
            // rage::grcViewport::mCameraMatrix, its position row
            std::copy_n((const float*)(viewport + 0x10 + 0x40 + 0x30), 3, cameraPosition);
        }
    }

    bool IsNearCamera(const rage::Vector3& position, float range)
    {
        auto dx = position.x - cameraPosition[0], dy = position.y - cameraPosition[1], dz = position.z - cameraPosition[2];
        return dx * dx + dy * dy + dz * dz < range * range;
    }

    // Shadows fade with distance, and over time when a light gains or loses its shadow: a light that a more important
    // one is about to replace keeps its place until its shadow has faded out, and the one that takes the place fades
    // in, so streetlights trading shadows as the camera moves never pop. 0 is the full shadow, 1 none.
    float cacheFade[CacheCount] = {};
    float slotFade[SlotCount] = {};

    constexpr float ShadowFadeSeconds = 0.4f;

    struct ShadowFadeState
    {
        float fade = 1.0f;
        bool releasing = false;
        uint32_t frame = 0;
    };
    std::unordered_map<uint32_t, ShadowFadeState> fadeStates;
    uint32_t cacheOwners[CacheCount] = {};

    // One light hands its place over at a time: lights keep their shadow while the one that a waiting light beats
    // fades out; once that fade is complete it drops back to its own value and the waiting one takes exactly its place
    bool HasReleasedShadow(uint32_t trackingID)
    {
        auto it = fadeStates.find(trackingID);
        return it != fadeStates.end() && it->second.releasing && it->second.fade >= 1.0f;
    }

    // The lights of the frame by tracking ID, open addressing
    constexpr uint32_t LightTableSize = 1024;

    struct ShadowPriorities
    {
        uint32_t frame;
        float waiting;
        float cached[CacheCount];
        uint32_t trackingIDs[LightTableSize];
        float values[LightTableSize];
        float coneCos[LightTableSize];
        float tangents[LightTableSize][3];
        bool spot[LightTableSize];
        bool dynamic[LightTableSize];
        bool staticGeometry[LightTableSize];
        int32_t cacheIndex[LightTableSize];
    };
    ShadowPriorities priorities = {};

    // The light's place in the table: where it is, or the empty place it would take; -1 if the table is full
    int32_t FindLight(uint32_t trackingID)
    {
        auto& it = priorities;
        for (uint32_t probe = 0; probe < LightTableSize; probe++)
        {
            auto i = (trackingID * 2654435761u + probe) % LightTableSize;
            if (it.trackingIDs[i] == trackingID || !it.trackingIDs[i])
                return int32_t(i);
        }
        return -1;
    }

    float SmoothStep(float edge0, float edge1, float x)
    {
        auto t = std::clamp((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
        return t * t * (3.0f - 2.0f * t);
    }

    float DistanceFade(float value)
    {
        constexpr float FadeStart = 60.0f;
        constexpr float FadeEnd = 80.0f;
        return SmoothStep(FadeStart, FadeEnd, value);
    }

    void PublishShadowFades()
    {
        auto& it = priorities;

        // The time since the last frame, for fades that take the same time at any frame rate
        static auto last = std::chrono::steady_clock::now();
        auto now = std::chrono::steady_clock::now();
        auto step = std::clamp(std::chrono::duration<float>(now - last).count(), 0.0f, 0.1f) / ShadowFadeSeconds;
        last = now;

        // The most important light waiting for a per-frame shadow
        auto waitingDynamic = FLT_MAX;
        for (uint32_t i = 0; i < LightTableSize; i++)
        {
            if (it.trackingIDs[i] && it.dynamic[i] && it.values[i] < waitingDynamic && !HadShadowLastFrame(it.trackingIDs[i]))
                waitingDynamic = it.values[i];
        }

        // The light that gives its place up: the least important one with a per-frame shadow when a more important
        // one waits for a per-frame shadow, likewise in the cache. A handover in progress continues while the waiting
        // light is still more important.
        auto frame = *CTimer::m_frameCount;
        auto releasing = [](uint32_t trackingID)
        {
            auto state = fadeStates.find(trackingID);
            return state != fadeStates.end() && state->second.releasing;
        };
        // Pick per kind: a light already handing over, otherwise the least important one a waiting light beats
        auto pick = [&](auto holds, float waiting)
        {
            int32_t current = -1, worst = -1;
            for (uint32_t i = 0; i < LightTableSize; i++)
            {
                if (!it.trackingIDs[i] || it.values[i] <= 0.0f || !holds(i) || waiting >= it.values[i])
                    continue;
                if (releasing(it.trackingIDs[i]))
                    current = int32_t(i);
                if (worst < 0 || it.values[i] > it.values[worst])
                    worst = int32_t(i);
            }
            return current >= 0 ? current : worst;
        };
        auto releaseSlot = pick([&](uint32_t i) { return it.dynamic[i] && HadShadowLastFrame(it.trackingIDs[i]); }, waitingDynamic);
        auto releaseCache = pick([&](uint32_t i) { return it.cacheIndex[i] >= 0; }, it.waiting);

        // That light fades its shadow out, the others in; a light without a shadow starts from none
        for (uint32_t i = 0; i < LightTableSize; i++)
        {
            auto trackingID = it.trackingIDs[i];
            if (!trackingID)
                continue;

            auto& state = fadeStates[trackingID];
            state.frame = frame;
            if (ShadowCuts::IsRecovering())
            {
                state.fade = 0.0f;
                state.releasing = false;
                continue;
            }
            if (!HadShadowLastFrame(trackingID) && it.cacheIndex[i] < 0)
            {
                state.fade = 1.0f;
                state.releasing = false;
                continue;
            }
            state.releasing = int32_t(i) == releaseSlot || int32_t(i) == releaseCache;
            state.fade = std::clamp(state.fade + (state.releasing ? step : -step), 0.0f, 1.0f);
        }

        // Lights not seen for a while are forgotten
        if (fadeStates.size() > 2048)
            std::erase_if(fadeStates, [frame](const auto& entry) { return frame - entry.second.frame > 300; });

        auto fadeOf = [](uint32_t trackingID)
        {
            auto state = fadeStates.find(trackingID);
            return state != fadeStates.end() ? state->second.fade : 0.0f;
        };

        for (int32_t i = 0; i < CacheCount; i++)
            cacheFade[i] = it.cached[i] < FLT_MAX ? std::max(DistanceFade(it.cached[i]), fadeOf(cacheOwners[i])) : 0.0f;

        for (int32_t slot = 1; slot < SlotCount; slot++)
        {
            auto i = FindLight(SlotTrackingID(slot));
            if (i < 0 || !it.trackingIDs[i])
            {
                slotFade[slot] = 0.0f;
                continue;
            }
            slotFade[slot] = std::max(DistanceFade(it.values[i]), fadeOf(it.trackingIDs[i]));
        }
    }

    void NoteShadowPriority(const rage::CLightSource& light, float value)
    {
        auto& it = priorities;
        if (it.frame != *CTimer::m_frameCount)
        {
            if (it.frame)
                PublishShadowFades();
            it.frame = *CTimer::m_frameCount;
            it.waiting = FLT_MAX;
            std::fill(std::begin(it.cached), std::end(it.cached), FLT_MAX);
            std::fill(std::begin(it.trackingIDs), std::end(it.trackingIDs), 0u);
        }

        // The player's headlights keep their shadow
        if (light.mFlags & rage::LIGHTFLAG_USE_VEHICLE_TWIN)
            value = 0.0f;

        auto index = light.mShadowCacheIndex;
        if (index >= 0 && index < CacheCount && value <= it.cached[index])
        {
            it.cached[index] = value;
            cacheOwners[index] = light.mShadowTrackingID;
        }
        else if (light.mFlags & rage::LIGHTFLAG_CAST_STATIC_GEOM_SHADOWS)
            it.waiting = std::min(it.waiting, value);

        if (auto i = FindLight(light.mShadowTrackingID); i >= 0 && light.mShadowTrackingID)
        {
            it.trackingIDs[i] = light.mShadowTrackingID;
            it.values[i] = value;
            it.spot[i] = light.mType == rage::LT_SPOT;
            it.dynamic[i] = (light.mFlags & rage::LIGHTFLAG_CAST_DYNAMIC_GEOM_SHADOWS) != 0;
            it.staticGeometry[i] = (light.mFlags & rage::LIGHTFLAG_CAST_STATIC_GEOM_SHADOWS) != 0;
            it.coneCos[i] = light.mOuterConeAngle;
            it.cacheIndex[i] = index >= 0 && index < CacheCount ? index : -1;
            it.tangents[i][0] = light.mTangent.x;
            it.tangents[i][1] = light.mTangent.y;
            it.tangents[i][2] = light.mTangent.z;
        }
    }

    // Headlights use a perspective projection aimed along the beam, so the whole tile covers the cone; the pair of
    // hemispheres the other lights use spends most of it outside the beam. Other spotlights (wall and ceiling lamps
    // shining down, often right over a car or a person) keep the hemispheres, and so do wider beams.
    constexpr int32_t PerspectiveSlotType = 2;   // The game's own type for it, which it never set
    constexpr int32_t DynamicRenderType = 4; // Rendered every frame, on top of its cached shadow if it has one
    constexpr float MinPerspectiveConeCos = 0.42f; // 65 degrees from the beam's centre
    // How far the shadow of such a light reaches: the beam itself lights further (the player's high beams light 75
    // metres), but a shadow over the whole of it would be spread thin and have the whole street drawn into it every
    // frame. It fades out over the last fifth of the range.
    constexpr float PerspectiveShadowRange = 35.0f;
    uintptr_t pSlots = 0;
    int32_t* pSlotCopyIndex = nullptr;
    uintptr_t pSlotCopies = 0;
    float slotConeCos[SlotCount] = {};
    float slotTangents[SlotCount][3] = {};

    // The render phase flags that draw the map (buildings, trees) and the objects that don't move (lampposts, traffic
    // lights), set for shadows rendered without a cache like the game's cached shadows have them
    constexpr uint32_t RenderMapFlags = 0x280;
    constexpr int32_t CachedRenderType = 3; // Slot 0 rendering a cached shadow

    // Whether each cached shadow was rendered with the perspective projection. A per-frame shadow drawn on top of a
    // cached one takes the same projection, so the two always match.
    bool cachePerspective[CacheCount] = {};

    // Spotlights (streetlights, headlights) are rendered from the bulb along the beam. The game's projection for every
    // light looks at the light's sphere from a point behind it, which draws whatever is close to the bulb (a lamppost's
    // own pole) from the wrong side: its shadow came out as a hollow outline, depending on which way the lamp faces.
    // Returns whether the shadow has to draw the map itself, having no cached shadow under it.
    bool ChooseSlotProjection(uint32_t slotOffset)
    {
        auto slot = slotOffset / SlotStride;
        auto base = pSlots + slotOffset;
        auto& type = *(int32_t*)(base + 0x00);
        type = 0;
        if (slot >= SlotCount)
            return false;

        auto renderType = *(int32_t*)(base + 0xFC);
        auto cacheIndex = *(int32_t*)(base + 0xF0);
        auto cached = cacheIndex >= 0 && cacheIndex < CacheCount;

        auto& it = priorities;
        auto trackingID = *(uint32_t*)(base + 0xF8);
        auto i = FindLight(trackingID);
        auto perspective = i >= 0 && trackingID && it.trackingIDs[i] == trackingID && it.spot[i] && it.coneCos[i] >= MinPerspectiveConeCos;

        if (slot == 0)
        {
            if (renderType != CachedRenderType || !cached)
                return false;
            cachePerspective[cacheIndex] = perspective;
        }
        else if (renderType != DynamicRenderType || (cached && !cachePerspective[cacheIndex]))
            return false;

        if (!perspective)
            return slot != 0 && (cacheIndex < 0 || cacheIndex >= CacheCount) && trackingID && i >= 0 && it.trackingIDs[i] == trackingID && it.staticGeometry[i];

        slotConeCos[slot] = it.coneCos[i];
        std::copy(std::begin(it.tangents[i]), std::end(it.tangents[i]), slotTangents[slot]);
        type = PerspectiveSlotType;
        return slot != 0 && cacheIndex == -1;
    }

    // The camera of a perspective slot, and the matrix that takes a world position to the shadow map: the view along
    // the beam, scaled to the cone
    void __cdecl SetUpPerspectiveSlot(uintptr_t /*world*/, int32_t slot)
    {
        auto base = pSlots + slot * SlotStride;
        auto position = (const float*)(base + 0xC0);
        auto direction = (const float*)(base + 0xD0);
        auto radius = *(float*)(base + 0xE0);

        auto dot = [](const float* a, const float* b) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; };
        auto normalize = [&dot](float* v)
        {
            auto length = std::sqrt(dot(v, v));
            if (length > 1e-6f)
            {
                v[0] /= length;
                v[1] /= length;
                v[2] /= length;
            }
            return length > 1e-6f;
        };
        auto cross = [](const float* a, const float* b, float* out)
        {
            out[0] = a[1] * b[2] - a[2] * b[1];
            out[1] = a[2] * b[0] - a[0] * b[2];
            out[2] = a[0] * b[1] - a[1] * b[0];
        };

        float forward[3] = { direction[0], direction[1], direction[2] };
        if (!normalize(forward))
            forward[2] = -1.0f;

        float up[3] = { slotTangents[slot][0], slotTangents[slot][1], slotTangents[slot][2] };
        if (!normalize(up) || std::abs(dot(up, forward)) > 0.99f)
        {
            up[0] = 0.0f;
            up[1] = std::abs(forward[2]) > 0.99f ? 1.0f : 0.0f;
            up[2] = std::abs(forward[2]) > 0.99f ? 0.0f : 1.0f;
        }

        float right[3], back[3] = { -forward[0], -forward[1], -forward[2] };
        cross(forward, up, right);
        normalize(right);
        cross(right, forward, up);

        // A little wider than the cone, so its soft edge is covered too
        auto halfAngle = std::min(std::acos(std::clamp(slotConeCos[slot], -1.0f, 1.0f)) * 1.05f, 1.25f);
        auto tanHalf = std::tan(halfAngle);

        // The map holds the view with its right reversed, so the lookup reverses it too: without it, a shadow to one
        // side of the beam's centre lands on the other side
        auto scale = 1.0f / tanHalf;
        auto scaleRight = -scale;

        // Camera: right, up, back, position
        auto camera = (float*)(base + 0x40);
        const float* rows[4] = { right, up, back, position };
        for (int32_t row = 0; row < 4; row++)
        {
            camera[row * 4 + 0] = rows[row][0];
            camera[row * 4 + 1] = rows[row][1];
            camera[row * 4 + 2] = rows[row][2];
            camera[row * 4 + 3] = row == 3 ? 1.0f : 0.0f;
        }

        // World to shadow map, applied to row vectors
        auto view = (float*)(base + 0x80);
        for (int32_t axis = 0; axis < 3; axis++)
        {
            view[axis * 4 + 0] = right[axis] * scaleRight;
            view[axis * 4 + 1] = up[axis] * scale;
            view[axis * 4 + 2] = back[axis];
            view[axis * 4 + 3] = 0.0f;
        }
        view[12] = -dot(position, right) * scaleRight;
        view[13] = -dot(position, up) * scale;
        view[14] = -dot(position, back);
        view[15] = 1.0f;

        *(float*)(base + 0x0C) = tanHalf;
        *(float*)(base + 0x14) = std::min(radius, PerspectiveShadowRange);
    }

    // The shadow parameters of the light about to be drawn. The game sets them for every shadowed light, but the
    // technique for lights with a projected texture (the player's headlights) is bound with empty ones, so those lights
    // never shadowed anything; they're set again once that technique is bound.
    namespace ShadowParams
    {
        int32_t* pVarParam0123 = nullptr;
        int32_t* pVarParam891113 = nullptr;
        int32_t* pVarOffsetScale = nullptr;
        int32_t* pVarMatrix = nullptr;
        float param0123[4] = {};
        float param891113[4] = {};
        float offsetScale[4] = {};
        float matrix[16] = {};

        SafetyHookInline shSetVector = {};
        SafetyHookInline shSetMatrix = {};

        void __cdecl SetVector(void* group, int32_t var, const float* value)
        {
            if (var == *pVarParam0123)
                std::copy_n(value, 4, param0123);
            else if (var == *pVarParam891113)
                std::copy_n(value, 4, param891113);
            else if (var == *pVarOffsetScale)
                std::copy_n(value, 4, offsetScale);
            shSetVector.ccall(group, var, value);
        }

        void __cdecl SetMatrix(void* group, int32_t var, const float* value)
        {
            if (var == *pVarMatrix)
                std::copy_n(value, 16, matrix);
            shSetMatrix.ccall(group, var, value);
        }

        constexpr int32_t TexturedShadowTechnique = 20; // lightVolumeShadowTexPoint
        SafetyHookInline shStartShader = {};

        int32_t __cdecl StartShader(int32_t technique, int32_t pass)
        {
            auto result = shStartShader.ccall<int32_t>(technique, pass);
            if (technique != TexturedShadowTechnique)
                return result;

            // The registers of the deferred lighting shaders
            auto device = rage::grcDevice::GetD3DDevice();
            device->SetPixelShaderConstantF(66, param0123, 1);
            device->SetPixelShaderConstantF(72, matrix, 4);
            device->SetPixelShaderConstantF(76, param891113, 1);
            device->SetPixelShaderConstantF(77, offsetScale, 1);
            return result;
        }
    }

    bool IsPerspectiveShadow(int32_t shadow)
    {
        if (shadow >= SlotCount && shadow < SlotCount + CacheCount)
            return cachePerspective[shadow - SlotCount];
        if (shadow < 1 || shadow >= SlotCount)
            return false;
        return *(int32_t*)(pSlotCopies + (shadow + SlotCount * *pSlotCopyIndex) * SlotStride) == PerspectiveSlotType;
    }

    // The fade of the shadow the lighting is about to use: the per-frame shadows first, then the cached ones
    void SetShadowFade(int32_t shadow)
    {
        auto fade = 0.0f;
        if (shadow >= 0 && shadow < SlotCount)
            fade = slotFade[shadow];
        else if (shadow >= SlotCount && shadow < SlotCount + CacheCount)
            fade = cacheFade[shadow - SlotCount];

        float constant[4] = { fade, IsPerspectiveShadow(shadow) ? 1.0f : 0.0f, 0.0f, 0.0f };
        rage::grcDevice::GetD3DDevice()->SetPixelShaderConstantF(205, constant, 1);
    }

    // The game sorts the shadow lights by this value, smallest first, and keeps the cache by it as well.
    // It was the distance to the camera; now it's the distance in units of the light's radius, so a big streetlight
    // ten metres away beats a small lamp at five, doubled for lights outside the view, and lowered for lights that
    // have a shadow already, so two lights of about the same importance don't keep trading it.
    float ShadowPriorityDistance(const rage::CLightSource& light, float distance)
    {
        NoteCameraPosition();
        SetOutdoorLight(light.mShadowTrackingID, light.mInteriorIndex == -1 && !(light.mFlags & rage::LIGHTFLAG_VEHICLE));

        constexpr float ReferenceRadius = 10.0f;
        auto value = distance * ReferenceRadius / std::max(light.mRadius, 1.0f);

        if (!IsSphereVisible(light.mPosition, light.mRadius) || ShadowCuts::IsLightOccluded(light))
            value *= 2.0f;

        if (HadShadowLastFrame(light.mShadowTrackingID))
            value *= 0.75f;
        else if (light.mShadowCacheIndex != -1)
            value *= 0.85f;

        NoteShadowPriority(light, value);

        // A light keeps its shadow until it has faded out: per-frame shadows above everything else, cached ones above
        // the lights without a shadow, so neither pushes the other out
        if (!ShadowCuts::IsPreparedCut() && !HasReleasedShadow(light.mShadowTrackingID))
        {
            if (HadShadowLastFrame(light.mShadowTrackingID))
                value *= 0.001f;
            else if (light.mShadowCacheIndex != -1)
                value *= 0.1f;
        }
        return value;
    }

    SafetyHookInline shCWarpShadowUpdate;

    void IncludeUncachedStaticGeometry(uintptr_t phase)
    {
        auto slot = *(int32_t*)(phase + 0x940);
        if (!ShadowCuts::installed || slot <= 0 || slot >= SlotCount
            || *(int32_t*)(pSlotTrackingIDs + slot * SlotStride - 8) != -1)
            return;
        auto trackingID = SlotTrackingID(slot);
        auto i = FindLight(trackingID);
        if (trackingID && i >= 0 && priorities.trackingIDs[i] == trackingID && priorities.staticGeometry[i])
        {
            *(uint8_t*)(phase + 0x15) = 1;
            *(uint32_t*)(phase + 0x8D0) |= RenderMapFlags;
        }
    }

    void __cdecl CWarpShadowUpdate()
    {
        auto cut = ShadowCuts::BeginFrame();
        auto previousLights = *ShadowCuts::pPreviousLights;
        auto previousCount = *ShadowCuts::pPreviousLightCount;
        if (cut)
        {
            *ShadowCuts::pPreviousLights = ShadowCuts::lights.data();
            *ShadowCuts::pPreviousLightCount = ShadowCuts::lightCount;
        }
        shCWarpShadowUpdate.ccall<void>();
        if (cut)
        {
            *ShadowCuts::pPreviousLights = previousLights;
            *ShadowCuts::pPreviousLightCount = previousCount;
        }

        if (ShadowCuts::IsRecovering() && priorities.frame == *CTimer::m_frameCount)
        {
            // Publish the new view after allocation. The ordinary publication precedes selection and describes the
            // previous view. Keep distance weighting, including a cache entry allocated by this very update.
            PublishShadowFades();
            auto cacheCount = bStreetlightsCached ? CacheCount : 8;
            for (int32_t index = 0; index < cacheCount; index++)
            {
                auto trackingID = *(uint32_t*)(pCache + index * CacheStride + 0xE0);
                auto i = FindLight(trackingID);
                cacheFade[index] = trackingID && i >= 0 && priorities.trackingIDs[i] == trackingID
                    ? DistanceFade(priorities.values[i]) : 0.0f;
            }
        }
    }

    // Whether a light may take the place of another one in the sorted list: 0 = yes, 1 = compare the values above,
    // 2 = no. The player's headlights come first; the game also put every vehicle light before any other light,
    // so passing traffic took the shadows of the lampposts.
    // Uses only eax and ecx, like the game's own: its callers keep values in the other registers across the call.
    __declspec(naked) int __cdecl CompareShadowLights(uint32_t /*flags*/, uint32_t /*otherFlags*/)
    {
        __asm
        {
            mov     eax, [esp + 8]
            test    eax, eax
            jz      take
            mov     ecx, [esp + 4]
            test    ecx, 0x400 // LIGHTFLAG_USE_VEHICLE_TWIN
            jnz     take
            test    eax, 0x400
            jnz     keep
            mov     eax, 1
            ret
        take:
            xor     eax, eax
            ret
        keep:
            mov     eax, 2
            ret
        }
    }

    // A vehicle and the people in it are left out of the shadow of its headlights, otherwise the vehicle's front would
    // shadow the whole beam. Outdoor lights also leave out people and vehicles when the Streetlight Shadows setting asks
    // for it.
    // Each shadow sets its exclusions up when its render list is set up (main thread); the entities are left out when
    // the render list is drawn, which may happen on other threads, where the exclusions are only read.
    constexpr int32_t RenderListCount = 24;
    constexpr int32_t MaxOwnEntities = 10;

    enum eEntityType { EntityVehicle = 2, EntityPed = 3, EntityObject = 4 };

    struct ShadowExclusions
    {
        uint32_t frame;
        bool noPeople;
        bool noVehicles;
        bool overDrawn;     // a per-frame shadow on top of its cached one: only what moves
        int32_t count;
        uintptr_t entities[MaxOwnEntities];
    };
    ShadowExclusions exclusions[RenderListCount] = {};

    uintptr_t FindVehicle(uintptr_t address)
    {
        auto pool = CVehicle::GetVehiclePool();
        for (int32_t i = 0; i < pool->GetSize(); i++)
        {
            if (auto vehicle = (uintptr_t)pool->GetSlot(i); vehicle && vehicle == address)
                return vehicle;
        }
        return 0;
    }

    // Lampposts far away are dummy objects, near ones objects. What is found for a light is kept a while.

    constexpr uint32_t StreetlightFrames = 600;

    // The lamp of a light: its tracking ID is the address of the entity it belongs to plus the light's index
    constexpr uint32_t MaxLightsPerEntity = 64;

    // The entity a light belongs to, in the given pools
    uintptr_t FindOwner(uint32_t trackingID, std::initializer_list<rage::fwPool<void*>*> pools)
    {
        uintptr_t owner = 0;
        for (auto pool : pools)
        {
            for (int32_t i = 0; pool && i < pool->GetSize(); i++)
            {
                auto entity = (uintptr_t)pool->GetSlot(i);
                if (entity && entity <= trackingID && trackingID - entity < MaxLightsPerEntity && entity > owner)
                    owner = entity;
            }
        }
        return owner;
    }

    // Streetlights: the lights of the lamppost models (objects, or dummy objects far away), by model name
    constexpr uint32_t HashLowercase(std::string_view name)
    {
        uint32_t hash = 0;
        for (auto c : name)
        {
            hash += uint8_t(c >= 'A' && c <= 'Z' ? c - 'A' + 'a' : c);
            hash += hash << 10;
            hash ^= hash >> 6;
        }
        hash += hash << 3;
        hash ^= hash >> 11;
        hash += hash << 15;
        return hash;
    }

    constexpr std::array LamppostModels =
    {
        HashLowercase("bm_fount_lamp"), HashLowercase("bm_nylamp1"), HashLowercase("bm_nylamp10"), HashLowercase("bm_nylamp110b"),
        HashLowercase("bm_nylamp12"), HashLowercase("bm_nylamp1_italy"), HashLowercase("bm_nylamp1a"), HashLowercase("bm_nylamp1b"),
        HashLowercase("bm_nylamp1c"), HashLowercase("bm_nylamp2a"), HashLowercase("bm_nylamp2b"), HashLowercase("bm_nylamp3"),
        HashLowercase("bm_nylamp4"), HashLowercase("bm_nylamp5"), HashLowercase("bm_nylamp6"), HashLowercase("bm_nylamp6a"),
        HashLowercase("bm_nylamp6c"), HashLowercase("bm_nylamp7"), HashLowercase("bm_nylamp7_b"), HashLowercase("bm_nylamp7_c"),
        HashLowercase("bm_nylamp8a"), HashLowercase("bm_nylamp9"), HashLowercase("cj_nylamp1"), HashLowercase("cj_nylamp2"),
        HashLowercase("cj_nylamp2b"),
    };

    bool IsLamppost(uintptr_t entity)
    {
        if (!entity || !EntityLights::ready)
            return false;
        auto modelInfo = EntityLights::ModelInfo(entity);
        return modelInfo && std::find(LamppostModels.begin(), LamppostModels.end(), *(uint32_t*)(modelInfo + 0x3C)) != LamppostModels.end();
    }

    // Traffic light poles: the model type the game draws traffic lights for
    bool IsTrafficLight(uintptr_t entity)
    {
        if (!entity || !EntityLights::ready)
            return false;
        auto modelInfo = EntityLights::ModelInfo(entity);
        return modelInfo && (*(uint32_t*)(modelInfo + 0x40) & 0xFF000000) == 0x3000000;
    }

    // The object or dummy object a light belongs to (lampposts, traffic light poles), kept a while: the pools are large
    struct OwnerOfLight
    {
        uintptr_t owner;
        uint32_t frame;
    };
    std::unordered_map<uint32_t, OwnerOfLight> owners;

    uintptr_t FindLightOwner(uint32_t trackingID)
    {
        if (!EntityLights::ready)
            return 0;
        auto frame = *CTimer::m_frameCount;
        auto it = owners.find(trackingID);
        if (it == owners.end() || frame - it->second.frame >= StreetlightFrames)
        {
            if (owners.size() > 4096)
                owners.clear();
            it = owners.insert_or_assign(trackingID, OwnerOfLight{ FindOwner(trackingID, { CObject::GetObjectsPool(), *EntityLights::pDummyPool }), frame }).first;
        }
        return it->second.owner;
    }

    bool IsStreetlight(uint32_t trackingID)
    {
        return IsLamppost(FindLightOwner(trackingID));
    }

    // One shadow per fixture: of the lamps of one fixture (traffic light poles, double lampposts) only the first one that
    // casts a shadow keeps it, the others still light. Each lamp would shadow the fixture from its own place, drawing
    // its pole twice, and spend the cache on one fixture. Other models keep the shadows of all their lights: one model
    // of an interior holds the lamps of a whole room.
    struct FirstLampOfFixture
    {
        uint32_t index;
        uint32_t frame;
    };
    std::unordered_map<uintptr_t, FirstLampOfFixture> firstLamps;

    uint32_t OneShadowPerFixture(uint32_t trackingID, uint32_t flags)
    {
        constexpr uint32_t ShadowFlags = rage::LIGHTFLAG_CAST_STATIC_GEOM_SHADOWS | rage::LIGHTFLAG_CAST_DYNAMIC_GEOM_SHADOWS;
        if (!(flags & ShadowFlags) || !trackingID)
            return flags;

        auto owner = FindLightOwner(trackingID);
        if (!IsLamppost(owner) && !IsTrafficLight(owner))
            return flags;

        if (firstLamps.size() > 4096)
            firstLamps.clear();

        auto frame = *CTimer::m_frameCount;
        auto index = trackingID - (uint32_t)owner;
        auto& first = firstLamps[owner];
        // A lamp not seen for a few frames gives its place up
        if (first.frame == 0 || frame - first.frame > 2 || index <= first.index)
        {
            first = { index, frame };
            return flags;
        }
        return flags & ~ShadowFlags;
    }

    void SetUpExclusions(int32_t renderList, int32_t slot)
    {
        if (renderList < 0 || renderList >= RenderListCount)
            return;

        auto& it = exclusions[renderList];
        it = {};
        if (slot < 0 || slot >= SlotCount || !SlotEnabled(slot))
            return;
        it.frame = *CTimer::m_frameCount;
        it.overDrawn = slot != 0 && *(int32_t*)(pSlots + slot * SlotStride + 0xF0) != -1;

        auto add = [&it](uintptr_t entity)
        {
            if (entity && it.count < MaxOwnEntities)
                it.entities[it.count++] = entity;
        };

        // Headlight tracking IDs are the vehicle's address, plus one when a single headlight works; the vehicle of the
        // headlights is left out of their shadow whether anyone drives it or not
        auto vehicle = FindVehicle(SlotTrackingID(slot) & ~1u);
        if (vehicle)
        {
            add(vehicle);
            auto occupants = (uintptr_t*)(vehicle + nVehicleOccupantsOffset); // Driver, then 8 passengers
            for (int32_t i = 0; i < 9; i++)
                add(occupants[i]);
            return;
        }

        if (slot == 0)
        {
            // A cached shadow is kept while things move: only what doesn't move goes into it
            it.noPeople = true;
            it.noVehicles = true;
        }
        else if (IsOutdoorLight(SlotTrackingID(slot)))
        {
            auto casters = StreetlightShadows();
            it.noPeople = casters < SceneryAndPeople;
            it.noVehicles = casters < SceneryPeopleAndVehicles;
        }
    }

    // Whether the entity is left out of the shadow drawn from the render list
    bool IsExcluded(int32_t renderList, uintptr_t entity, uint32_t type)
    {
        if (renderList < 0 || renderList >= RenderListCount)
            return false;

        // Set up this frame, so the render list still belongs to the same shadow
        auto& it = exclusions[renderList];
        if (it.frame != *CTimer::m_frameCount)
            return false;

        // A per-frame shadow drawn on top of a cached one adds people and vehicles: buildings and objects are in the cached
        // shadow already (it is rendered again every few frames, so an object that moves still updates), and drawing
        // them every frame was most of the night shadows' cost
        if (it.overDrawn && type != EntityPed && type != EntityVehicle)
            return true;

        // People in a vehicle go with the vehicle
        constexpr uint32_t PedInVehicle = 4;
        if (type == EntityPed && (it.noPeople || (it.noVehicles && (*(uint8_t*)(entity + 0x26C) & PedInVehicle))))
            return true;
        if (type == EntityVehicle && it.noVehicles)
            return true;

        for (int32_t i = 0; i < it.count; i++)
        {
            if (it.entities[i] == entity)
                return true;
        }
        return false;
    }
}

namespace CLights
{
    injector::hook_back<void(__cdecl*)(void*, rage::eLightType, uint32_t, rage::Vector3*, rage::Vector3*, rage::Vector3*, rage::Vector3*, float, uint32_t, uint32_t, float, float, float, int32_t, int32_t, int32_t)> hbAddSceneLight;

    void __cdecl AddVehicleLightPlayer(void* LightAttr, rage::eLightType Type, uint32_t Flags, rage::Vector3* Direction, rage::Vector3* Tangent, rage::Vector3* Position, rage::Vector3* Color, float Intensity, uint32_t TextureHash, uint32_t TxdSlot, float Radius, float InnerConeAngle, float OuterConeAngle, int32_t InteriorID, int32_t RoomID, int32_t ShadowTrackingID)
    {
        static auto HeadlightShadows = FusionFixSettings.GetRef("PREF_HEADLIGHTSHADOWS");
        if (!HeadlightShadows->get())
            Flags &= ~rage::LIGHTFLAG_CAST_DYNAMIC_GEOM_SHADOWS;

        // The player's headlights reach further and are a little brighter, so they light what is in front of the car
        // up to where their shadows fall (the pavement, the foot of a wall) and a shadow shows along its whole length
        constexpr float PlayerHeadlightReach = 1.5f;
        constexpr float PlayerHeadlightIntensity = 1.2f;
        Radius *= PlayerHeadlightReach;
        Intensity *= PlayerHeadlightIntensity;

        return hbAddSceneLight.fun(LightAttr, Type, Flags, Direction, Tangent, Position, Color, Intensity, TextureHash, TxdSlot, Radius, InnerConeAngle, OuterConeAngle, InteriorID, RoomID, ShadowTrackingID);
    }

    void __cdecl AddVehicleLightNPC(void* LightAttr, rage::eLightType Type, uint32_t Flags, rage::Vector3* Direction, rage::Vector3* Tangent, rage::Vector3* Position, rage::Vector3* Color, float Intensity, uint32_t TextureHash, uint32_t TxdSlot, float Radius, float InnerConeAngle, float OuterConeAngle, int32_t InteriorID, int32_t RoomID, int32_t ShadowTrackingID)
    {
        // The traffic's headlights near the camera cast shadows too (people in front of a car, the player next to the
        // car they left); farther ones would only take places the streetlights need, so they light without
        static auto HeadlightShadows = FusionFixSettings.GetRef("PREF_HEADLIGHTSHADOWS");
        if (!HeadlightShadows->get() || !NightShadows::IsNearCamera(*Position, NightShadows::TrafficHeadlightRange))
            Flags &= ~rage::LIGHTFLAG_CAST_DYNAMIC_GEOM_SHADOWS;

        return hbAddSceneLight.fun(LightAttr, Type, Flags, Direction, Tangent, Position, Color, Intensity, TextureHash, TxdSlot, Radius, InnerConeAngle, OuterConeAngle, InteriorID, RoomID, ShadowTrackingID);
    }
}

int GetNightShadowQuality()
{
    static auto NightShadows = FusionFixSettings.GetRef("PREF_SHADOW_DENSITY");
    switch (NightShadows->get())
    {
        // MO_OFF / MO_MED / MO_HIGH / MO_VHIGH
        case 0: return 0;
        case 1: return 256;
        case 2: return 512;
        case 3: return 1024;
        default: return 0;
    }
}

// The video memory the night shadow maps take, which the game counts against its streaming budget
int __cdecl GetNightShadowMemory(int32_t tileSize)
{
    if (tileSize < 256)
        return 0;

    auto atlas = tileSize * NightShadows::CacheColumns;
    auto slot = tileSize * 3 / 2;
    return atlas * atlas * 4 + slot * 4 * slot * 4 + slot * slot * 8 + 24; // Atlas, hemicube, the two point maps
}

class NightShadowsInit
{
public:
    NightShadowsInit()
    {
        FusionFix::onInitEventAsync() += []()
        {
            // Make the night shadow options adjust the night shadow resolution (Half the cascaded shadow resolution otherwise)
            {
                auto pattern = find_pattern("8B 0D ? ? ? ? 85 C9 7E 1B", "8B 0D ? ? ? ? 33 C0 85 C9 7E 1B");
                static auto shsub_925E70 = safetyhook::create_inline(pattern.get_first(0), GetNightShadowQuality);
            }

            // The shadow atlas: sharper per-frame shadows and twice the cached ones
            {
                auto pattern = hook::pattern("39 1D ? ? ? ? 75 14 39 3D");
                if (!pattern.empty()) // 1.2.0.32, 1.2.0.43, 1.2.0.59
                {
                    NightShadows::pCacheTileSize = *pattern.get_first<int32_t*>(2);

                    // The textures: the atlas, the hemicube the cached shadows are rendered through, the point maps
                    // each shadow is rendered to
                    pattern = hook::pattern("8D 04 B5 00 00 00 00 50 50 C7 44 24");
                    static auto AtlasSizeHook = safetyhook::create_mid(pattern.get_first(7), [](SafetyHookContext& regs)
                    {
                        regs.eax = NightShadows::AtlasSize();
                    });

                    pattern = hook::pattern("A1 ? ? ? ? 6A 20 50 C1 E0 02 50");
                    static auto HemicubeSizeHook = safetyhook::create_mid(pattern.get_first(5), [](SafetyHookContext& regs)
                    {
                        regs.eax = NightShadows::SlotTileSize();
                    });

                    pattern = hook::pattern("8B D7 52 6A 20 56 56");
                    static auto PointMapSizeHook = safetyhook::create_mid(pattern.count(2).get(0).get<void>(0), [](SafetyHookContext& regs)
                    {
                        regs.esi = NightShadows::SlotTileSize();
                    });

                    // Tile sizes and positions, after the textures are made
                    pattern = hook::pattern("89 0D ? ? ? ? 89 0D ? ? ? ? 89 0D ? ? ? ? 89 0D ? ? ? ? 89 0D ? ? ? ? 89 0D ? ? ? ? 89 0D ? ? ? ? 89 0D ? ? ? ? B8");
                    NightShadows::pSlotTileSizes = *pattern.get_first<uintptr_t>(2);
                    pattern = hook::pattern("89 0D ? ? ? ? B8 ? ? ? ? 89 08 05 00 01 00 00 3D ? ? ? ? 7C F2 5F 5E 5B 83 C4 30 C3");
                    NightShadows::pCache = *pattern.get_first<uintptr_t>(7) - 0x04;
                    static auto LayoutHook = safetyhook::create_mid(pattern.get_first(25), [](SafetyHookContext& regs)
                    {
                        NightShadows::SetUpAtlasLayout();
                    });

                    pattern = hook::pattern("8B 44 24 04 3D 00 01 00 00 73 03 33 C0 C3 0F AF C0 C1 E0 04 83 C0 18 C3");
                    static auto MemoryHook = safetyhook::create_inline(pattern.get_first(0), GetNightShadowMemory);

                    // A cached shadow copied to a point map, where a light adds its moving objects to it
                    pattern = hook::pattern("F3 0F 59 15 ? ? ? ? F3 0F 10 86 ? ? ? ? F3 0F 10 8E");
                    static auto CopyPositionHook = safetyhook::create_mid(pattern.get_first(8), [](SafetyHookContext& regs)
                    {
                        regs.xmm2.f32[0] = float(NightShadows::AtlasSize());
                    });
                    pattern = hook::pattern("FF 35 ? ? ? ? FF 35 ? ? ? ? E8 ? ? ? ? 83 C4 34 80 7D 10 00");
                    static auto CopySizeHook = safetyhook::create_mid(pattern.get_first(12), [](SafetyHookContext& regs)
                    {
                        *(int32_t*)(regs.esp + 0x10) = NightShadows::SlotTileSize();
                        *(int32_t*)(regs.esp + 0x14) = NightShadows::SlotTileSize();
                    });

                    // A cached shadow rendered from the hemicube to its tile
                    pattern = hook::pattern("F3 0F 59 15 ? ? ? ? 8B 74 24 38 C1 E6 08 57");
                    static auto CachePositionHook = safetyhook::create_mid(pattern.get_first(8), [](SafetyHookContext& regs)
                    {
                        regs.xmm2.f32[0] = float(NightShadows::AtlasSize());
                    });
                    pattern = hook::pattern("8D 04 8D 00 00 00 00 66 0F 6E C0 0F 5B C0 6A FF");
                    static auto CacheScaleHook = safetyhook::create_mid(pattern.get_first(7), [](SafetyHookContext& regs)
                    {
                        regs.eax = NightShadows::AtlasSize();
                    });

                    // A shadow that's also cached copies its slot to the cache entry; the entry keeps its tile size
                    pattern = hook::pattern("8B 8E ? ? ? ? 83 F9 FF 74 0F C1 E1 08 55 81 C1 ? ? ? ? E8 ? ? ? ? 80 BE");
                    static auto pSlotCacheIndices = *pattern.get_first<uintptr_t>(2);
                    static auto CacheEntryCopyHook = safetyhook::create_mid(pattern.get_first(26), [](SafetyHookContext& regs)
                    {
                        auto index = *(int32_t*)(pSlotCacheIndices + regs.esi);
                        if (index != -1)
                            *(int32_t*)(NightShadows::pCache + index * NightShadows::CacheStride + 0x04) = NightShadows::CacheTileSize();
                    });

                    // Fading shadows: set for each light the lighting draws with a shadow
                    pattern = hook::pattern("55 8B EC 83 E4 F0 83 EC 2C F3 0F 10 05 ? ? ? ? F3 0F 11 44 24 0C F3 0F 10 05 ? ? ? ? 56 8D 44 24 10 50 FF 35");
                    static auto ShadowFadeHook = safetyhook::create_mid(pattern.get_first(0), [](SafetyHookContext& regs)
                    {
                        NightShadows::SetShadowFade(*(int32_t*)(regs.esp + 0x04));
                    });

                    // Perspective spotlight shadows: the slot's projection is chosen before its camera is set up, which
                    // the game's perspective type builds through its own function, and the shadow shader then renders
                    // the depth along the beam
                    pattern = hook::pattern("83 BE ? ? ? ? 02 55 8D AE ? ? ? ? 52 53 75 24 E8");
                    NightShadows::pSlots = *pattern.get_first<uintptr_t>(2);
                    static auto SlotProjectionHook = safetyhook::create_mid(pattern.get_first(0), [](SafetyHookContext& regs)
                    {
                        // An uncached perspective light, or a light admitted on a cut before its cache was ready,
                        // draws buildings, trees and lampposts into its live map until a cached baseline is available
                        if (NightShadows::ChooseSlotProjection(regs.esi))
                            *(uint32_t*)(regs.edi + 0x8D0) |= NightShadows::RenderMapFlags;
                    });
                    injector::MakeCALL(pattern.get_first(18), NightShadows::SetUpPerspectiveSlot, true);

                    pattern = hook::pattern("0F 28 05 ? ? ? ? 56 8D 44 24 10 50 FF 35 ? ? ? ? 0F 29 44 24 18 E8");
                    static auto PerspectiveModeHook = safetyhook::create_mid(pattern.get_first(24), [](SafetyHookContext& regs)
                    {
                        constexpr float NormalizedPerspective = 3.0f;
                        *(float*)regs.eax = NormalizedPerspective;
                    });

                    // The drawing pass scales the projected position by the slot's cone (+0x0C) in the vehicles' shaders (the
                    // others don't): the matrix already holds the cone, so vehicles were drawn too far from the beam's centre and
                    // their shadows landed above and beside them. The cone stays in the slot for the shadow's view.
                    pattern = hook::pattern("C7 44 24 20 CD CC 4C 3E C7 44 24 24 00 00 00 00 F3 0F 11 44 24 2C E8");
                    static auto PerspectiveScaleHook = safetyhook::create_mid(pattern.get_first(22), [](SafetyHookContext& regs)
                    {
                        ((float*)regs.eax)[2] = 1.0f;
                    });

                    pattern = hook::pattern("A1 ? ? ? ? 83 C4 08 83 F9 08 7D ? 8D 34 C1 69 F6 10 01 00 00 81 C6");
                    NightShadows::pSlotCopyIndex = *pattern.get_first<int32_t*>(1);
                    NightShadows::pSlotCopies = *pattern.get_first<uintptr_t>(24);

                    // Sixteen cached shadows: the loops over the cache, and the one cached shadow rendered each frame
                    for (auto p : { "05 00 01 00 00 41 3D ? ? ? ? 7C ? 83 FA FF", "05 00 01 00 00 41 3D ? ? ? ? 7C ? 83 C8 FF C3" })
                    {
                        pattern = hook::pattern(p);
                        injector::WriteMemory(pattern.get_first(7), *pattern.get_first<uint32_t>(7) + 8 * NightShadows::CacheStride, true);
                    }
                    pattern = hook::pattern("81 C1 00 04 00 00 81 F9 ? ? ? ? 0F 8C");
                    injector::WriteMemory(pattern.get_first(8), *pattern.get_first<uint32_t>(8) + 8 * NightShadows::CacheStride, true);
                    pattern = hook::pattern("41 81 E1 07 00 00 80 79 05 49 83 C9 F8 41");
                    injector::WriteMemory<uint8_t>(pattern.get_first(3), NightShadows::CacheCount - 1, true);
                    injector::WriteMemory<uint8_t>(pattern.get_first(12), uint8_t(-NightShadows::CacheCount), true);

                    NightShadows::bStreetlightsCached = true;

                    // The headlights of the traffic: the game adds them without a shadow and without the vehicle's
                    // tracking ID. Near the camera they ask for one like the player's, so a person in front of a car
                    // or the player next to the car they left casts a shadow; the priorities decide which get one.
                    static auto DoHeadLightsEffectReturn = hook::get_pattern("83 C0 30 50 FF 75 24 E8 ? ? ? ? 83 C4 34 5F 5E 8B E5 5D C2 24 00", 12);
                    pattern = hook::pattern("68 00 01 00 00 6A 02 6A 00 E8");
                    static auto TrafficHeadlightHook = safetyhook::create_mid(pattern.get_first(9), [](SafetyHookContext& regs)
                    {
                        // AddVehicleLight also handles rear lights, which reuse the single-headlight ID.
                        // EBP+4 is its caller's return address, preserved across this mid-hook.
                        if (*(void**)(regs.ebp + 0x04) != DoHeadLightsEffectReturn)
                            return;

                        static auto HeadlightShadows = FusionFixSettings.GetRef("PREF_HEADLIGHTSHADOWS");
                        auto trackingID = *(uint32_t*)(regs.ebp + 0x30);
                        auto position = *(rage::Vector3**)(regs.esp + 0x14);
                        if (HeadlightShadows->get() && trackingID && NightShadows::IsNearCamera(*position, NightShadows::TrafficHeadlightRange))
                        {
                            *(uint32_t*)(regs.esp + 0x08) |= rage::LIGHTFLAG_CAST_DYNAMIC_GEOM_SHADOWS;
                            *(uint32_t*)(regs.esp + 0x3C) = trackingID;
                        }
                    });
                }
            }

            // Shadows of lights with a projected texture (the player's headlights): the game sets their shadow
            // parameters, but the technique that draws these lights is bound without them
            {
                auto pattern = hook::pattern("8B 44 24 04 56 83 F8 17 0F 87");
                if (!pattern.empty()) // 1.2.0.32, 1.2.0.43, 1.2.0.59
                {
                    NightShadows::ShadowParams::shStartShader = safetyhook::create_inline(pattern.get_first(0), NightShadows::ShadowParams::StartShader);

                    pattern = hook::pattern("FF 35 ? ? ? ? FF 35 ? ? ? ? E8 ? ? ? ? 8D 44 24 44 50 FF 35 ? ? ? ? FF 35 ? ? ? ? E8 ? ? ? ? 83");
                    NightShadows::ShadowParams::pVarMatrix = *pattern.get_first<int32_t*>(2);
                    NightShadows::ShadowParams::pVarOffsetScale = *pattern.get_first<int32_t*>(24);
                    NightShadows::ShadowParams::shSetMatrix = safetyhook::create_inline(injector::GetBranchDestination(pattern.get_first(12)).as_int(), NightShadows::ShadowParams::SetMatrix);
                    NightShadows::ShadowParams::shSetVector = safetyhook::create_inline(injector::GetBranchDestination(pattern.get_first(34)).as_int(), NightShadows::ShadowParams::SetVector);
                    NightShadows::ShadowParams::pVarParam0123 = *hook::get_pattern<int32_t*>("F3 0F 5E 46 14 8D 44 24 10 50 FF 35 ? ? ? ? F3 0F 11 44 24 24 FF 35", 12);
                    NightShadows::ShadowParams::pVarParam891113 = *hook::get_pattern<int32_t*>("8D 44 24 1C 50 FF 35 ? ? ? ? F3 0F 11 44 24 28 F3 0F 10 46 10 FF 35", 7);
                }
                else // 1.0.7.0, 1.0.8.0, 1.1.2.0, 1.1.3.0
                {
                    pattern = hook::pattern("8B 44 24 04 83 F8 17 56 0F 87");
                    NightShadows::ShadowParams::shStartShader = safetyhook::create_inline(pattern.get_first(0), NightShadows::ShadowParams::StartShader);

                    pattern = hook::pattern("8B 15 ? ? ? ? A1 ? ? ? ? 81 C6 80 00 00 00 56 52 50 E8 ? ? ? ? 8B 15 ? ? ? ? A1 ? ? ? ? 8D 4C 24 44 51 52 50 E8");
                    NightShadows::ShadowParams::pVarMatrix = *pattern.get_first<int32_t*>(2);
                    NightShadows::ShadowParams::pVarOffsetScale = *pattern.get_first<int32_t*>(27);
                    NightShadows::ShadowParams::shSetMatrix = safetyhook::create_inline(injector::GetBranchDestination(pattern.get_first(20)).as_int(), NightShadows::ShadowParams::SetMatrix);
                    NightShadows::ShadowParams::shSetVector = safetyhook::create_inline(injector::GetBranchDestination(pattern.get_first(43)).as_int(), NightShadows::ShadowParams::SetVector);
                    NightShadows::ShadowParams::pVarParam0123 = *hook::get_pattern<int32_t*>("F3 0F 10 86 CC 00 00 00 A1 ? ? ? ? 8B 0D", 9);
                    NightShadows::ShadowParams::pVarParam891113 = *hook::get_pattern<int32_t*>("F3 0F 10 46 28 A1 ? ? ? ? 8B 0D ? ? ? ? F3 0F 11 44 24 1C F3 0F 2A 46 08", 6);
                }
            }

            // Which lights get shadows
            {
                auto pattern = hook::pattern("8B 44 24 08 85 C0 75 03 33 C0 C3 8B 4C 24 04 F7 C1 00 04 00 00");
                static auto CompareHook = safetyhook::create_inline(pattern.get_first(0), NightShadows::CompareShadowLights);

                pattern = hook::pattern("8B 54 3E 60 B8 ? ? ? ? 90 39 10");
                if (!pattern.empty()) // 1.2.0.32, 1.2.0.43, 1.2.0.59
                {
                    NightShadows::pSlotTrackingIDs = *pattern.get_first<uint8_t*>(5);
                    NightShadows::pViewports = *hook::get_pattern<uintptr_t*>("A1 ? ? ? ? 66 0F 6E C1 8B 04 85", 1);
                    NightShadows::nVehicleOccupantsOffset = 0xF50;

                    pattern = hook::pattern("0F 51 C2 F3 0F 11 44 24 ? F3 0F 5C 05 ? ? ? ? 0F 2F C8");
                    static auto PriorityHook = safetyhook::create_mid(pattern.get_first(3), [](SafetyHookContext& regs)
                    {
                        auto& light = *(rage::CLightSource*)(regs.esi + regs.edi);
                        regs.xmm0.f32[0] = NightShadows::ShadowPriorityDistance(light, regs.xmm0.f32[0]);
                    });
                }
                else // 1.0.7.0, 1.0.8.0, 1.1.2.0, 1.1.3.0
                {
                    pattern = hook::pattern("8B 44 06 60 33 D2 B9 ? ? ? ?");
                    NightShadows::pSlotTrackingIDs = *pattern.get_first<uint8_t*>(7);
                    NightShadows::pViewports = *hook::get_pattern<uintptr_t*>("A1 ? ? ? ? 8B 04 85 ? ? ? ? F3 0F 11 05 ? ? ? ? F3 0F 10 40 70", 1);
                    NightShadows::nVehicleOccupantsOffset = 0xFA0;

                    pattern = hook::pattern("F3 0F 51 CB 0F 28 C1 F3 0F 5C 05 ? ? ? ? 0F 2F D0");
                    static auto PriorityHook = safetyhook::create_mid(pattern.get_first(4), [](SafetyHookContext& regs)
                    {
                        auto& light = *(rage::CLightSource*)(regs.esi + regs.eax);
                        regs.xmm1.f32[0] = NightShadows::ShadowPriorityDistance(light, regs.xmm1.f32[0]);
                    });
                }
            }

            // CWarpShadow::Update: repair the previous-light list on a camera cut before selecting shadows.
            {
                auto pattern = hook::pattern("55 8B EC 83 E4 F0 81 EC B8 00 00 00 A1 ? ? ? ? 33 C4 89 84 24 B4 00 00 00 80 3D ? ? ? ? 00 56 57 0F 84 ? ? ? ? E8 ? ? ? ? 84 C0");
                auto admission = hook::pattern("84 C0 0F 85 ? ? ? ? 83 7C 24 28 FF 74 ? F3 0F 10 4C 24 2C 0F 2F C8 0F 86");
                static bool legacy = pattern.empty();
                // The older allocator has the same slot/cache fields but different registers and local variables.
                if (legacy) // 1.0.7.0, 1.0.8.0
                {
                    pattern = hook::pattern("55 8B EC 83 E4 F0 81 EC 94 00 00 00 53 33 DB 38 1D ? ? ? ? 56 57 0F 84 ? ? ? ? E8 ? ? ? ?");
                    admission = hook::pattern("3A CB 0F 85 ? ? ? ? 39 7C 24 20 74 ? F3 0F 10 4C 24 18 0F 2F C8 76 ? F3 0F 11 44 24 18");
                }
                auto updateMatches = pattern.size();
                auto admissionMatches = admission.size();

                auto legacyCache = hook::pattern("8B 4C 24 04 8B 51 60 33 C0 B9 ? ? ? ? 8B FF 3B 11 74 14 81 C1 00 01 00 00 83 C0 01 81 F9");
                auto legacyScan = hook::pattern("0F B6 15 ? ? ? ? 6A 00 52 56 8D 86 FC 08 00 00 50 8D 8E F8 08 00 00 51 8D 96 00 09 00 00 52 E8 ? ? ? ? 83 C4 18 B9 ? ? ? ?");
                auto legacyDraw = hook::pattern("88 46 15 C6 46 16 01 75 0C 81 8E E8 08 00 00 00 02 00 00");
                auto legacyValid = !legacy || (legacyCache.size() == 1 && legacyScan.size() == 1 && legacyDraw.size() == 1);

                if (updateMatches == 1 && admissionMatches == 1 && legacyValid && ShadowCuts::Install(NightShadows::pViewports))
                {
                    // CWarpShadow::Update normally queues an uncached static+dynamic light for cache generation and
                    // skips live-slot selection. On a cut it may use a live slot immediately, rendering both kinds of
                    // geometry there. Keep that path while it owns the slot, until its ordinary static cache is ready.
                    static auto selectLiveSlot = uintptr_t(admission.get_first(legacy ? 0x25 : 0x2D));
                    static auto CutAdmissionHook = safetyhook::create_mid(admission.get_first(), [](SafetyHookContext& regs)
                    {
                        auto& light = *(rage::CLightSource*)(regs.esi + (legacy ? regs.eax : regs.edi));
                        if (!ShadowCuts::installed || light.mShadowCacheIndex != -1
                            || !(light.mFlags & rage::LIGHTFLAG_CAST_DYNAMIC_GEOM_SHADOWS)
                            || (!ShadowCuts::IsPreparedCut() && !NightShadows::HadShadowLastFrame(light.mShadowTrackingID)))
                            return;

                        // Preserve the native choice of one light to build a static cache for this update. At this
                        // branch EDX is the current index; the candidate index/priority are esp+0x28/+0x2C in CE,
                        // esp+0x20/+0x18 in Patch 7/8. ESI is the light offset, EDI (CE) or EAX (legacy) its list.
                        auto& cacheCandidate = *(int32_t*)(regs.esp + (legacy ? 0x20 : 0x28));
                        auto& cachePriority = *(float*)(regs.esp + (legacy ? 0x18 : 0x2C));
                        if (cacheCandidate == -1 || cachePriority > regs.xmm0.f32[0])
                        {
                            cacheCandidate = int32_t(regs.edx);
                            cachePriority = regs.xmm0.f32[0];
                        }
                        regs.eip = selectLiveSlot;
                    });
                    if (legacy)
                    {
                        // CWarpShadow::UpdateLight searches eight native cache owners at entry+0xE0.
                        NightShadows::pCache = *legacyCache.get_first<uintptr_t>(10) - 0xE0;
                        ShadowCuts::hooks.emplace_back(safetyhook::create_mid(legacyScan.get_first(), [](SafetyHookContext& regs)
                        {
                            // BuildRenderList has set the normal per-frame caster flags. An uncached cut-frame
                            // admission must also scan the static baseline; keep the native projection and slot type.
                            NightShadows::IncludeUncachedStaticGeometry(regs.esi);
                        }));
                        ShadowCuts::hooks.emplace_back(safetyhook::create_mid(legacyDraw.get_first(3), [](SafetyHookContext& regs)
                        {
                            // AddToDrawList resets the static-caster flag according to shadow quality. An uncached
                            // combined map still needs that baseline at every quality, not just during scanning.
                            NightShadows::IncludeUncachedStaticGeometry(regs.esi);
                        }));
                    }
                    NightShadows::shCWarpShadowUpdate = safetyhook::create_inline(pattern.get_first(), NightShadows::CWarpShadowUpdate);
                    if (!CutAdmissionHook || !NightShadows::shCWarpShadowUpdate
                        || !std::all_of(ShadowCuts::hooks.begin(), ShadowCuts::hooks.end(), [](const auto& hook) { return bool(hook); }))
                    {
                        ShadowCuts::installed = false;
                        CutAdmissionHook = {};
                        NightShadows::shCWarpShadowUpdate = {};
                        ShadowCuts::hooks.clear();
                        ShadowCuts::shMainUpdateViewport = {};
                        ShadowCuts::shProcessOne2dEffectLight = {};
                        ShadowCuts::shAddSceneLightOrdered = {};
                        CutsceneCuts::RemoveHook();
                    }
                }
            }

            // Leave what a light belongs to out of its shadow: set up with each shadow's render list
            {
                // Each shadow sets up its render list
                auto pattern = hook::pattern("FF B6 40 09 00 00 89 86 ? ? 00 00 50 E8 ? ? ? ? 83 C4 08 5E E9");
                int32_t callOffset = 13;
                if (pattern.empty()) // 1.0.7.0, 1.0.8.0, 1.1.2.0, 1.1.3.0
                {
                    pattern = hook::pattern("8B 8E 40 09 00 00 51 50 89 86 ? ? 00 00 E8 ? ? ? ? 83 C4 08 5E E9");
                    callOffset = 14;
                }
                static auto SetUpRenderListHook = safetyhook::create_mid(pattern.get_first(callOffset), [](SafetyHookContext& regs)
                {
                    NightShadows::SetUpExclusions(*(int32_t*)(regs.esi + 0x938), *(int32_t*)(regs.esi + 0x940));
                });
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
                    static auto VehicleNightShadows = FusionFixSettings.GetRef("PREF_VEHICLENIGHTSHADOWS");
                    if ((regs.eflags & 0x0040) && VehicleNightShadows->get())
                    {
                        auto pushTech = new CShaderFx_PushForcedTechnique(nLocalShadowTechId);
                        if (pushTech)
                            pushTech->Append();

                        CRenderer::AddToDrawListEntityListShadow(nRenderListIndex, 0, 0, 0, -1);
                        CRenderer::AddToDrawListEntityListShadow(nRenderListIndex, 1, 0, 0, -1);

                        auto popTech = new CShaderFx_PopForcedTechnique();
                        if (popTech)
                            popTech->Append();
                    }
                });
            }

            // Shadows of local lights draw people in vehicles too (the game left them out), except what the light's
            // exclusions leave out
            {
                auto pattern = hook::pattern("83 F8 03 75 14 F6 86 6C 02 00 00 04 74 0B 80 7C 24 0B 00 0F 84 ? ? ? ? C6 44 24 0C 00 83 F8 04 75 12 8B 86 10 02 00 00 C1 E8 0B 25 01 FF FF FF 89 44 24 0C 85 D2 75 4C 0F B6 46 62 50 FF 75 18 E8 ? ? ? ? 83 C4 08 3C 01 0F 85 ? ? ? ? 8B 46 28 25 C0 03 00 00 3D 80 00 00 00 74 17");
                if (!pattern.empty()) // 1.2.0.32, 1.2.0.43, 1.2.0.59
                {
                    static auto loc_AE3867 = resolve_next_displacement(pattern.get_first(14)).value();
                    static auto loc_AE374F = resolve_next_displacement(pattern.get_first(3)).value();
                    struct DrawShadowEntityHook
                    {
                        void operator()(injector::reg_pack& regs)
                        {
                            auto renderList = *(int32_t*)(regs.ebp + 0x08);
                            if (NightShadows::IsExcluded(renderList, regs.esi, regs.eax))
                                return_to(loc_AE3867);
                            return_to(loc_AE374F);
                        }
                    }; injector::MakeInline<DrawShadowEntityHook>(pattern.get_first(0), pattern.get_first(25));
                }
                else // 1.0.7.0, 1.0.8.0, 1.1.2.0, 1.1.3.0
                {
                    pattern = hook::pattern("83 F8 03 75 17 F6 86");
                    static auto loc_AE3867 = resolve_next_displacement(pattern.get_first(14)).value();
                    static auto loc_AE374F = resolve_next_displacement(pattern.get_first(3)).value();
                    struct DrawShadowEntityHook
                    {
                        void operator()(injector::reg_pack& regs)
                        {
                            auto renderList = *(int32_t*)(regs.ebp + 0x08);
                            if (NightShadows::IsExcluded(renderList, regs.esi, regs.eax))
                                return_to(loc_AE3867);
                            regs.ecx = *(uint32_t*)(regs.ebp + 0x14);
                            return_to(loc_AE374F);
                        }
                    }; injector::MakeInline<DrawShadowEntityHook>(pattern.get_first(0), pattern.get_first(25));
                }
            }

            // Streetlights: the shadow of what doesn't move is rendered once and kept in the cache, people and vehicles
            // are added to it each frame for the lights that have a per-frame shadow; off, they light like on consoles
            {
                auto pattern = hook::pattern("8B 55 20 F6 C1 06");
                if (!pattern.empty()) // 1.2.0.32, 1.2.0.43, 1.2.0.59
                {
                    static auto StreetlightFlagsHook = safetyhook::create_mid(pattern.get_first(0), [](SafetyHookContext& regs)
                    {
                        auto trackingID = *(uint32_t*)(regs.ebp + 0x20);
                        if (NightShadows::IsStreetlight(trackingID))
                            regs.ecx = NightShadows::StreetlightFlags(regs.ecx);
                        regs.ecx = NightShadows::OneShadowPerFixture(trackingID, regs.ecx);
                        *(uint32_t*)(regs.esp + 0x18) = regs.ecx;
                    });
                }
                else // 1.0.7.0, 1.0.8.0, 1.1.2.0, 1.1.3.0
                {
                    // Before the test of the shadow flags (light attributes in ecx), so both ways of adding the light see them
                    pattern = hook::pattern("F6 C3 06 75 02 33 FF F7 C1 00 00 01 00");
                    static auto StreetlightFlagsHook = safetyhook::create_mid(pattern.get_first(0), [](SafetyHookContext& regs)
                    {
                        auto trackingID = *(uint32_t*)(regs.ebp + 0x20);
                        if (NightShadows::IsStreetlight(trackingID))
                            regs.ebx = NightShadows::StreetlightFlags(regs.ebx);
                        regs.ebx = NightShadows::OneShadowPerFixture(trackingID, regs.ebx);
                    });
                }
            }

            // Headlights of the player's vehicle
            {
                // Spot light, LIGHTFLAG_USE_VEHICLE_TWIN | LIGHTFLAG_VEHICLE | LIGHTFLAG_CAST_DYNAMIC_GEOM_SHADOWS
                auto pattern = hook::pattern("68 04 05 00 00 6A 02 6A 00");
                CLights::hbAddSceneLight.fun = injector::MakeCALL(pattern.count(2).get(0).get<void*>(9), CLights::AddVehicleLightPlayer).get();
                CLights::hbAddSceneLight.fun = injector::MakeCALL(pattern.count(2).get(1).get<void*>(9), CLights::AddVehicleLightPlayer).get();
            }

            // Headlights of other vehicles
            {
                // Spot light, LIGHTFLAG_VEHICLE | LIGHTFLAG_CAST_DYNAMIC_GEOM_SHADOWS
                auto pattern = hook::pattern("68 04 01 00 00 6A 02 6A 00");
                CLights::hbAddSceneLight.fun = injector::MakeCALL(pattern.count(2).get(0).get<void*>(9), CLights::AddVehicleLightNPC).get();
                CLights::hbAddSceneLight.fun = injector::MakeCALL(pattern.count(2).get(1).get<void*>(9), CLights::AddVehicleLightNPC).get();
            }
        };
    }
} NightShadowsInit;
