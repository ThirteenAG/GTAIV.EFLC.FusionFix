module;

#include <common.hxx>
#include <cmath>
#include <utility>
#include <algorithm>

export module autoladder;

import common;
import comvars;
import natives;
import settings;

export namespace CTaskSimpleClimbLadder
{
    constexpr int nIdleAction = 0;
    constexpr ptrdiff_t nActionOffset = 24;
}

namespace ShortLadderTraversal
{
    void TrackEntry(uintptr_t task, uintptr_t ped, bool fromTop);
    void OnTaskExit(uintptr_t task);
}

export namespace CTaskComplexClimbLadder
{
    // Follow-camera ped used to qualify short-ladder completion.
    void SetCameraPed(uintptr_t ped);
    namespace RearApproach
    {
        bool IsActiveForPed(uintptr_t ped);
    }
}

// Automatic entry configuration
static constexpr int32_t nRegrabCooldownMs = 2500; // ms before automatic re-entry from water
static constexpr int32_t nOnFootRegrabCooldownMs = 350; // ms before automatic re-entry on foot
static constexpr float fLadderEntryHalfWidth = 0.775f; // Entry corridor half-width while moving (m)
static constexpr float fLadderEntryHalfWidthStill = 0.575f; // Entry corridor half-width while stationary (m)
static constexpr float fCloseBottomEntryDistance = 0.9f; // Close side-approach range (m)
static constexpr float fSideEntryMaxSpeed = 1.5f; // Maximum speed for close side entry (m/s)
static constexpr float fMinMovementToLadderDot = 0.2f; // Minimum bottom-entry approach alignment
static constexpr float fTopEntryHeadingDot = 0.4f; // Minimum top-entry approach alignment

static float fLadderScanRadius = 4.0f;
static constexpr float fLadderCandidateRadius = 1.5f; // Automatic ladder search radius (m)

static int32_t nLastLadderExitTime = -nRegrabCooldownMs;

export bool IsAutoClimbEnabled()
{
    static auto AutoClimbLadders = FusionFixSettings.GetRef("PREF_AUTOCLIMBLADDERS");
    return AutoClimbLadders->get();
}

namespace CTaskComplexClimbLadder
{
    static constexpr int nBottomEntry = 1;
    static constexpr int nTopEntry = 4;
    static constexpr ptrdiff_t nEntryModeOffset = 20;

    static uintptr_t cameraPed = 0;

    void SetCameraPed(uintptr_t ped)
    {
        cameraPed = ped;
    }

    static SafetyHookInline shCreateSubTask;
    static uintptr_t (__thiscall* GetPedWalkLeftRight)(uintptr_t);
    static uintptr_t (__thiscall* GetPedWalkUpDown)(uintptr_t);
    static int (__cdecl* GetAxisValue)(uintptr_t);
    static const float* controlCameraHeading = nullptr;
    static uintptr_t onFootControl = 0;
    static bool (__cdecl* FindTopAndBottomOfAllLadderSections)(uintptr_t, int, rage::Vector4*, rage::Vector4*, bool*);
    static bool (__cdecl* CalculatePositionsForThisLadder)(uintptr_t, uintptr_t, int, rage::Vector4*, rage::Vector4*, rage::Vector4*, float*, int, rage::Vector4*, bool);

    // Navigate around the ladder to its usable side
    namespace RearApproach
    {
        static constexpr uint32_t nTimeoutMs = 4000; // Rear-entry navigation timeout (ms)
        static constexpr float fMaxDistance = 5.0f; // Maximum rear-entry travel and remaining route (m)
        static constexpr int nFinishedTask = 1302;
        static constexpr ptrdiff_t nLadderOffset = 144;
        static constexpr ptrdiff_t nLadderIndexOffset = 148;
        static SafetyHookInline shCreateFirstSubTask, shControlSubTask;
        static SafetyHookMid shDestructor;
        static uintptr_t (__thiscall* GetRunningMovementTask)(uintptr_t, uintptr_t);
        static bool installed = false;
        static thread_local uintptr_t calculatingTask = 0;

        struct Candidate
        {
            uintptr_t ped = 0, ladder = 0;
            int index = -1;
            uint32_t time = 0;
            float directionX = 0.0f, directionY = 0.0f;
        };
        struct Approach
        {
            Candidate candidate;
            uintptr_t task = 0;
            rage::Vector4 start{}, previous{};
            float travelled = 0.0f;
            bool cancelled = false;
        };
        static Candidate pending, blocked;
        static Approach active;

        bool IsActiveForPed(uintptr_t ped)
        {
            return ped && active.task && active.candidate.ped == ped && !active.cancelled && IsAutoClimbEnabled();
        }

        static void Finish(bool retryBlocked = true)
        {
            if (retryBlocked)
                blocked = active.candidate;
            active = {};
        }

