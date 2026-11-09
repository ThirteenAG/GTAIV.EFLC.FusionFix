module;

#include <common.hxx>
#include <algorithm>
#include <cmath>
#include <utility>

export module autoladder;

import common;
import comvars;
import settings;

// The ladder search radius of the game, also used by the button
static constexpr float fButtonScanRadius = 4.0f;
static float fLadderScanRadius = fButtonScanRadius;
static float fAutoClimbRadius = 1.5f;

// No automatic grab right after leaving a ladder, longer in water
static constexpr uint32_t nOnFootRegrabCooldownMs = 350;
static constexpr uint32_t nInWaterRegrabCooldownMs = 2500;
static uint32_t nLastLadderExitTime = 0;

// Entry corridor in front of the ladder: half its width (m) while moving and while standing, the side entry from
// close by, and how much the stick and the heading must point at the ladder
static constexpr float fEntryHalfWidth = 0.775f;
static constexpr float fEntryHalfWidthStill = 0.575f;
static constexpr float fCloseSideEntryDistance = 0.9f;
static constexpr float fCloseSideEntryMaxSpeed = 1.5f;
static constexpr float fMinBottomApproachDot = 0.2f;
static constexpr float fMinTopApproachDot = 0.4f;

static bool IsAutoClimbEnabled()
{
    static auto AutoClimbLadders = FusionFixSettings.GetRef("PREF_AUTOCLIMBLADDERS");
    return AutoClimbLadders->get();
}

namespace CTaskSimpleClimbLadder
{
    enum Action { Idle = 0, ClimbUp = 1, ClimbDown = 2 };

    constexpr ptrdiff_t nAction = 24;
    constexpr ptrdiff_t nRequestedAction = 32;
    constexpr ptrdiff_t nTopZ = 56;
    constexpr ptrdiff_t nBottomZ = 72;
    constexpr ptrdiff_t nStepHeight = 168;
    constexpr ptrdiff_t nCanExitAtTop = 203;

    SafetyHookInline shProcessPed;
}

namespace CTaskComplexClimbLadder
{
    enum EntryMode { BottomEntry = 1, TopEntry = 4 };

    constexpr ptrdiff_t nEntryMode = 20;
    constexpr ptrdiff_t nLadder = 144;
    constexpr ptrdiff_t nLadderIndex = 148;

    SafetyHookInline shCreateSubTask;
    bool (__cdecl* FindTopAndBottomOfAllLadderSections)(uintptr_t ladder, int index, rage::Vector4* top, rage::Vector4* bottom, bool* isMetal);
    bool (__cdecl* CalculatePositionsForThisLadder)(uintptr_t ped, uintptr_t ladder, int index, rage::Vector4* top, rage::Vector4* bottom,
        rage::Vector4* position, float* heading, int mode, rage::Vector4* normal, bool checkSide);
}

namespace CControl
{
    uintptr_t (__thiscall* GetPedWalkLeftRight)(uintptr_t control);
    uintptr_t (__thiscall* GetPedWalkUpDown)(uintptr_t control);
    int (__cdecl* GetValue)(uintptr_t input);
    const float* pCameraHeading = nullptr;
}

static ptrdiff_t nPedDesiredHeading = 0;
static ptrdiff_t nPedMoveBlender = 0;
static ptrdiff_t nMoveBlenderDesiredRatio = 0;

static rage::Matrix44* GetMatrix(uintptr_t ped)
{
    return *reinterpret_cast<rage::Matrix44**>(ped + 0x20);
}

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
    if (lengthSquared < 0.000001f)
        return false;
    const float inverseLength = 1.0f / std::sqrt(lengthSquared);
    direction.x *= inverseLength;
    direction.y *= inverseLength;
    return true;
}

// The direction the stick points in the world, none in the dead zone
static bool ReadMovementDirection(uintptr_t control, Direction& direction)
{
    const float x = static_cast<float>(CControl::GetValue(CControl::GetPedWalkLeftRight(control)));
    const float y = -static_cast<float>(CControl::GetValue(CControl::GetPedWalkUpDown(control)));
    if (x * x + y * y <= 50.0f)
        return false;
    const float heading = std::atan2(-x, y) + *CControl::pCameraHeading;
    direction = { -std::sin(heading), std::cos(heading) };
    return true;
}

