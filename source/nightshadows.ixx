module;

#include <common.hxx>
#include <unordered_map>

export module nightshadows;

import common;
import comvars;
import settings;

// Night shadows: the shadows of local lights (streetlights, headlights, interior lights).
// The game renders seven of them per frame and keeps a small cache of static ones. Which lights get them is decided by
// the light's size on screen, visible lights first; a light that has a shadow keeps it unless another one is clearly
// more important, and the player's headlights always get one. A light never shadows what it belongs to, so a vehicle
// keeps its own shadow while its headlights cast theirs, and a lamppost doesn't darken the street with its own lamp.

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

    // Fusion Fix's lamppost models mark the lights the PC version gave shadows
    constexpr uint32_t LightAttrStreetlight = 0x8000000;

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

    bool IsNearCamera(const rage::Vector3& position, float range)
    {
        auto viewport = pViewports ? pViewports[pViewports[0]] : 0;
        if (!viewport)
            return false;

        // rage::grcViewport::mCameraMatrix, its position row
        auto camera = (const float*)(viewport + 0x10 + 0x40 + 0x30);
        auto dx = position.x - camera[0], dy = position.y - camera[1], dz = position.z - camera[2];
        return dx * dx + dy * dy + dz * dz < range * range;
    }

    // Shadows fade out before a light loses them: with distance, and as a cached light nears the importance of the
    // most important light that's waiting for a place in the cache, so the one that gives its place up has no shadow
    // left to lose and the one that takes it fades in. 0 is the full shadow, 1 none.
    float cacheFade[CacheCount] = {};
    float slotFade[SlotCount] = {};

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
        for (int32_t i = 0; i < CacheCount; i++)
        {
            auto fade = it.cached[i] < FLT_MAX ? DistanceFade(it.cached[i]) : 0.0f;
            if (it.waiting < FLT_MAX)
                fade = std::max(fade, SmoothStep(it.waiting * 0.8f, it.waiting, it.cached[i]));
            cacheFade[i] = fade;
        }

        for (int32_t slot = 1; slot < SlotCount; slot++)
        {
            auto i = FindLight(SlotTrackingID(slot));
            slotFade[slot] = i >= 0 && it.trackingIDs[i] ? DistanceFade(it.values[i]) : 0.0f;
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
        if (index >= 0 && index < CacheCount)
            it.cached[index] = std::min(it.cached[index], value);
        else if (light.mFlags & rage::LIGHTFLAG_CAST_STATIC_GEOM_SHADOWS)
            it.waiting = std::min(it.waiting, value);

        if (auto i = FindLight(light.mShadowTrackingID); i >= 0 && light.mShadowTrackingID)
        {
            it.trackingIDs[i] = light.mShadowTrackingID;
            it.values[i] = value;
            it.spot[i] = light.mType == rage::LT_SPOT;
            it.coneCos[i] = light.mOuterConeAngle;
            it.tangents[i][0] = light.mTangent.x;
            it.tangents[i][1] = light.mTangent.y;
            it.tangents[i][2] = light.mTangent.z;
        }
    }

    // Spotlights whose shadow is rendered every frame use a perspective projection aimed along the beam, so the whole
    // tile covers the cone; the pair of hemispheres the other lights use spends most of it outside the beam. Wider
    // beams keep the hemispheres.
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

    void ChooseSlotProjection(uint32_t slotOffset)
    {
        auto slot = slotOffset / SlotStride;
        auto base = pSlots + slotOffset;
        auto& type = *(int32_t*)(base + 0x00);
        type = 0;
        if (slot == 0 || slot >= SlotCount || *(int32_t*)(base + 0xFC) != DynamicRenderType || *(int32_t*)(base + 0xF0) != -1)
            return;

        auto& it = priorities;
        auto trackingID = *(uint32_t*)(base + 0xF8);
        auto i = FindLight(trackingID);
        if (i < 0 || !trackingID || it.trackingIDs[i] != trackingID || !it.spot[i] || it.coneCos[i] < MinPerspectiveConeCos)
            return;

        slotConeCos[slot] = it.coneCos[i];
        std::copy(std::begin(it.tangents[i]), std::end(it.tangents[i]), slotTangents[slot]);
        type = PerspectiveSlotType;
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
        auto scale = 1.0f / tanHalf;

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
            view[axis * 4 + 0] = right[axis] * scale;
            view[axis * 4 + 1] = up[axis] * scale;
            view[axis * 4 + 2] = back[axis];
            view[axis * 4 + 3] = 0.0f;
        }
        view[12] = -dot(position, right) * scale;
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
        SetOutdoorLight(light.mShadowTrackingID, light.mInteriorIndex == -1 && !(light.mFlags & rage::LIGHTFLAG_VEHICLE));

        constexpr float ReferenceRadius = 10.0f;
        auto value = distance * ReferenceRadius / std::max(light.mRadius, 1.0f);

        if (!IsSphereVisible(light.mPosition, light.mRadius))
            value *= 2.0f;

        if (HadShadowLastFrame(light.mShadowTrackingID))
            value *= 0.75f;
        else if (light.mShadowCacheIndex != -1)
            value *= 0.85f;

        NoteShadowPriority(light, value);
        return value;
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

    // What a light belongs to is left out of its shadow: the player's vehicle and the people in it for its
    // headlights, the lamppost for the lights of the lamppost. Otherwise a headlight shadows its whole beam with the
    // vehicle's front, and a lamppost darkens the street under it with its own lamp. Outdoor lights also leave out
    // people and vehicles when the Streetlight Shadows setting asks for it.
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

    // Lampposts far away are dummy objects, near ones objects, some are part of the map (buildings). The building pool is
    // large, so the lamp found for a light is kept a while.
    rage::fwPool<void*>** pDummyObjectPool = nullptr;
    rage::fwPool<void*>** pBuildingPool = nullptr;

    struct LampOfLight
    {
        uintptr_t lamp;
        uint32_t frame;
    };
    std::unordered_map<uint32_t, LampOfLight> lamps;
    constexpr uint32_t LampFrames = 600;

    // The lamp of a light: its tracking ID is the address of the entity it belongs to plus the light's index
    constexpr uint32_t MaxLightsPerEntity = 64;

    uintptr_t FindLamp(uint32_t trackingID)
    {
        auto frame = *CTimer::m_frameCount;
        if (auto it = lamps.find(trackingID); it != lamps.end() && frame - it->second.frame < LampFrames)
            return it->second.lamp;

        if (lamps.size() > 4096)
            lamps.clear();

        uintptr_t lamp = 0;
        for (auto pool : { CObject::GetObjectsPool(), *pDummyObjectPool, *pBuildingPool })
        {
            for (int32_t i = 0; pool && i < pool->GetSize(); i++)
            {
                auto entity = (uintptr_t)pool->GetSlot(i);
                if (entity && entity <= trackingID && trackingID - entity < MaxLightsPerEntity && entity > lamp)
                    lamp = entity;
            }
        }
        lamps[trackingID] = { lamp, frame };
        return lamp;
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

        // The lamp the light belongs to doesn't shadow its own light
        auto lamp = FindLamp(SlotTrackingID(slot));
        add(lamp);
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
            NightShadows::pDummyObjectPool = *hook::pattern("68 B0 36 00 00 8B C8 E8 ? ? ? ? A3").get_first<rage::fwPool<void*>**>(13);
            NightShadows::pBuildingPool = *hook::pattern("68 00 7D 00 00 8B C8 E8 ? ? ? ? A3").get_first<rage::fwPool<void*>**>(13);

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
                        NightShadows::ChooseSlotProjection(regs.esi);
                    });
                    injector::MakeCALL(pattern.get_first(18), NightShadows::SetUpPerspectiveSlot, true);

                    pattern = hook::pattern("0F 28 05 ? ? ? ? 56 8D 44 24 10 50 FF 35 ? ? ? ? 0F 29 44 24 18 E8");
                    static auto PerspectiveModeHook = safetyhook::create_mid(pattern.get_first(24), [](SafetyHookContext& regs)
                    {
                        constexpr float NormalizedPerspective = 3.0f;
                        *(float*)regs.eax = NormalizedPerspective;
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
                    pattern = hook::pattern("68 00 01 00 00 6A 02 6A 00 E8");
                    static auto TrafficHeadlightHook = safetyhook::create_mid(pattern.get_first(9), [](SafetyHookContext& regs)
                    {
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
                auto pattern = hook::pattern("83 F8 03 75 14 F6 86");
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
                        if (*(uint32_t*)(regs.edi + 0x4C) & NightShadows::LightAttrStreetlight)
                        {
                            regs.ecx = NightShadows::StreetlightFlags(regs.ecx);
                            *(uint32_t*)(regs.esp + 0x18) = regs.ecx;
                        }
                    });
                }
                else // 1.0.7.0, 1.0.8.0, 1.1.2.0, 1.1.3.0
                {
                    // Before the test of the shadow flags (light attributes in ecx), so both ways of adding the light see them
                    pattern = hook::pattern("F6 C3 06 75 02 33 FF F7 C1 00 00 01 00");
                    static auto StreetlightFlagsHook = safetyhook::create_mid(pattern.get_first(0), [](SafetyHookContext& regs)
                    {
                        if (regs.ecx & NightShadows::LightAttrStreetlight)
                            regs.ebx = NightShadows::StreetlightFlags(regs.ebx);
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