        static bool Matches(uintptr_t task, uintptr_t ped)
        {
            return active.task == task && active.candidate.ped == ped;
        }

        static bool AtUsableEntry(uintptr_t task, uintptr_t ped)
        {
            if (*reinterpret_cast<const int*>(task + nEntryModeOffset) != nBottomEntry)
                return false;
            const auto matrix = *reinterpret_cast<const rage::Matrix44**>(ped + 0x20);
            if (!matrix)
                return false;
            alignas(16) rage::Vector4 top{}, bottom{}, position{}, normal{};
            bool isMetal = false;
            if (!FindTopAndBottomOfAllLadderSections(active.candidate.ladder, active.candidate.index, &top, &bottom, &isMetal))
                return false;
            float heading = 0.0f;
            if (!CalculatePositionsForThisLadder(ped, active.candidate.ladder, active.candidate.index,
                &top, &bottom, &position, &heading, nBottomEntry, &normal, true))
                return false;
            const float x = matrix->pos.x - position.x;
            const float y = matrix->pos.y - position.y;
            return x * x + y * y <= 0.8f * 0.8f && std::abs(matrix->pos.z - 1.0f - bottom.z) <= 1.0f;
        }

        static int FilterSubTask(uintptr_t task, uintptr_t ped, int type)
        {
            if (!Matches(task, ped))
                return type;
            if (active.cancelled || !IsAutoClimbEnabled())
            {
                Finish();
                return nFinishedTask;
            }
            if (type == std::to_underlying(TaskID::TaskComplexMoveGoToPointAndStandStill))
            {
                Finish();
                return nFinishedTask;
            }
            if (type == std::to_underlying(TaskID::TaskSimpleSlideToCoord))
            {
                const bool ready = IsAutoClimbEnabled() && AtUsableEntry(task, ped);
                Finish(!ready);
                return ready ? type : nFinishedTask;
            }
            if (type != std::to_underlying(TaskID::TaskComplexMoveFollowNavMeshRoute))
            {
                Finish();
                return nFinishedTask;
            }
            return type;
        }

        static bool __cdecl CalculateEntryPositions(uintptr_t ped, uintptr_t ladder, int index,
            rage::Vector4* top, rage::Vector4* bottom, rage::Vector4* position, float* heading,
            int mode, rage::Vector4* normal, bool checkSide)
        {
            if (calculatingTask && Matches(calculatingTask, ped) && IsAutoClimbEnabled()
                && ladder == active.candidate.ladder && index == active.candidate.index && mode == nBottomEntry)
                checkSide = false;
            return CalculatePositionsForThisLadder(ped, ladder, index, top, bottom, position, heading, mode, normal, checkSide);
        }

        static uintptr_t __fastcall CreateFirstSubTask(uintptr_t task, void* edx, uintptr_t ped)
        {
            const auto candidate = pending;
            const auto matrix = *reinterpret_cast<const rage::Matrix44**>(ped + 0x20);
            if (!active.task && candidate.ped == ped && matrix && IsAutoClimbEnabled()
                && static_cast<uint32_t>(*CTimer::m_snTimeInMilliseconds) - candidate.time <= 250
                && *reinterpret_cast<const uintptr_t*>(task + nLadderOffset) == candidate.ladder
                && *reinterpret_cast<const int*>(task + nLadderIndexOffset) == candidate.index)
            {
                pending = {};
                active.candidate = candidate;
                active.task = task;
                active.start = active.previous = matrix->pos;
            }
            const auto previous = std::exchange(calculatingTask, Matches(task, ped) ? task : 0);
            const auto result = shCreateFirstSubTask.unsafe_fastcall<uintptr_t>(task, edx, ped);
            calculatingTask = previous;
            if (!result && Matches(task, ped))
                Finish();
            return result;
        }