// Whether a walk along 'direction' crosses the ladder's entry line within halfWidth of the ladder
static bool CrossesEntry(const Direction& direction, const Direction& toLadder, const Direction& normal, float halfWidth)
{
    const float approachDot = Dot(direction, normal);
    if (std::abs(approachDot) < 0.0001f)
        return false;
    const float distance = Dot(toLadder, normal) / approachDot;
    if (distance <= 0.0f)
        return false;
    const Direction miss{ distance * direction.x - toLadder.x, distance * direction.y - toLadder.y };
    return Dot(miss, miss) < halfWidth * halfWidth;
}

static float GetDesiredMoveBlendRatio(uintptr_t ped)
{
    const auto moveBlender = *reinterpret_cast<const uintptr_t*>(ped + nPedMoveBlender);
    if (!moveBlender)
        return 0.0f;
    return std::clamp(*reinterpret_cast<const float*>(moveBlender + nMoveBlenderDesiredRatio), 0.0f, 3.0f);
}

static rage::Vector4 GetMoveSpeed(uintptr_t ped)
{
    alignas(16) rage::Vector4 speed{};
    auto getMoveSpeed = reinterpret_cast<rage::Vector4* (__thiscall*)(uintptr_t, rage::Vector4*)>((*reinterpret_cast<uintptr_t**>(ped))[0xEC / 4]);
    getMoveSpeed(ped, &speed);
    return speed;
}

// Walking around a ladder from behind to its usable side, along the game's own route
namespace RearApproach
{
    constexpr uint32_t nTimeoutMs = 4000;
    constexpr float fMaxDistance = 5.0f;
    constexpr int nFinishedTask = 1302;

    SafetyHookInline shCreateFirstSubTask, shControlSubTask;
    SafetyHookMid shDestructor;
    uintptr_t (__thiscall* GetRunningMovementTask)(uintptr_t task, uintptr_t ped);
    uintptr_t calculatingTask = 0;

    struct Candidate
    {
        uintptr_t ped = 0, ladder = 0;
        int index = -1;
        uint32_t time = 0;
        Direction direction{};
    };
    struct Approach
    {
        Candidate candidate;
        uintptr_t task = 0;
        rage::Vector4 start{}, previous{};
        float travelled = 0.0f;
        bool cancelled = false;
    };
    // The candidate found by the scan, a rear entry that failed (not tried again while the stick points the same
    // way) and the entry in progress
    Candidate pending, blocked;
    Approach active;

    int GetTaskType(uintptr_t task)
    {
        return reinterpret_cast<int (__thiscall*)(uintptr_t)>((*reinterpret_cast<uintptr_t**>(task))[3])(task);
    }

    void Finish(bool block = true)
    {
        if (block)
            blocked = active.candidate;
        active = {};
    }

    bool Matches(uintptr_t task, uintptr_t ped)
    {
        return active.task == task && active.candidate.ped == ped;
    }

    bool AtUsableEntry(uintptr_t task, uintptr_t ped)
    {
        if (*reinterpret_cast<const int*>(task + CTaskComplexClimbLadder::nEntryMode) != CTaskComplexClimbLadder::BottomEntry)
            return false;
        alignas(16) rage::Vector4 top{}, bottom{}, position{}, normal{};
        bool isMetal = false;
        float heading = 0.0f;
        if (!CTaskComplexClimbLadder::FindTopAndBottomOfAllLadderSections(active.candidate.ladder, active.candidate.index, &top, &bottom, &isMetal)
            || !CTaskComplexClimbLadder::CalculatePositionsForThisLadder(ped, active.candidate.ladder, active.candidate.index,
                &top, &bottom, &position, &heading, CTaskComplexClimbLadder::BottomEntry, &normal, true))
            return false;
        const auto& pos = GetMatrix(ped)->pos;
        const float x = pos.x - position.x;
        const float y = pos.y - position.y;
        return x * x + y * y <= 0.8f * 0.8f && std::abs(pos.z - 1.0f - bottom.z) <= 1.0f;
    }

    // Only the route to the ladder and the final slide are allowed; anything else ends the approach
    int FilterSubTask(uintptr_t task, uintptr_t ped, int type)
    {
        if (!Matches(task, ped))
            return type;
        if (!active.cancelled && IsAutoClimbEnabled())
        {
            if (type == std::to_underlying(TaskID::TaskComplexMoveFollowNavMeshRoute))
                return type;
            if (type == std::to_underlying(TaskID::TaskSimpleSlideToCoord) && AtUsableEntry(task, ped))
            {
                Finish(false);
                return type;
            }
        }
        Finish();
        return nFinishedTask;
    }

