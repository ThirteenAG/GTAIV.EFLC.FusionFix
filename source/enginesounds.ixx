module;

#include <common.hxx>

export module enginesounds;

import common;
import comvars;

// Car engines play from a small set of streamed wave slots (STREAM_ENGINE_n in waveslots.xml). A car asks for one every
// frame until it gets one and keeps it until the car is deleted, shared by every car with the same engine bank. The game
// hands out free slots first come, first served, so once enough different engines are around, parked or far away ones
// included, the next car gets none and stays silent, the player's own car too.
// Here a car that gets no slot can take one over: the player's car from whichever slot has its nearest user farthest
// away, any other car only from cars out of hearing range (where the game already turns their engine sound off). The
// cars that give the slot up turn dummy, as when they drive out of range, and wait for a slot of their own.
// Cars ask on the audio update thread, which runs alongside the main thread's frame, so the request only notes the car
// and the hand-over is done on the main thread once the audio update has finished (audGtaAudioEngine::FinishUpdate).
namespace audVehicleAudioEntity
{
    struct audEngineSlot
    {
        uintptr_t pWaveSlot;
        uint32_t nBankId;
        uint32_t nReferences;
        uint32_t nLastFreeTime;
    };

    constexpr uint32_t MaxEngineSlots = 25; // The game's table size, InitClass stops looking for STREAM_ENGINE_n there

    GameRef<std::array<audEngineSlot, MaxEngineSlots>> sm_EngineWaveSlots;
    GameRef<uint32_t> g_audNumEngineSlots;
    GameRef<uint32_t> g_NumEngineSlotsUsed;

    SafetyHookInline shRequestEngineWaveSlot = {};
    void (__cdecl* FreeEngineWaveSlot)(int32_t slot) = nullptr;
    void (__thiscall* ConvertToDummy)(uintptr_t entity) = nullptr;
    void (__thiscall* StopAndForget)(uintptr_t sound, bool allowRelease) = nullptr;

    // audVehicleAudioEntity fields, the same in every supported version
    constexpr ptrdiff_t VehicleOffset = 0x820;
    constexpr ptrdiff_t EngineSlotOffset = 0xA3C;
    constexpr ptrdiff_t EngineWasOnOffset = 0xB24;      // Engine state seen while dummy, ConvertFromDummy restarts the sounds when it was off
    constexpr ptrdiff_t EnginePreparedOffset = 0xB31;
    constexpr ptrdiff_t EnginePlayingOffset = 0xB32;
    constexpr ptrdiff_t EngineSoundOffsets[] = { 0x828, 0x82C, 0x830, 0x834, 0x840, 0x844 }; // Engine and exhaust loops, prepared from the slot

    // Squared distance beyond which every car is a dummy: the loudest class's real range (65 m) plus the game's margin
    constexpr float HearingRangeSq = 4225.0f + 100.0f;
    constexpr uint32_t ReservationTimeout = 5000;
    constexpr uint32_t OtherCarsInterval = 250;
}

class EngineSounds
{
    static inline uintptr_t pRequester = 0; // The car asking right now; boats ask without one
    static inline ptrdiff_t nEntityOffset = 0; // audVehicleAudioEntity inside CVehicle

    // Cars that got no slot during this audio update
    static inline std::array<uintptr_t, 64> aWaiting{};
    static inline uint32_t nWaiting = 0;

    // A slot given up for a car stays held by one extra reference until that car takes it, so no other car (or the
    // cars that just gave it up, whose bank is still loaded in it) gets it first
    static inline int32_t nReservedSlot = -1;
    static inline uintptr_t pReservedFor = 0;
    static inline uint32_t nReservedTime = 0;
    static inline uint32_t nLastOtherCarsTime = 0;

    static uintptr_t GetVehicle(uintptr_t entity)
    {
        return *(uintptr_t*)(entity + audVehicleAudioEntity::VehicleOffset);
    }

    static int32_t& GetEngineSlot(uintptr_t entity)
    {
        return *(int32_t*)(entity + audVehicleAudioEntity::EngineSlotOffset);
    }

    static float DistanceSq(uintptr_t a, uintptr_t b)
    {
        auto pa = *(float**)(a + 0x20) + 12;
        auto pb = *(float**)(b + 0x20) + 12;
        auto x = pa[0] - pb[0], y = pa[1] - pb[1], z = pa[2] - pb[2];
        return x * x + y * y + z * z;
    }

    static void Hold(int32_t slot)
    {
        auto& s = audVehicleAudioEntity::sm_EngineWaveSlots.get()[slot];
        if (s.nReferences++ == 0)
            ++audVehicleAudioEntity::g_NumEngineSlotsUsed;
    }

    static void Reserve(int32_t slot, uintptr_t entity, uint32_t now)
    {
        Hold(slot);
        nReservedSlot = slot;
        pReservedFor = entity;
        nReservedTime = now;
    }