        static bool ShouldCancelRoute(uintptr_t task, uintptr_t ped, const rage::Vector4& position)
        {
            const auto wrapper = *reinterpret_cast<const uintptr_t*>(task + 8);
            if (!wrapper)
                return false;
            const auto wrapperType = reinterpret_cast<int (__thiscall*)(uintptr_t)>((*reinterpret_cast<uintptr_t**>(wrapper))[3]);
            if (wrapperType(wrapper) != std::to_underlying(TaskID::TaskComplexControlMovement))
                return true;
            const auto movement = GetRunningMovementTask(wrapper, ped);
            if (!movement)
                return false;
            const auto getType = reinterpret_cast<int (__thiscall*)(uintptr_t)>((*reinterpret_cast<uintptr_t**>(movement))[3]);
            if (getType(movement) != std::to_underlying(TaskID::TaskComplexMoveFollowNavMeshRoute))
                return true;
            const auto flags = *reinterpret_cast<const uint32_t*>(movement + 0xD8);
            if (((flags >> 11) & 3) == 2)
                return true;
            const auto route = *reinterpret_cast<const uintptr_t*>(movement + 0x70);
            if (!(flags & 0x200) || !route)
                return false;
            const int count = *reinterpret_cast<const int*>(route);
            if (count < 0 || count > 32)
                return true;
            if (!count)
                return false;
            const int first = *reinterpret_cast<const int8_t*>(movement + 0x98);
            if (first < 0 || first > count)
                return true;
            float length = 0.0f;
            auto previous = position;
            for (int i = 0; i < count; ++i)
            {
                const auto& point = *reinterpret_cast<const rage::Vector4*>(route + 16 * (i + 1));
                if (!std::isfinite(point.x) || !std::isfinite(point.y) || !std::isfinite(point.z))
                    return true;
                if (i >= first)
                {
                    const float x = point.x - previous.x, y = point.y - previous.y;
                    length += std::sqrt(x * x + y * y);
                    if (!std::isfinite(length) || std::abs(point.z - active.start.z) > 1.5f)
                        return true;
                    previous = point;
                }
            }
            if (length > fMaxDistance)
                return true;
            return false;
        }

        static uintptr_t __fastcall ControlSubTask(uintptr_t task, void* edx, uintptr_t ped)
        {
            if (Matches(task, ped))
            {
                const auto matrix = *reinterpret_cast<const rage::Matrix44**>(ped + 0x20);
                bool cancelled = active.cancelled;
                if (!IsAutoClimbEnabled() || !matrix
                    || static_cast<uint32_t>(*CTimer::m_snTimeInMilliseconds) - active.candidate.time > nTimeoutMs)
                    cancelled = true;
                else if (!cancelled)
                {
                    const float x = matrix->pos.x - active.previous.x, y = matrix->pos.y - active.previous.y;
                    active.travelled += std::sqrt(x * x + y * y);
                    active.previous = matrix->pos;
                    if (!std::isfinite(active.travelled) || active.travelled > fMaxDistance
                        || !std::isfinite(matrix->pos.z) || std::abs(matrix->pos.z - active.start.z) > 1.0f)
                        cancelled = true;
                    else
                        cancelled = ShouldCancelRoute(task, ped, matrix->pos);
                }
                if (cancelled)
                {
                    active.cancelled = true;
                    const auto abort = reinterpret_cast<bool (__thiscall*)(uintptr_t, uintptr_t, int, uintptr_t)>(
                        (*reinterpret_cast<uintptr_t**>(task))[5]);
                    if (abort(task, ped, 1, 0))
                    {
                        *reinterpret_cast<uint32_t*>(task + 12) |= 2;
                        Finish();
                    }
                }
            }
            const auto result = shControlSubTask.unsafe_fastcall<uintptr_t>(task, edx, ped);
            if (!result && Matches(task, ped))
                Finish();
            return result;
        }

        static void Install()
        {
            auto constructor = hook::pattern("8B 44 24 10 C7 06 ? ? ? ? C7 46 14 00 00 00 00 C7 46 20");
            auto first = find_pattern("51 A0 ? ? ? ? 56 8B F1 0F B6 D0 B9 01 00 00 00", "55 8B EC 83 E4 F0 51 80 3D ? ? ? ? 00 53 56 57 8B F1 75 07 C6 05 ? ? ? ? 01 8B BE 90 00 00 00");
            auto positions = hook::pattern("E8 ? ? ? ? 83 C4 28 84 C0 74 13 57 68 AE 03 00 00");
            auto movement = hook::pattern("8B 4E 08 57 E8 ? ? ? ? 89 44 24 30 85 C0 0F 84 ? ? ? ? 8B C8 E8");
            const auto table = *constructor.get_first<uintptr_t*>(6);
            GetRunningMovementTask = (decltype(GetRunningMovementTask))injector::GetBranchDestination(movement.get_first(4)).as_int();
            shCreateFirstSubTask = safetyhook::create_inline(first.get_first(0), CreateFirstSubTask);
            shControlSubTask = safetyhook::create_inline(reinterpret_cast<void*>(table[20]), ControlSubTask);
            shDestructor = safetyhook::create_mid(reinterpret_cast<void*>(table[0]), [](SafetyHookContext& regs)
            {
                if (active.task == regs.ecx)
                    Finish();
            });
            injector::MakeCALL(positions.get_first(0), CalculateEntryPositions);
            installed = true;
        }
    }

    static uintptr_t __fastcall CreateSubTask(uintptr_t task, void* edx, int type, uintptr_t ped)
    {
        type = RearApproach::FilterSubTask(task, ped, type);
        const auto result = shCreateSubTask.unsafe_fastcall<uintptr_t>(task, edx, type, ped);
        if (!result || ped != cameraPed || !IsAutoClimbEnabled())
            return result;

        const int entryMode = *reinterpret_cast<const int*>(task + nEntryModeOffset);
        if (type == std::to_underlying(TaskID::TaskSimpleClimbLadder) && (entryMode == nBottomEntry || entryMode == nTopEntry))
            ShortLadderTraversal::TrackEntry(result, ped, entryMode == nTopEntry);

        return result;
    }