    // The game refuses the back side of a ladder: allow it for the approach, which walks around first
    bool __cdecl CalculateEntryPositions(uintptr_t ped, uintptr_t ladder, int index, rage::Vector4* top, rage::Vector4* bottom,
        rage::Vector4* position, float* heading, int mode, rage::Vector4* normal, bool checkSide)
    {
        if (calculatingTask && Matches(calculatingTask, ped) && ladder == active.candidate.ladder
            && index == active.candidate.index && mode == CTaskComplexClimbLadder::BottomEntry)
            checkSide = false;
        return CTaskComplexClimbLadder::CalculatePositionsForThisLadder(ped, ladder, index, top, bottom, position, heading, mode, normal, checkSide);
    }

    uintptr_t __fastcall CreateFirstSubTask(uintptr_t task, void* edx, uintptr_t ped)
    {
        if (!active.task && pending.ped == ped && IsAutoClimbEnabled()
            && *CTimer::m_snTimeInMilliseconds - pending.time <= 250
            && *reinterpret_cast<const uintptr_t*>(task + CTaskComplexClimbLadder::nLadder) == pending.ladder
            && *reinterpret_cast<const int*>(task + CTaskComplexClimbLadder::nLadderIndex) == pending.index)
        {
            active.candidate = std::exchange(pending, {});
            active.task = task;
            active.start = active.previous = GetMatrix(ped)->pos;
        }
        calculatingTask = Matches(task, ped) ? task : 0;
        const auto result = shCreateFirstSubTask.unsafe_fastcall<uintptr_t>(task, edx, ped);
        calculatingTask = 0;
        if (!result && Matches(task, ped))
            Finish();
        return result;
    }

    // Give up when the route leaves the ladder's level, is long, or is no longer a plain route
    bool ShouldCancelRoute(uintptr_t task, uintptr_t ped, const rage::Vector4& position)
    {
        const auto subTask = *reinterpret_cast<const uintptr_t*>(task + 8);
        if (!subTask)
            return false;
        if (GetTaskType(subTask) != std::to_underlying(TaskID::TaskComplexControlMovement))
            return true;
        const auto movement = GetRunningMovementTask(subTask, ped);
        if (!movement)
            return false;
        if (GetTaskType(movement) != std::to_underlying(TaskID::TaskComplexMoveFollowNavMeshRoute))
            return true;
        const auto flags = *reinterpret_cast<const uint32_t*>(movement + 0xD8);
        if (((flags >> 11) & 3) == 2)
            return true;
        const auto route = *reinterpret_cast<const uintptr_t*>(movement + 0x70);
        if (!(flags & 0x200) || !route)
            return false;
        const int count = *reinterpret_cast<const int*>(route);
        const int first = *reinterpret_cast<const int8_t*>(movement + 0x98);
        if (count < 0 || count > 32 || first < 0 || first > count)
            return true;
        float length = 0.0f;
        auto previous = position;
        for (int i = first; i < count; ++i)
        {
            const auto& point = *reinterpret_cast<const rage::Vector4*>(route + 16 * (i + 1));
            const float x = point.x - previous.x, y = point.y - previous.y;
            length += std::sqrt(x * x + y * y);
            if (std::abs(point.z - active.start.z) > 1.5f)
                return true;
            previous = point;
        }
        return length > fMaxDistance;
    }