    static void FreeSlotFor(uintptr_t requester, uint32_t now)
    {
        using namespace audVehicleAudioEntity;

        auto vehicle = GetVehicle(requester);

        auto playerVehicle = (uintptr_t)FindPlayerVehicle(0);
        auto isPlayer = vehicle == playerVehicle;
        auto listener = playerVehicle ? playerVehicle : (uintptr_t)FindPlayerPed(0);
        if (!listener)
            return;

        auto requesterDistance = isPlayer ? 0.0f : DistanceSq(vehicle, listener);
        if (!isPlayer)
        {
            if (requesterDistance >= HearingRangeSq || now - nLastOtherCarsTime < OtherCarsInterval)
                return;
            nLastOtherCarsTime = now;
        }

        auto& slots = sm_EngineWaveSlots.get();
        auto count = std::min(g_audNumEngineSlots.get(), MaxEngineSlots);

        // A slot nobody uses any more is only waiting for its sounds to stop, the player's car keeps it
        for (uint32_t i = 0; i < count; i++)
        {
            if (slots[i].nReferences == 0)
            {
                if (isPlayer)
                    Reserve(i, requester, now);
                return;
            }
        }

        struct SlotUsers
        {
            uint32_t nUsers = 0;
            float fNearest = FLT_MAX;
            bool bPlayer = false;
        };
        std::array<SlotUsers, MaxEngineSlots> users{};

        auto pool = CVehicle::GetVehiclePool();
        for (int32_t i = 0; i < pool->GetSize(); i++)
        {
            auto v = (uintptr_t)pool->GetSlot(i);
            if (!v || !*(uintptr_t*)(v + 0x20)) // No matrix yet
                continue;
            auto slot = GetEngineSlot(v + nEntityOffset);
            if (slot < 0 || slot >= (int32_t)count)
                continue;
            auto& u = users[slot];
            u.nUsers++;
            u.fNearest = std::min(u.fNearest, DistanceSq(v, listener));
            u.bPlayer |= v == playerVehicle;
        }

        // Slots shared with anything else (boats keep their own index) are left alone
        int32_t victim = -1;
        auto farthest = isPlayer ? 0.0f : std::max(requesterDistance, HearingRangeSq);
        for (uint32_t i = 0; i < count; i++)
        {
            auto& u = users[i];
            if (u.bPlayer || u.nUsers != slots[i].nReferences || u.fNearest <= farthest)
                continue;
            farthest = u.fNearest;
            victim = i;
        }

        if (victim < 0)
            return;

        for (int32_t i = 0; i < pool->GetSize(); i++)
        {
            auto v = (uintptr_t)pool->GetSlot(i);
            if (!v)
                continue;
            auto entity = v + nEntityOffset;
            if (GetEngineSlot(entity) != victim)
                continue;
            // The engine loops are cut without their fade-out, which would keep the slot's bank in use for seconds
            for (auto offset : EngineSoundOffsets)
            {
                if (auto& sound = *(uintptr_t*)(entity + offset))
                {
                    StopAndForget(sound, false);
                    sound = 0;
                }
            }
            ConvertToDummy(entity);
            FreeEngineWaveSlot(victim);
            GetEngineSlot(entity) = -1;
            *(bool*)(entity + EngineWasOnOffset) = false;
            *(bool*)(entity + EnginePreparedOffset) = false;
            *(bool*)(entity + EnginePlayingOffset) = false;
        }

        Reserve(victim, requester, now);
    }

    static int32_t __cdecl RequestEngineWaveSlot(uint16_t bankId)
    {
        using namespace audVehicleAudioEntity;

        auto requester = std::exchange(pRequester, 0);
        auto now = *CTimer::m_snTimeInMilliseconds;

        if (nReservedSlot != -1)
        {
            auto mine = requester && requester == pReservedFor;
            auto expired = now - nReservedTime > ReservationTimeout;
            if (mine || expired || nReservedSlot >= (int32_t)g_audNumEngineSlots.get())
            {
                auto slot = std::exchange(nReservedSlot, -1);
                if (slot < (int32_t)g_audNumEngineSlots.get())
                    FreeEngineWaveSlot(slot);

                if (mine)
                {
                    auto result = shRequestEngineWaveSlot.unsafe_ccall<int32_t>(bankId);
                    if (result == -1 && !expired && slot < (int32_t)g_audNumEngineSlots.get())
                        Reserve(slot, requester, nReservedTime);
                    return result;
                }
            }
            else if (sm_EngineWaveSlots.get()[nReservedSlot].nBankId == bankId)
            {
                return -1;
            }
        }

        auto result = shRequestEngineWaveSlot.unsafe_ccall<int32_t>(bankId);
        if (result == -1 && requester && nReservedSlot == -1)
        {
            nEntityOffset = requester - GetVehicle(requester);
            if (nWaiting < aWaiting.size())
                aWaiting[nWaiting++] = requester;
        }
        return result;
    }