    // Movement intent and entry tests
    struct Direction
    {
        float x, y;
    };

    static float Dot(const Direction& a, const Direction& b)
    {
        return a.x * b.x + a.y * b.y;
    }

    static bool Normalize(Direction& direction)
    {
        const float lengthSquared = Dot(direction, direction);
        if (!std::isfinite(lengthSquared) || lengthSquared < 0.000001f)
            return false;
        const float inverseLength = 1.0f / std::sqrt(lengthSquared);
        direction.x *= inverseLength;
        direction.y *= inverseLength;
        return true;
    }

    static bool ReadWorldMovementDirection(uintptr_t control, Direction& direction)
    {
        if (!control || !controlCameraHeading || !std::isfinite(*controlCameraHeading))
            return false;

        const float x = static_cast<float>(GetAxisValue(GetPedWalkLeftRight(control)));
        const float y = -static_cast<float>(GetAxisValue(GetPedWalkUpDown(control)));
        if (x * x + y * y <= 50.0f)
            return false;
        const float heading = std::atan2(-x, y) + *controlCameraHeading;
        direction = { -std::sin(heading), std::cos(heading) };
        return Normalize(direction);
    }

    static bool ProjectApproach(const Direction& direction, const Direction& toLadder,
        const Direction& normal, float halfWidth, bool& withinWidth)
    {
        withinWidth = false;
        const float approachDot = Dot(direction, normal);
        if (std::abs(approachDot) < 0.0001f)
            return false;
        const float distance = Dot(toLadder, normal) / approachDot;
        if (!std::isfinite(distance) || distance <= 0.0f)
            return false;

        const Direction miss{ distance * direction.x - toLadder.x, distance * direction.y - toLadder.y };
        const float missSquared = Dot(miss, miss);
        withinWidth = std::isfinite(missSquared) && missSquared < halfWidth * halfWidth;
        return true;
    }

    static const rage::Vector4* GetPhysicalMoveSpeed(uintptr_t ped, rage::Vector4& storage)
    {
        auto getMoveSpeed = reinterpret_cast<rage::Vector4* (__thiscall*)(uintptr_t, rage::Vector4*)>(
            (*reinterpret_cast<uintptr_t**>(ped))[0xEC / sizeof(uintptr_t)]);
        return getMoveSpeed(ped, &storage);
    }

    static ptrdiff_t nMoveBlendOffset = 0, nDesiredMoveBlendRatioOffset = 0;

    static void ResolveMoveBlendLayout()
    {
        auto caller = hook::pattern("E8 ? ? ? ? F3 0F 10 44 24 18 5F F3 0F 11 46 18");
        auto store = hook::pattern("F3 0F 11 41 ? FF 50 4C C2 0C 00");
        nMoveBlendOffset = *caller.get_first<int32_t>(-22);
        nDesiredMoveBlendRatioOffset = *store.get_first<uint8_t>(4);
    }

    static float GetDesiredMoveBlendRatio(uintptr_t ped)
    {
        if (!nMoveBlendOffset || !nDesiredMoveBlendRatioOffset)
            return 0.0f;
        const auto moveBlend = *reinterpret_cast<const uintptr_t*>(ped + nMoveBlendOffset);
        if (!moveBlend)
            return 0.0f;
        const float ratio = *reinterpret_cast<const float*>(moveBlend + nDesiredMoveBlendRatioOffset);
        return std::isfinite(ratio) ? std::clamp(ratio, 0.0f, 3.0f) : 0.0f;
    }