    uintptr_t __fastcall ControlSubTask(uintptr_t task, void* edx, uintptr_t ped)
    {
        if (Matches(task, ped))
        {
            const auto& pos = GetMatrix(ped)->pos;
            bool cancel = active.cancelled || !IsAutoClimbEnabled() || *CTimer::m_snTimeInMilliseconds - active.candidate.time > nTimeoutMs;
            if (!cancel)
            {
                const float x = pos.x - active.previous.x, y = pos.y - active.previous.y;
                active.travelled += std::sqrt(x * x + y * y);
                active.previous = pos;
                cancel = active.travelled > fMaxDistance || std::abs(pos.z - active.start.z) > 1.0f || ShouldCancelRoute(task, ped, pos);
            }
            if (cancel)
            {
                active.cancelled = true;
                // MakeAbortable(ped, urgent)
                const auto abort = reinterpret_cast<bool (__thiscall*)(uintptr_t, uintptr_t, int, uintptr_t)>((*reinterpret_cast<uintptr_t**>(task))[5]);
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
}

namespace CTaskComplexClimbLadder
{
    // Short ladders are climbed in one step: the player would otherwise stop on them and need to push again
    struct ShortLadder
    {
        uintptr_t task = 0, ped = 0;
        bool pending = false, active = false, descending = false, ownsRequest = false;
        uint32_t startTime = 0;
    } shortLadder;

    constexpr float fBottomExitHeight = 1.5f;
    constexpr uint32_t nShortLadderTimeoutMs = 4000;

    uintptr_t __fastcall CreateSubTask(uintptr_t task, void* edx, int type, uintptr_t ped)
    {
        type = RearApproach::FilterSubTask(task, ped, type);
        const auto result = shCreateSubTask.unsafe_fastcall<uintptr_t>(task, edx, type, ped);
        if (result && ped == CPlayer::getLocalPlayerPed() && IsAutoClimbEnabled() && type == std::to_underlying(TaskID::TaskSimpleClimbLadder))
        {
            const int entryMode = *reinterpret_cast<const int*>(task + nEntryMode);
            if (entryMode == BottomEntry || entryMode == TopEntry)
                shortLadder = { result, ped, true, false, entryMode == TopEntry };
        }
        return result;
    }

    // Whether the player may climb into this ladder: the stick, the heading and the movement point at its entry
    template <bool InWater>
    bool CanAutoMountLadder(uintptr_t ped, uintptr_t ladder, int index, const Direction& movement, bool* rearEntry)
    {
        const auto matrix = GetMatrix(ped);
        alignas(16) rage::Vector4 top{}, bottom{}, position{}, normal{};
        bool isMetal = false;
        if (!FindTopAndBottomOfAllLadderSections(ladder, index, &top, &bottom, &isMetal) || bottom.z - matrix->pos.z > 1.0f)
            return false;

        const float feetZ = matrix->pos.z - 1.0f;
        const int mode = (InWater || std::abs(feetZ - bottom.z) <= std::abs(feetZ - top.z)) ? BottomEntry : TopEntry;
        float heading = 0.0f;
        const bool wrongSide = !CalculatePositionsForThisLadder(ped, ladder, index, &top, &bottom, &position, &heading, mode, &normal, true);
        if (wrongSide && (!rearEntry || mode != BottomEntry
            || !CalculatePositionsForThisLadder(ped, ladder, index, &top, &bottom, &position, &heading, mode, &normal, false)))
            return false;

        const float desiredHeading = *reinterpret_cast<const float*>(ped + nPedDesiredHeading);
        const Direction desired{ -std::sin(desiredHeading), std::cos(desiredHeading) };
        Direction facing{ matrix->up.x, matrix->up.y };
        Direction ladderNormal{ normal.x, normal.y };
        const auto& endpoint = mode == BottomEntry ? bottom : top;
        const Direction toLadder{ endpoint.x - matrix->pos.x, endpoint.y - matrix->pos.y };
        Direction toward = toLadder;
        if (!Normalize(facing) || !Normalize(ladderNormal) || !Normalize(toward))
            return false;

        const auto speed = GetMoveSpeed(ped);
        const Direction actualMovement{ speed.x, speed.y };
        const float speedSquared = Dot(actualMovement, actualMovement);

        if (mode == BottomEntry)
        {
            if (!wrongSide)
            {
                const float x = position.x - matrix->pos.x, y = position.y - matrix->pos.y;
                const float limit = (InWater ? 0.9f : 0.8f) + GetDesiredMoveBlendRatio(ped) * 0.1f;
                if (x * x + y * y >= limit * limit)
                    return false;
            }
            // cos(2 rad): not turning around
            if (Dot(desired, facing) <= std::cos(2.0f) || Dot(movement, toward) <= fMinBottomApproachDot)
                return false;

            const float sideDot = Dot(ladderNormal, toward);
            if (wrongSide && (Dot(toLadder, toLadder) > 1.0f || sideDot < 0.25f || std::abs(feetZ - bottom.z) > 1.0f))
                return false;
            const bool closeSideEntry = sideDot > -0.25f && sideDot < 0.15f
                && (InWater || (Dot(toLadder, toLadder) < fCloseSideEntryDistance * fCloseSideEntryDistance
                    && speedSquared <= fCloseSideEntryMaxSpeed * fCloseSideEntryMaxSpeed));
            if (closeSideEntry)
                return true;

            const float halfWidth = speedSquared > (InWater ? 0.0f : 0.001f) ? fEntryHalfWidth : fEntryHalfWidthStill;
            const bool intended = CrossesEntry(desired, toLadder, ladderNormal, halfWidth) || CrossesEntry(movement, toLadder, ladderNormal, halfWidth);
            if (rearEntry)
                *rearEntry = wrongSide && intended;
            return intended;
        }

        if (matrix->pos.z - top.z > 1.5f || top.z > matrix->pos.z
            || Dot(ladderNormal, toward) < -0.15f || Dot(movement, toward) < fMinTopApproachDot)
            return false;
        Direction travel = actualMovement;
        if (speedSquared > 0.001f && (!Normalize(travel) || Dot(travel, toward) < fMinTopApproachDot))
            return false;
        return Dot(ladderNormal, desired) > fMinTopApproachDot || Dot(ladderNormal, facing) > fMinTopApproachDot;
    }

    // Set by the input hooks when the scan that follows is an automatic one
    template <bool InWater>
    static bool bAutomaticScan = false;
    uintptr_t onFootControl = 0;

    template <bool InWater>
    static injector::hook_back<int(__cdecl*)(int, DWORD*, char)> hbScanForLadderToClimb;

    template <bool InWater>
    int __cdecl ScanForLadderToClimb(int ped, DWORD* index, char ignoreHeading)
    {
        const bool automatic = std::exchange(bAutomaticScan<InWater>, false);
        if constexpr (!InWater)
            RearApproach::pending = {};
        if (!automatic)
            return hbScanForLadderToClimb<InWater>.fun(ped, index, ignoreHeading);

        constexpr auto cooldown = InWater ? nInWaterRegrabCooldownMs : nOnFootRegrabCooldownMs;
        if (*CTimer::m_snTimeInMilliseconds - nLastLadderExitTime < cooldown)
            return 0;

        Direction movement{};
        const auto control = InWater ? CPlayerPed::GetControlFromPlayer(ped) : onFootControl;
        if (!control || !ReadMovementDirection(control, movement))
        {
            RearApproach::blocked = {};
            return 0;
        }
        if (Dot(movement, RearApproach::blocked.direction) < 0.5f)
            RearApproach::blocked = {};

        fLadderScanRadius = fAutoClimbRadius;
        auto ladder = hbScanForLadderToClimb<InWater>.fun(ped, index, !InWater);
        fLadderScanRadius = fButtonScanRadius;
        if (!ladder)
            return 0;

        bool rearEntry = false;
        if (!CanAutoMountLadder<InWater>(ped, ladder, static_cast<int>(*index), movement, InWater ? nullptr : &rearEntry))
        {
            *index = static_cast<DWORD>(-1);
            return 0;
        }

        if (rearEntry)
        {
            const auto& blocked = RearApproach::blocked;
            if (RearApproach::active.task || (blocked.ped == static_cast<uintptr_t>(ped) && blocked.ladder == static_cast<uintptr_t>(ladder)
                && blocked.index == static_cast<int>(*index)))
            {
                *index = static_cast<DWORD>(-1);
                return 0;
            }
            RearApproach::pending = { static_cast<uintptr_t>(ped), static_cast<uintptr_t>(ladder), static_cast<int>(*index), *CTimer::m_snTimeInMilliseconds, movement };
        }
        return ladder;
    }
}

namespace CTaskSimpleClimbLadder
{
    bool __fastcall ProcessPed(uintptr_t task, void* edx, uintptr_t ped)
    {
        auto& state = CTaskComplexClimbLadder::shortLadder;
        if (task == state.task && ped == state.ped && (state.pending || state.active || state.ownsRequest))
        {
            const int action = *reinterpret_cast<const int*>(task + nAction);
            const int climbAction = state.descending ? ClimbDown : ClimbUp;
            const auto control = CPlayerPed::GetControlFromPlayer(ped);
            const int stickY = control ? CControl::GetValue(CControl::GetPedWalkUpDown(control)) : 0;
            // Pushing the other way stops it
            if (!IsAutoClimbEnabled() || !control || (state.descending ? stickY < -63 : stickY > 63))
                state.pending = state.active = false;
            if (state.pending && action == Idle)
            {
                state.pending = false;
                const float step = *reinterpret_cast<const float*>(task + nStepHeight);
                const float bottomZ = *reinterpret_cast<const float*>(task + nBottomZ);
                const float height = *reinterpret_cast<const float*>(task + nTopZ) - bottomZ;
                const float remaining = GetMatrix(ped)->pos.z - (bottomZ + CTaskComplexClimbLadder::fBottomExitHeight);
                state.active = step > 0.0f && height > 0.0f && height <= step + CTaskComplexClimbLadder::fBottomExitHeight && remaining <= step
                    && (state.descending || *reinterpret_cast<const uint8_t*>(task + nCanExitAtTop));
                state.startTime = *CTimer::m_snTimeInMilliseconds;
            }
            if (state.active)
            {
                if (*CTimer::m_snTimeInMilliseconds - state.startTime > CTaskComplexClimbLadder::nShortLadderTimeoutMs
                    || (action != Idle && action != climbAction))
                    state.active = false;
                else if (action == Idle)
                {
                    *reinterpret_cast<int*>(task + nRequestedAction) = climbAction;
                    state.ownsRequest = true;
                }
            }
            if (!state.active && state.ownsRequest)
            {
                auto& request = *reinterpret_cast<int*>(task + nRequestedAction);
                if (request == climbAction)
                    request = Idle;
                state.ownsRequest = false;
            }
        }
        return shProcessPed.unsafe_fastcall<bool>(task, edx, ped);
    }
}

class AutoLadder
{
public:
    AutoLadder()
    {
        FusionFix::onInitEvent() += []()
        {
            SettingsReader reader;

            // [MISC]
            fAutoClimbRadius = reader.ReadFloat("MISC", "AutoClimbLaddersRange", 1.5f);

            auto pattern = find_pattern("56 57 8B F9 8D B7 58 27 00 00 56 E8", "56 8B F1 57 8D BE 58 27 00 00 57 E8");
            CControl::GetPedWalkLeftRight = (decltype(CControl::GetPedWalkLeftRight))pattern.get_first(0);
            CControl::GetValue = (decltype(CControl::GetValue))injector::GetBranchDestination(pattern.get_first(11)).as_int();
            pattern = find_pattern("56 57 8B F9 8D B7 78 27 00 00 56 E8", "56 8B F1 57 8D BE 78 27 00 00 57 E8");
            CControl::GetPedWalkUpDown = (decltype(CControl::GetPedWalkUpDown))pattern.get_first(0);

            // The camera heading the walk controls are relative to
            pattern = hook::pattern("F3 0F 58 05 ? ? ? ? F3 0F 11 04 24 E8 ? ? ? ? D9 5C 24 0C");
            if (!pattern.empty()) // 1.2.0.32, 1.2.0.43, 1.2.0.59
                CControl::pCameraHeading = *pattern.get_first<const float*>(4);
            else // 1.0.7.0, 1.0.8.0, 1.1.2.0, 1.1.3.0
            {
                pattern = hook::pattern("D8 05 ? ? ? ? D9 1C 24 E8 ? ? ? ? D9 5C 24 0C 83 C4 04 F3 0F 5A 44 24 08");
                CControl::pCameraHeading = *pattern.get_first<const float*>(2);
            }

            // CTaskSimpleJumpLaunch::ProcessInit copies the heading to the desired heading
            pattern = find_pattern("80 8E 92 00 00 00 02 8B 87 ? ? ? ? 89 87", "80 8E 92 00 00 00 02 D9 87 ? ? ? ? D9 9F");
            nPedDesiredHeading = *pattern.get_first<int32_t>(15);

            pattern = hook::pattern("E8 ? ? ? ? F3 0F 10 44 24 18 5F F3 0F 11 46 18");
            if (!pattern.empty()) // 1.2.0.32, 1.2.0.43, 1.2.0.59
                nPedMoveBlender = *pattern.get_first<int32_t>(-22);
            else // 1.0.7.0, 1.0.8.0, 1.1.2.0, 1.1.3.0
            {
                pattern = hook::pattern("8B 8E 90 0A 00 00 6A 00 83 EC 08 D9 5C 24 04 D9 44 24 20 D9 1C 24 E8");
                nPedMoveBlender = *pattern.get_first<int32_t>(2);
            }
            pattern = find_pattern("F3 0F 11 41 ? FF 50 4C C2 0C 00", "F3 0F 11 41 10 FF D2 C2 0C 00");
            nMoveBlenderDesiredRatio = *pattern.get_first<uint8_t>(4);

            pattern = hook::pattern("FF B6 ? ? ? ? 51 E8 ? ? ? ? 83 C4 14 84 C0 0F 84");
            if (!pattern.empty()) // 1.2.0.32, 1.2.0.43, 1.2.0.59
                CTaskComplexClimbLadder::FindTopAndBottomOfAllLadderSections = (decltype(CTaskComplexClimbLadder::FindTopAndBottomOfAllLadderSections))injector::GetBranchDestination(pattern.get_first(7)).as_int();
            else // 1.0.7.0, 1.0.8.0, 1.1.2.0, 1.1.3.0
            {
                pattern = hook::pattern("55 8B EC 83 E4 F0 83 EC 64 53 56 8B 75 08 0F BF 46 2E 8B 1C 85 ? ? ? ? 57 68 00 01 00 00");
                CTaskComplexClimbLadder::FindTopAndBottomOfAllLadderSections = (decltype(CTaskComplexClimbLadder::FindTopAndBottomOfAllLadderSections))pattern.get_first(0);
            }
            pattern = find_pattern("55 8B EC 83 E4 F0 81 EC ? ? ? ? 56 57 8B 7D 0C FF 75 10", "55 8B EC 83 E4 F0 8B 55 10 83 EC 58 56 57 8B 7D 0C 0F BF 47 2E 8B 0C 85 ? ? ? ? 52 E8");
            CTaskComplexClimbLadder::CalculatePositionsForThisLadder = (decltype(CTaskComplexClimbLadder::CalculatePositionsForThisLadder))pattern.get_first(0);

            pattern = find_pattern("55 8B EC 83 E4 F0 83 EC 18 56 57 8B 7D 08 8B F1 81 FF 87 03 00 00", "55 8B EC 83 E4 F0 83 EC 14 53 56 57 8B 7D 08 81 FF 87 03 00 00 8B F1");
            CTaskComplexClimbLadder::shCreateSubTask = safetyhook::create_inline(pattern.get_first(0), CTaskComplexClimbLadder::CreateSubTask);

            pattern = find_pattern("55 8B EC 83 E4 F0 83 EC 68 56 8B 75 08 57 8B 46 38", "55 8B EC 83 E4 F0 83 EC 54 53 56 57 8B 7D 08 8B 47 38 85 C0 8B F1 74 0C 3B 87 C4 07 00 00");
            CTaskSimpleClimbLadder::shProcessPed = safetyhook::create_inline(pattern.get_first(0), CTaskSimpleClimbLadder::ProcessPed);

            // CTaskComplexPlayerOnFoot: a ladder scan without the button, unless the button was just pressed
            pattern = find_pattern("76 ? 8A 81 ? ? ? ? 32 C2 3C ? 76 ? F7 87 ? ? ? ? ? ? ? ? 74", "76 ? 8A 96 ? ? ? ? 32 D0 80 FA ? 76 ? F7 83 ? ? ? ? ? ? ? ? 74 ? 8B 44 24");
            static auto loc_ScanForLadder = resolve_next_displacement(pattern.get_first(2)).value();
            static auto nJumpPrevious = *pattern.get_first<uint32_t>(4);
            static bool controlInEcx = *pattern.get_first<uint8_t>(3) == 0x81;
            static auto CTaskComplexPlayerOnFoot__HandlePlayerInput_Hook = safetyhook::create_mid(pattern.get_first(0), [](SafetyHookContext& regs)
            {
                CTaskComplexClimbLadder::bAutomaticScan<false> = false;
                if (!IsAutoClimbEnabled())
                    return;

                const auto control = controlInEcx ? regs.ecx : regs.esi;
                const auto ped = controlInEcx ? regs.edi : regs.ebx;
                // The jump input record: XOR byte at +4, current value at +6, previous at +7
                const auto* input = reinterpret_cast<const uint8_t*>(control + nJumpPrevious - 7);
                const bool pressed = (input[6] ^ input[4]) > 0x7F && (input[7] ^ input[4]) <= 0x7F;
                if (pressed || (*reinterpret_cast<const uint32_t*>(ped + 0x270) & 0x80020000))
                    return;

                CTaskComplexClimbLadder::bAutomaticScan<false> = true;
                CTaskComplexClimbLadder::onFootControl = control;
                return_to(loc_ScanForLadder);
            });

            pattern = find_pattern("E8 ? ? ? ? 83 C4 ? 89 44 24 ? F6 87", "E8 ? ? ? ? 83 C4 ? 89 44 24 ? EB ? 8B 74 24");
            CTaskComplexClimbLadder::hbScanForLadderToClimb<false>.fun = injector::MakeCALL(pattern.get_first(0), CTaskComplexClimbLadder::ScanForLadderToClimb<false>).get();

            // CTaskComplexInWater: a ladder scan without the button
            pattern = find_pattern("80 F9 ? 0F 86 ? ? ? ? 8B 4E", "3C ? 0F 86 ? ? ? ? 8B 4B");
            static auto reg = *pattern.get_first<uint8_t>(0);
            static auto CTaskComplexInWater__HandlePlayerInput_Hook = safetyhook::create_mid(pattern.get_first(0), [](SafetyHookContext& regs)
            {
                CTaskComplexClimbLadder::bAutomaticScan<true> = false;
                auto& input = reg != 0x3C ? regs.ecx : regs.eax;
                if (IsAutoClimbEnabled() && (input & 0xFF) <= 0x7F)
                {
                    CTaskComplexClimbLadder::bAutomaticScan<true> = true;
                    input |= 0xFF;
                }
            });

            pattern = find_pattern("E8 ? ? ? ? 83 C4 ? 85 C0 74 ? 8B 87 ? ? ? ? 81 8F", "E8 ? ? ? ? 83 C4 ? 85 C0 74 ? 81 8E");
            CTaskComplexClimbLadder::hbScanForLadderToClimb<true>.fun = injector::MakeCALL(pattern.get_first(0), CTaskComplexClimbLadder::ScanForLadderToClimb<true>).get();

            pattern = find_pattern("F3 0F 10 05 ? ? ? ? F3 0F 51 CC", "F3 0F 10 25 ? ? ? ? 0F 28 C1 0F 28 D3");
            injector::WriteMemory<float*>(pattern.get_first(4), &fLadderScanRadius, true);

            pattern = find_pattern("56 57 8B 7C 24 ? 57 8B F1 E8 ? ? ? ? 57 8B CE E8 ? ? ? ? 8B 8F", "56 8B 74 24 ? 57 56 8B F9 E8 ? ? ? ? 56 8B CF E8 ? ? ? ? 8B 8E");
            static auto CTaskSimpleClimbLadder__CleanUpBeforeExit_Hook = safetyhook::create_mid(pattern.get_first(0), [](SafetyHookContext& regs)
            {
                nLastLadderExitTime = *CTimer::m_snTimeInMilliseconds;
                if (regs.ecx == CTaskComplexClimbLadder::shortLadder.task)
                    CTaskComplexClimbLadder::shortLadder = {};
            });

            // The rear approach: CTaskComplexClimbLadder's first subtask, its vtable for ControlSubTask and the destructor
            pattern = hook::pattern("8B 44 24 10 C7 06 ? ? ? ? C7 46 14 00 00 00 00 C7 46 20");
            uintptr_t* vtable = nullptr;
            if (!pattern.empty()) // 1.2.0.32, 1.2.0.43, 1.2.0.59
                vtable = *pattern.get_first<uintptr_t*>(6);
            else // 1.0.7.0, 1.0.8.0, 1.1.2.0, 1.1.3.0
            {
                pattern = hook::pattern("56 8B F1 E8 ? ? ? ? 0F 57 C0 8B 4C 24 08 C7 06 ? ? ? ? 33 C0 89 46 14 F3 0F 11 46 20");
                vtable = *pattern.get_first<uintptr_t*>(17);
            }
            RearApproach::shControlSubTask = safetyhook::create_inline(vtable[20], RearApproach::ControlSubTask);
            RearApproach::shDestructor = safetyhook::create_mid(vtable[0], [](SafetyHookContext& regs)
            {
                if (RearApproach::active.task == regs.ecx)
                    RearApproach::Finish();
            });

            pattern = find_pattern("51 A0 ? ? ? ? 56 8B F1 0F B6 D0 B9 01 00 00 00", "55 8B EC 83 E4 F0 51 80 3D ? ? ? ? 00 53 56 57 8B F1 75 07 C6 05 ? ? ? ? 01 8B BE 90 00 00 00");
            RearApproach::shCreateFirstSubTask = safetyhook::create_inline(pattern.get_first(0), RearApproach::CreateFirstSubTask);

            pattern = find_pattern("8B 4E 08 57 E8 ? ? ? ? 89 44 24 30 85 C0 0F 84 ? ? ? ? 8B C8 E8", "8B 4E 08 57 E8 ? ? ? ? 8B D8 85 DB 0F 84 ? ? ? ? 8B CB E8");
            RearApproach::GetRunningMovementTask = (decltype(RearApproach::GetRunningMovementTask))injector::GetBranchDestination(pattern.get_first(4)).as_int();

            pattern = find_pattern("E8 ? ? ? ? 83 C4 28 84 C0 74 13 57 68 AE 03 00 00", "E8 ? ? ? ? 83 C4 28 84 C0 74 16 53 68 AE 03 00 00");
            injector::MakeCALL(pattern.get_first(0), RearApproach::CalculateEntryPositions);
        };
    }
} AutoLadder;