    // Main thread, the audio update is done
    static void HandOverSlot()
    {
        auto waiting = std::exchange(nWaiting, 0);
        if (!waiting || nReservedSlot != -1)
            return;

        auto playerVehicle = (uintptr_t)FindPlayerVehicle(0);
        auto listener = playerVehicle ? playerVehicle : (uintptr_t)FindPlayerPed(0);
        if (!listener)
            return;

        // The player's car, else the nearest; only cars still in the pool, one may have been deleted since it asked
        uintptr_t requester = 0;
        auto nearest = FLT_MAX;
        auto pool = CVehicle::GetVehiclePool();
        for (int32_t i = 0; i < pool->GetSize(); i++)
        {
            auto v = (uintptr_t)pool->GetSlot(i);
            if (!v || !*(uintptr_t*)(v + 0x20) || std::find(aWaiting.begin(), aWaiting.begin() + waiting, v + nEntityOffset) == aWaiting.begin() + waiting)
                continue;
            auto distance = v == playerVehicle ? -1.0f : DistanceSq(v, listener);
            if (distance < nearest)
            {
                nearest = distance;
                requester = v + nEntityOffset;
            }
        }

        if (requester)
            FreeSlotFor(requester, *CTimer::m_snTimeInMilliseconds);
    }

public:
    EngineSounds()
    {
        FusionFix::onInitEventAsync() += []()
        {
            using namespace audVehicleAudioEntity;

            // audVehicleAudioEntity::UpdateCar, the car asking for its engine slot
            auto pattern = hook::pattern("0F B7 80 B8 00 00 00 50 E8 ? ? ? ? 8B 4C 24 20 83 C4 04 89 87 3C 0A 00 00 6A 00 E8");
            if (!pattern.empty()) // 1.2.0.32, 1.2.0.43, 1.2.0.59
            {
                static auto UpdateCarHook = safetyhook::create_mid(pattern.get_first(0), [](SafetyHookContext& regs)
                {
                    pRequester = regs.edi;
                });
            }
            else // 1.0.7.0, 1.0.8.0, 1.1.2.0, 1.1.3.0
            {
                pattern = hook::pattern("0F B7 80 B8 00 00 00 50 E8 ? ? ? ? 8B 4C 24 1C 83 C4 04 6A 00 89 86 3C 0A 00 00 E8");
                static auto UpdateCarHook = safetyhook::create_mid(pattern.get_first(0), [](SafetyHookContext& regs)
                {
                    pRequester = regs.esi;
                });
            }

            auto pRequestEngineWaveSlot = injector::GetBranchDestination(pattern.get_first(8)).as_int();
            StopAndForget = (decltype(StopAndForget))injector::GetBranchDestination(pattern.get_first(28)).as_int();
            g_audNumEngineSlots.SetAddress(*(uint32_t**)(pRequestEngineWaveSlot + 0x0B));
            sm_EngineWaveSlots.SetAddress(*(uintptr_t*)(pRequestEngineWaveSlot + 0x16));

            pattern = find_pattern("FF 05 ? ? ? ? 0F B7 C5 89 86", "01 3D ? ? ? ? 0F B7 D3 89 96");
            g_NumEngineSlotsUsed.SetAddress(*pattern.get_first<uint32_t*>(2));

            // ~audVehicleAudioEntity
            pattern = hook::pattern("83 F8 FF 74 13 50 E8 ? ? ? ? 83 C4 04 C7 86 3C 0A 00 00 FF FF FF FF");
            FreeEngineWaveSlot = (decltype(FreeEngineWaveSlot))injector::GetBranchDestination(pattern.get_first(6)).as_int();

            pattern = hook::pattern("56 8B F1 80 BE 22 0B 00 00 00 0F 84");
            ConvertToDummy = (decltype(ConvertToDummy))pattern.get_first(0);

            // audGtaAudioEngine::FinishUpdate, after waiting for the audio update thread
            pattern = find_pattern("80 3D ? ? ? ? 00 74 6C FF 35 ? ? ? ? E8 ? ? ? ? 83 C4 04 E8", "80 3D ? ? ? ? 00 74 3E A1 ? ? ? ? 50 E8 ? ? ? ? 83 C4 04 E8");
            static auto FinishUpdateHook = safetyhook::create_mid(pattern.get_first(23), [](SafetyHookContext& regs)
            {
                HandOverSlot();
            });

            shRequestEngineWaveSlot = safetyhook::create_inline(pRequestEngineWaveSlot, RequestEngineWaveSlot);
        };
    }
} EngineSounds;