    template <bool InWater>
    static bool CanAutoMountLadder(uintptr_t ped, uintptr_t ladder, int index, const Direction& movement, bool* rearEntry = nullptr)
    {
        auto* matrix = *reinterpret_cast<rage::Matrix44**>(ped + 0x20);
        if (!matrix)
            return false;

        alignas(16) rage::Vector4 top{}, bottom{}, position{}, normal{};
        bool isMetal = false;
        if (!FindTopAndBottomOfAllLadderSections(ladder, index, &top, &bottom, &isMetal))
            return false;

        if (bottom.z - matrix->pos.z > 1.0f)
            return false;

        const float feetZ = matrix->pos.z - 1.0f;
        const int mode = (InWater || std::abs(feetZ - bottom.z) <= std::abs(feetZ - top.z)) ? nBottomEntry : nTopEntry;
        float heading = 0.0f;
        const bool wrongSide = !CalculatePositionsForThisLadder(ped, ladder, index, &top, &bottom, &position, &heading, mode, &normal, true);
        if (wrongSide)
        {
            if (!rearEntry || !RearApproach::installed || mode != nBottomEntry
                || !CalculatePositionsForThisLadder(ped, ladder, index, &top, &bottom, &position, &heading, mode, &normal, false))
                return false;
        }

        const float desiredHeading = *reinterpret_cast<const float*>(ped + 0xAA4);
        if (!std::isfinite(desiredHeading))
            return false;
        const Direction desired{ -std::sin(desiredHeading), std::cos(desiredHeading) };
        Direction facing{ matrix->up.x, matrix->up.y };
        Direction ladderNormal{ normal.x, normal.y };
        if (!Normalize(facing) || !Normalize(ladderNormal))
            return false;

        const auto& endpoint = mode == nBottomEntry ? bottom : top;
        const Direction toLadder{ endpoint.x - matrix->pos.x, endpoint.y - matrix->pos.y };
        Direction toward = toLadder;
        if (!Normalize(toward))
            return false;

        alignas(16) rage::Vector4 velocityStorage{};
        const auto* velocity = GetPhysicalMoveSpeed(ped, velocityStorage);
        if (!velocity)
            return false;
        const Direction actualMovement{ velocity->x, velocity->y };
        const float speedSquared = Dot(actualMovement, actualMovement);
        if (!std::isfinite(speedSquared))
            return false;

        if (mode == nBottomEntry)
        {
            if (!wrongSide)
            {
                const float x = position.x - matrix->pos.x, y = position.y - matrix->pos.y;
                const float distance = std::sqrt(x * x + y * y);
                const float ratio = GetDesiredMoveBlendRatio(ped);
                const float limit = (InWater ? 0.9f : 0.8f) + ratio * 0.1f;
                if (!std::isfinite(distance) || distance >= limit)
                    return false;
            }
            if (Dot(desired, facing) <= std::cos(2.0f)
                || Dot(movement, toward) <= fMinMovementToLadderDot)
                return false;

            const float sideDot = Dot(ladderNormal, toward);
            if (wrongSide && (Dot(toLadder, toLadder) > 1.0f || sideDot < 0.25f
                || std::abs(feetZ - bottom.z) > 1.0f))
                return false;
            const bool closeSideEntry = sideDot > -0.25f && sideDot < 0.15f
                && (InWater || (Dot(toLadder, toLadder) < fCloseBottomEntryDistance * fCloseBottomEntryDistance
                    && speedSquared <= fSideEntryMaxSpeed * fSideEntryMaxSpeed));
            if (closeSideEntry)
                return true;

            const float halfWidth = speedSquared > (InWater ? 0.0f : 0.001f) ? fLadderEntryHalfWidth : fLadderEntryHalfWidthStill;
            bool headingWithinWidth = false;
            bool movementWithinWidth = false;
            if (!ProjectApproach(desired, toLadder, ladderNormal, halfWidth, headingWithinWidth)
                || !ProjectApproach(movement, toLadder, ladderNormal, halfWidth, movementWithinWidth))
                return false;
            const bool intended = headingWithinWidth || movementWithinWidth;
            if (rearEntry)
                *rearEntry = wrongSide && intended;
            return intended;
        }

        if (matrix->pos.z - top.z > 1.5f || top.z > matrix->pos.z
            || Dot(ladderNormal, toward) < -0.15f
            || Dot(movement, toward) < fTopEntryHeadingDot)
            return false;
        if (speedSquared > 0.001f)
        {
            Direction travel = actualMovement;
            if (!Normalize(travel) || Dot(travel, toward) < fTopEntryHeadingDot)
                return false;
        }
        return Dot(ladderNormal, desired) > fTopEntryHeadingDot
            || Dot(ladderNormal, facing) > fTopEntryHeadingDot;
    }

    // Automatic ladder selection
    template <bool InWater>
    static bool bAutomaticEntry = false;

    template <bool InWater>
    static injector::hook_back<int(__cdecl*)(int, DWORD*, char)> hbScanForLadderToClimb;

    template <bool InWater>
    static int __cdecl ScanForLadderToClimb(int ped, DWORD* index, char ignoreHeading)
    {
        const bool automatic = std::exchange(bAutomaticEntry<InWater>, false);
        if constexpr (!InWater)
            RearApproach::pending = {};
        if (!automatic)
            return hbScanForLadderToClimb<InWater>.fun(ped, index, ignoreHeading);

        constexpr auto cooldown = InWater ? nRegrabCooldownMs : nOnFootRegrabCooldownMs;
        if (static_cast<uint32_t>(*CTimer::m_snTimeInMilliseconds) - static_cast<uint32_t>(nLastLadderExitTime) < cooldown)
            return 0;

        Direction movement{};
        const auto control = InWater ? CPlayerPed::GetControlFromPlayer(ped) : onFootControl;
        if (!ReadWorldMovementDirection(control, movement))
        {
            if constexpr (!InWater)
                RearApproach::blocked = {};
            return 0;
        }
        if constexpr (!InWater)
        {
            if (movement.x * RearApproach::blocked.directionX + movement.y * RearApproach::blocked.directionY < 0.5f)
                RearApproach::blocked = {};
        }

        fLadderScanRadius = fLadderCandidateRadius;
        auto ladder = hbScanForLadderToClimb<InWater>.fun(ped, index, !InWater);
        fLadderScanRadius = 4.0f;

        bool rearEntry = false;
        if (ladder && !CanAutoMountLadder<InWater>(ped, ladder, static_cast<int>(*index), movement, InWater ? nullptr : &rearEntry))
        {
            *index = static_cast<DWORD>(-1);
            return 0;
        }

        if constexpr (!InWater)
        {
            if (ladder && rearEntry)
            {
                const auto& blocked = RearApproach::blocked;
                if (RearApproach::active.task || (blocked.ped == static_cast<uintptr_t>(ped)
                    && blocked.ladder == static_cast<uintptr_t>(ladder) && blocked.index == static_cast<int>(*index)))
                {
                    *index = static_cast<DWORD>(-1);
                    return 0;
                }
                RearApproach::pending = { static_cast<uintptr_t>(ped), static_cast<uintptr_t>(ladder), static_cast<int>(*index),
                    static_cast<uint32_t>(*CTimer::m_snTimeInMilliseconds), movement.x, movement.y };
            }
        }
        return ladder;
    }
}

// Complete one-step ladders without idling
namespace ShortLadderTraversal
{
    static constexpr int nClimbUpAction = 1;
    static constexpr int nClimbDownAction = 2;
    static constexpr ptrdiff_t nMovementRequestOffset = 32;
    static constexpr ptrdiff_t nTopZOffset = 56;
    static constexpr ptrdiff_t nBottomZOffset = 72;
    static constexpr ptrdiff_t nStepDistanceOffset = 168;
    static constexpr ptrdiff_t nCanExitAtTopOffset = 203;
    static constexpr float fBottomDismountHeight = 1.5f; // Bottom-exit root height above ladder base (m)
    static constexpr uint32_t nCompletionTimeoutMs = 4000; // Short-ladder completion timeout (ms)

    static SafetyHookInline shProcessPed;
    struct CompletionState
    {
        uintptr_t task = 0;
        uintptr_t ped = 0;
        bool pending = false;
        bool active = false;
        bool descending = true;
        bool ownsMovementRequest = false;
        uint32_t startTime = 0;
    };
    static CompletionState completion;

    static bool ReadClimbInput(uintptr_t control, float& y)
    {
        using namespace CTaskComplexClimbLadder;
        if (!control || !GetPedWalkUpDown || !GetAxisValue)
            return false;
        y = static_cast<float>(GetAxisValue(GetPedWalkUpDown(control)));
        return true;
    }

    void TrackEntry(uintptr_t task, uintptr_t ped, bool fromTop)
    {
        completion.task = task;
        completion.ped = ped;
        completion.pending = true;
        completion.active = false;
        completion.descending = fromTop;
        completion.ownsMovementRequest = false;
        completion.startTime = 0;
    }

    void OnTaskExit(uintptr_t task)
    {
        if (task == completion.task)
        {
            completion.pending = completion.active = false;
            completion.ownsMovementRequest = false;
            completion.task = completion.ped = 0;
        }
    }

    static bool __fastcall ProcessPed(uintptr_t task, void* edx, uintptr_t ped)
    {
        if (task == completion.task && ped == completion.ped
            && (completion.pending || completion.active || completion.ownsMovementRequest))
        {
            const int action = *reinterpret_cast<const int*>(task + CTaskSimpleClimbLadder::nActionOffset);
            const auto matrix = *reinterpret_cast<const rage::Matrix44**>(ped + 0x20);
            const auto control = CPlayerPed::GetControlFromPlayer ? CPlayerPed::GetControlFromPlayer(ped) : 0;
            float y = 0.0f;
            if (!IsAutoClimbEnabled() || !matrix || !ReadClimbInput(control, y)
                || (completion.descending ? y < -63.0f : y > 63.0f))
                completion.pending = completion.active = false;
            if (completion.pending && action == CTaskSimpleClimbLadder::nIdleAction)
            {
                completion.pending = false;
                const float step = *reinterpret_cast<const float*>(task + nStepDistanceOffset);
                const float remaining = matrix->pos.z - (*reinterpret_cast<const float*>(task + nBottomZOffset) + fBottomDismountHeight);
                const float height = *reinterpret_cast<const float*>(task + nTopZOffset) - *reinterpret_cast<const float*>(task + nBottomZOffset);
                const bool shortLadder = std::isfinite(step) && step > 0.0f && std::isfinite(height)
                    && height > 0.0f && height <= step + fBottomDismountHeight;
                const bool eligible = shortLadder && std::isfinite(remaining) && remaining <= step
                    && (completion.descending || *reinterpret_cast<const uint8_t*>(task + nCanExitAtTopOffset));
                completion.active = eligible;
                completion.startTime = static_cast<uint32_t>(*CTimer::m_snTimeInMilliseconds);
            }
            if (completion.active)
            {
                if (static_cast<uint32_t>(*CTimer::m_snTimeInMilliseconds) - completion.startTime > nCompletionTimeoutMs
                    || (action != CTaskSimpleClimbLadder::nIdleAction && action != (completion.descending ? nClimbDownAction : nClimbUpAction)))
                    completion.active = false;
                else if (action == CTaskSimpleClimbLadder::nIdleAction)
                {
                    *reinterpret_cast<int*>(task + nMovementRequestOffset) = completion.descending ? nClimbDownAction : nClimbUpAction;
                    completion.ownsMovementRequest = true;
                }
            }
            if (!completion.active && completion.ownsMovementRequest)
            {
                auto& request = *reinterpret_cast<int*>(task + nMovementRequestOffset);
                if (request == (completion.descending ? nClimbDownAction : nClimbUpAction))
                    request = 0;
                completion.ownsMovementRequest = false;
            }
        }
        return shProcessPed.unsafe_fastcall<bool>(task, edx, ped);
    }
}

// Module initialization
class AutoLadder
{
public:
    AutoLadder()
    {
        FusionFix::onInitEvent() += []()
        {
            auto onFootInput = find_pattern("76 ? 8A 81 ? ? ? ? 32 C2 3C ? 76 ? F7 87 ? ? ? ? ? ? ? ? 74", "76 ? 8A 96 ? ? ? ? 32 D0 80 FA ? 76 ? F7 83 ? ? ? ? ? ? ? ? 74 ? 8B 44 24");
            auto onFootScan = find_pattern("E8 ? ? ? ? 83 C4 ? 89 44 24 ? F6 87", "E8 ? ? ? ? 83 C4 ? 89 44 24 ? EB ? 8B 74 24");
            auto waterInput = find_pattern("80 F9 ? 0F 86 ? ? ? ? 8B 4E", "3C ? 0F 86 ? ? ? ? 8B 4B");
            auto waterScan = find_pattern("E8 ? ? ? ? 83 C4 ? 85 C0 74 ? 8B 87 ? ? ? ? 81 8F", "E8 ? ? ? ? 83 C4 ? 85 C0 74 ? 81 8E");
            auto scanRadius = find_pattern("F3 0F 10 05 ? ? ? ? F3 0F 51 CC", "F3 0F 10 25 ? ? ? ? 0F 28 C1 0F 28 D3");
            auto ladderCleanup = find_pattern("56 57 8B 7C 24 ? 57 8B F1 E8 ? ? ? ? 57 8B CE E8 ? ? ? ? 8B 8F", "56 8B 74 24 ? 57 56 8B F9 E8 ? ? ? ? 56 8B CF E8 ? ? ? ? 8B 8E");

            auto sections = hook::pattern("FF B6 ? ? ? ? 51 E8 ? ? ? ? 83 C4 14 84 C0 0F 84");
            auto ladderProcess = find_pattern("55 8B EC 83 E4 F0 83 EC 68 56 8B 75 08 57 8B 46 38", "55 8B EC 83 E4 F0 83 EC 54 53 56 57 8B 7D 08 8B 47 38 85 C0 8B F1 74 0C 3B 87 C4 07 00 00");
            auto positions = find_pattern("55 8B EC 83 E4 F0 81 EC ? ? ? ? 56 57 8B 7D 0C FF 75 10", "55 8B EC 83 E4 F0 8B 55 10 83 EC 58 56 57 8B 7D 0C 0F BF 47 2E 8B 0C 85 ? ? ? ? 52 E8");
            auto leftRight = hook::pattern("56 57 8B F9 8D B7 58 27 00 00 56 E8");
            auto upDown = hook::pattern("56 57 8B F9 8D B7 78 27 00 00 56 E8");
            auto cameraHeading = hook::pattern("F3 0F 58 05 ? ? ? ? F3 0F 11 04 24 E8 ? ? ? ? D9 5C 24 0C");

            CTaskComplexClimbLadder::GetPedWalkLeftRight = (decltype(CTaskComplexClimbLadder::GetPedWalkLeftRight))leftRight.get_first(0);
            CTaskComplexClimbLadder::GetPedWalkUpDown = (decltype(CTaskComplexClimbLadder::GetPedWalkUpDown))upDown.get_first(0);
            CTaskComplexClimbLadder::GetAxisValue = (decltype(CTaskComplexClimbLadder::GetAxisValue))injector::GetBranchDestination(leftRight.get_first(11)).as_int();
            CTaskComplexClimbLadder::controlCameraHeading = *cameraHeading.get_first<const float*>(4);
            CTaskComplexClimbLadder::ResolveMoveBlendLayout();

            if (!sections.empty())
                CTaskComplexClimbLadder::FindTopAndBottomOfAllLadderSections = (decltype(CTaskComplexClimbLadder::FindTopAndBottomOfAllLadderSections))injector::GetBranchDestination(sections.get_first(7)).as_int();
            else
            {
                sections = hook::pattern("55 8B EC 83 E4 F0 83 EC 64 53 56 8B 75 08 0F BF 46 2E 8B 1C 85 ? ? ? ? 57 68 00 01 00 00");
                CTaskComplexClimbLadder::FindTopAndBottomOfAllLadderSections = (decltype(CTaskComplexClimbLadder::FindTopAndBottomOfAllLadderSections))sections.get_first(0);
            }
            CTaskComplexClimbLadder::CalculatePositionsForThisLadder = (decltype(CTaskComplexClimbLadder::CalculatePositionsForThisLadder))positions.get_first(0);

            auto mount = find_pattern("55 8B EC 83 E4 F0 83 EC 18 56 57 8B 7D 08 8B F1 81 FF 87 03 00 00", "55 8B EC 83 E4 F0 83 EC 14 53 56 57 8B 7D 08 81 FF 87 03 00 00 8B F1");
            CTaskComplexClimbLadder::shCreateSubTask = safetyhook::create_inline(mount.get_first(0), CTaskComplexClimbLadder::CreateSubTask);
            ShortLadderTraversal::shProcessPed = safetyhook::create_inline(ladderProcess.get_first(0), ShortLadderTraversal::ProcessPed);

            static auto continueLadderEntry = resolve_next_displacement(onFootInput.get_first(2)).value();
            static auto previousInputOffset = *onFootInput.get_first<uint32_t>(4);
            static bool controlInEcx = *onFootInput.get_first<uint8_t>(3) == 0x81;
            static auto CTaskComplexPlayerOnFoot__HandlePlayerInput_Hook = safetyhook::create_mid(onFootInput.get_first(0), [](SafetyHookContext& regs)
            {
                CTaskComplexClimbLadder::bAutomaticEntry<false> = false;
                const auto* control = reinterpret_cast<const uint8_t*>(controlInEcx ? regs.ecx : regs.esi);
                const auto ped = controlInEcx ? regs.edi : regs.ebx;
                if (!IsAutoClimbEnabled())
                    return;

                const auto mask = control[previousInputOffset - 3];
                const bool pressed = (control[previousInputOffset - 1] ^ mask) > 0x7F
                    && (control[previousInputOffset] ^ mask) <= 0x7F;
                if (pressed || (*reinterpret_cast<const uint32_t*>(ped + 0x270) & 0x80020000))
                    return;

                CTaskComplexClimbLadder::bAutomaticEntry<false> = true;
                CTaskComplexClimbLadder::onFootControl = reinterpret_cast<uintptr_t>(control);
                return_to(continueLadderEntry);
            });

            CTaskComplexClimbLadder::hbScanForLadderToClimb<false>.fun = injector::MakeCALL(onFootScan.get_first(0), CTaskComplexClimbLadder::ScanForLadderToClimb<false>).get();

            static auto reg = *waterInput.get_first<uint8_t>(0);
            static auto CTaskComplexInWater__HandlePlayerInput_Hook = safetyhook::create_mid(waterInput.get_first(0), [](SafetyHookContext& regs)
            {
                CTaskComplexClimbLadder::bAutomaticEntry<true> = false;
                if (IsAutoClimbEnabled())
                {
                    auto& input = reg != 0x3C ? regs.ecx : regs.eax;
                    if ((input & 0xFF) <= 0x7F)
                    {
                        CTaskComplexClimbLadder::bAutomaticEntry<true> = true;
                        input |= 0xFF;
                    }
                }
            });

            CTaskComplexClimbLadder::hbScanForLadderToClimb<true>.fun = injector::MakeCALL(waterScan.get_first(0), CTaskComplexClimbLadder::ScanForLadderToClimb<true>).get();

            injector::WriteMemory<float*>(scanRadius.get_first(4), &fLadderScanRadius, true);

            static auto CTaskSimpleClimbLadder__CleanUpBeforeExit_Hook = safetyhook::create_mid(ladderCleanup.get_first(0), [](SafetyHookContext& regs)
            {
                nLastLadderExitTime = *CTimer::m_snTimeInMilliseconds;
                ShortLadderTraversal::OnTaskExit(regs.ecx);
            });
            CTaskComplexClimbLadder::RearApproach::Install();

        };
    }
} AutoLadder;
