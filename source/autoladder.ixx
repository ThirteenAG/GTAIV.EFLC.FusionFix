module;

#include <common.hxx>
#include <cmath>
#include <utility>
#include <cstring>

export module autoladder;

import common;
import comvars;
import settings;

namespace CTaskSimpleClimbLadder
{
    static constexpr int nIdleAction = 0;
    static constexpr ptrdiff_t nActionOffset = 24;
}

namespace ShortLadderTraversal
{
    void TrackEntry(uintptr_t task, uintptr_t ped, bool fromTop);
    void OnTaskExit(uintptr_t task);
}

static constexpr int32_t nRegrabCooldownMs = 2500;
static constexpr int32_t nOnFootRegrabCooldownMs = 350;
static constexpr float fLadderEntryHalfWidth = 0.775f;
static constexpr float fLadderEntryHalfWidthStill = 0.575f;
static constexpr float fCloseBottomEntryDistance = 0.9f;
static constexpr float fSideEntryMaxSpeed = 1.5f;
static constexpr float fMinMovementToLadderDot = 0.2f;
static constexpr float fTopEntryHeadingDot = 0.4f;

static float fLadderScanRadius = 4.0f;
static float fAutoClimbRadius = 1.5f;

static int32_t nLastLadderExitTime = -nRegrabCooldownMs;

static bool IsAutoClimbEnabled()
{
    static auto AutoClimbLadders = FusionFixSettings.GetRef("PREF_AUTOCLIMBLADDERS");
    return AutoClimbLadders->get();
}

namespace LadderCamera
{
    static SafetyHookInline shTargetBetaCalc;
    static uintptr_t (__thiscall* FindActiveTaskByType)(uintptr_t, int);
    static uintptr_t (__thiscall* GetPedAimWeaponLeftRight)(uintptr_t);
    static uintptr_t (__thiscall* GetPedLookUpDown)(uintptr_t);
    static int (__cdecl* GetAxisValue)(uintptr_t);
    static uintptr_t cameraPed = 0;

    static constexpr float pi = 3.14159265358979323846f;
    static constexpr float arrivalTolerance = 2.0f * pi / 180.0f;

    struct LadderAlignment
    {
        uintptr_t ped = 0;
        uintptr_t task = 0;
        uintptr_t camera = 0;
        float heading = 0.0f;
    };
    // Idle and camera input pause correction without clearing the mount heading.
    static LadderAlignment alignment;

    static void BeginAlignment(uintptr_t ped, uintptr_t task, float heading)
    {
        alignment = { ped, task, 0, heading };
    }

    // Adjust horizontal follow only; IV retains its native ladder pitch correction.
    static void __fastcall TargetBetaCalc(uintptr_t camera, void* edx, float* beta, float target,
        float force, float rate, float cap, uintptr_t ped, uintptr_t support, float motion,
        uint32_t mode, float timestep)
    {
        cameraPed = ped;
        if (!IsAutoClimbEnabled())
            alignment = {};

        if (alignment.task)
        {
            const auto intelligence = ped ? *reinterpret_cast<const uintptr_t*>(ped + 0x224) : 0;
            const auto ladderTask = intelligence
                ? FindActiveTaskByType(intelligence + 68, std::to_underlying(TaskID::TaskComplexClimbLadder)) : 0;
            if (ped != alignment.ped || ladderTask != alignment.task
                || (alignment.camera && camera != alignment.camera))
            {
                alignment = {};
            }
            else
            {
                alignment.camera = camera;
                const auto simpleLadderTask = FindActiveTaskByType(intelligence + 68, std::to_underlying(TaskID::TaskSimpleClimbLadder));
                const bool idle = simpleLadderTask
                    && *reinterpret_cast<const int*>(simpleLadderTask + CTaskSimpleClimbLadder::nActionOffset) == CTaskSimpleClimbLadder::nIdleAction;
                const auto flags = *reinterpret_cast<const uint8_t*>(camera + 910);
                const auto forceFlags = *reinterpret_cast<const uint8_t*>(camera + 909);
                const auto control = CPlayerPed::GetControlFromPlayer ? CPlayerPed::GetControlFromPlayer(ped) : 0;
                if (!idle && control && !(flags & 0x80) && !(forceFlags & 4) && force == 0.0f
                    && std::isfinite(*beta) && std::isfinite(timestep) && timestep >= 0.0f)
                {
                    // Yield to camera input and native overrides while mounting or moving on the ladder.
                    const int lookX = GetAxisValue(GetPedAimWeaponLeftRight(control));
                    const int lookY = GetAxisValue(GetPedLookUpDown(control));
                    const float error = std::remainder(alignment.heading - *beta, 2.0f * pi);
                    if (!lookX && !lookY && std::abs(error) > arrivalTolerance)
                    {
                        target = *beta + error;
                        force = 1.0f;
                        // The native forced-yaw path doubles the cap.
                        cap *= 0.5f;
                    }
                }
            }
        }
        shTargetBetaCalc.unsafe_fastcall<void>(camera, edx, beta, target, force, rate, cap, ped, support, motion, mode, timestep);
    }
}

namespace CTaskComplexClimbLadder
{
    static constexpr int nBottomEntry = 1;
    static constexpr int nTopEntry = 4;
    static constexpr ptrdiff_t nEntryModeOffset = 20;
    static constexpr ptrdiff_t nHeadingOffset = 112;

    static SafetyHookInline shCreateSubTask;
    static uintptr_t (__thiscall* GetPedWalkLeftRight)(uintptr_t);
    static uintptr_t (__thiscall* GetPedWalkUpDown)(uintptr_t);
    static int (__cdecl* GetAxisValue)(uintptr_t);
    static const float* controlCameraHeading = nullptr;
    static uintptr_t onFootControl = 0;
    static bool (__cdecl* FindTopAndBottomOfAllLadderSections)(uintptr_t, int, rage::Vector4*, rage::Vector4*, bool*);
    static bool (__cdecl* CalculatePositionsForThisLadder)(uintptr_t, uintptr_t, int, rage::Vector4*, rage::Vector4*, rage::Vector4*, float*, int, rage::Vector4*, bool);

    static uintptr_t __fastcall CreateSubTask(uintptr_t task, void* edx, int type, uintptr_t ped)
    {
        const auto result = shCreateSubTask.unsafe_fastcall<uintptr_t>(task, edx, type, ped);
        if (!result || ped != LadderCamera::cameraPed || !IsAutoClimbEnabled())
            return result;

        const int entryMode = *reinterpret_cast<const int*>(task + nEntryModeOffset);
        if (type == std::to_underlying(TaskID::TaskSimpleClimbLadder) && (entryMode == nBottomEntry || entryMode == nTopEntry))
            ShortLadderTraversal::TrackEntry(result, ped, entryMode == nTopEntry);

        // Top entry starts with its mount animation; bottom entry mounts through the simple ladder task.
        if ((type == std::to_underlying(TaskID::TaskSimpleRunNamedAnim) && entryMode == nTopEntry)
            || (type == std::to_underlying(TaskID::TaskSimpleClimbLadder) && entryMode == nBottomEntry))
        {
            const float heading = *reinterpret_cast<const float*>(task + nHeadingOffset);
            if (std::isfinite(heading))
                LadderCamera::BeginAlignment(ped, task, heading);
        }
        return result;
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
        if (!std::isfinite(lengthSquared) || lengthSquared < 0.000001f)
            return false;
        const float inverseLength = 1.0f / std::sqrt(lengthSquared);
        direction.x *= inverseLength;
        direction.y *= inverseLength;
        return true;
    }

    static bool ReadWorldMovementDirection(Direction& direction)
    {
        if (!onFootControl || !controlCameraHeading || !std::isfinite(*controlCameraHeading))
            return false;

        // Match IV's UpdateMovementStickHistory conversion and native axis dead zone.
        const float x = static_cast<float>(GetAxisValue(GetPedWalkLeftRight(onFootControl)));
        const float y = -static_cast<float>(GetAxisValue(GetPedWalkUpDown(onFootControl)));
        if (x * x + y * y <= 50.0f)
            return false;
        const float heading = std::atan2(-x, y) + *controlCameraHeading;
        direction = { -std::sin(heading), std::cos(heading) };
        return Normalize(direction);
    }

    // Returns whether the ray intersects ahead; withinWidth reports whether it reaches the entry corridor.
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

    static bool CanAutoMountLadder(uintptr_t ped, uintptr_t ladder, int index, const Direction* movement)
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

        // Match CreateFirstSubTask, including joined ladder sections and top entry.
        const float feetZ = matrix->pos.z - 1.0f;
        const int mode = std::abs(feetZ - bottom.z) <= std::abs(feetZ - top.z) ? nBottomEntry : nTopEntry;
        float heading = 0.0f;
        if (!CalculatePositionsForThisLadder(ped, ladder, index, &top, &bottom, &position, &heading, mode, &normal, true))
            return false;

        if (movement)
        {
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
                if (Dot(desired, facing) <= std::cos(2.0f)
                    || Dot(*movement, toward) <= fMinMovementToLadderDot)
                    return false;

                // Limit the close-side allowance to slow approaches, not running past.
                const float sideDot = Dot(ladderNormal, toward);
                const bool closeSideEntry = Dot(toLadder, toLadder) < fCloseBottomEntryDistance * fCloseBottomEntryDistance
                    && sideDot > -0.25f && sideDot < 0.15f
                    && speedSquared <= fSideEntryMaxSpeed * fSideEntryMaxSpeed;
                if (closeSideEntry)
                    return true;

                const float halfWidth = speedSquared > 0.001f ? fLadderEntryHalfWidth : fLadderEntryHalfWidthStill;
                bool headingWithinWidth = false;
                bool movementWithinWidth = false;
                if (!ProjectApproach(desired, toLadder, ladderNormal, halfWidth, headingWithinWidth)
                    || !ProjectApproach(*movement, toLadder, ladderNormal, halfWidth, movementWithinWidth))
                    return false;
                return headingWithinWidth || movementWithinWidth;
            }

            if (matrix->pos.z - top.z > 1.5f || top.z > matrix->pos.z
                || Dot(ladderNormal, toward) < -0.15f
                || Dot(*movement, toward) < fTopEntryHeadingDot)
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
        return true;
    }

    template <bool InWater>
    static bool bAutomaticEntry = false;

    template <bool InWater>
    static injector::hook_back<int(__cdecl*)(int, DWORD*, char)> hbScanForLadderToClimb;

    template <bool InWater>
    static int __cdecl ScanForLadderToClimb(int ped, DWORD* index, char ignoreHeading)
    {
        // Consume the automatic-entry marker; manual scans keep native behavior.
        const bool automatic = std::exchange(bAutomaticEntry<InWater>, false);
        if (!automatic)
            return hbScanForLadderToClimb<InWater>.fun(ped, index, ignoreHeading);

        constexpr auto cooldown = InWater ? nRegrabCooldownMs : nOnFootRegrabCooldownMs;
        if (static_cast<uint32_t>(*CTimer::m_snTimeInMilliseconds) - static_cast<uint32_t>(nLastLadderExitTime) < cooldown)
            return 0;

        Direction movement{};
        if constexpr (!InWater)
        {
            if (!ReadWorldMovementDirection(movement))
                return 0;
        }

        fLadderScanRadius = fAutoClimbRadius;
        // On-foot orientation is checked below using both facing and movement intent.
        auto ladder = hbScanForLadderToClimb<InWater>.fun(ped, index, !InWater);
        fLadderScanRadius = 4.0f;

        // Reject failed entry before the player controller can create a standstill task.
        if (ladder && !CanAutoMountLadder(ped, ladder, static_cast<int>(*index), InWater ? nullptr : &movement))
        {
            *index = static_cast<DWORD>(-1);
            return 0;
        }

        return ladder;
    }
}

namespace ShortLadderTraversal
{
    static constexpr int nClimbUpAction = 1;
    static constexpr int nClimbDownAction = 2;
    static constexpr ptrdiff_t nMovementRequestOffset = 32;
    static constexpr ptrdiff_t nTopZOffset = 56;
    static constexpr ptrdiff_t nBottomZOffset = 72;
    static constexpr ptrdiff_t nStepDistanceOffset = 168;
    static constexpr ptrdiff_t nCanExitAtTopOffset = 203;
    static constexpr float fBottomDismountHeight = 1.5f;
    static constexpr uint32_t nCompletionTimeoutMs = 4000;

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
            // Evaluate once at the first idle after mounting, when the native step distance is available.
            if (completion.pending && action == CTaskSimpleClimbLadder::nIdleAction)
            {
                completion.pending = false;
                const float step = *reinterpret_cast<const float*>(task + nStepDistanceOffset);
                const float remaining = matrix->pos.z - (*reinterpret_cast<const float*>(task + nBottomZOffset) + fBottomDismountHeight);
                const float height = *reinterpret_cast<const float*>(task + nTopZOffset) - *reinterpret_cast<const float*>(task + nBottomZOffset);
                // Use the native rung-cycle distance and bottom dismount threshold.
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
                // Release only our request when interrupted; leave native task transitions alone.
                auto& request = *reinterpret_cast<int*>(task + nMovementRequestOffset);
                if (request == (completion.descending ? nClimbDownAction : nClimbUpAction))
                    request = 0;
                completion.ownsMovementRequest = false;
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
        FusionFix::onInitEventAsync() += []()
        {
            CIniReader iniReader("");

            // [MISC]
            fAutoClimbRadius = iniReader.ReadFloat("MISC", "AutoClimbLaddersRange", 1.5f);

            auto onFootInput = find_pattern("76 ? 8A 81 ? ? ? ? 32 C2 3C ? 76 ? F7 87 ? ? ? ? ? ? ? ? 74", "76 ? 8A 96 ? ? ? ? 32 D0 80 FA ? 76 ? F7 83 ? ? ? ? ? ? ? ? 74 ? 8B 44 24");
            auto onFootScan = find_pattern("E8 ? ? ? ? 83 C4 ? 89 44 24 ? F6 87", "E8 ? ? ? ? 83 C4 ? 89 44 24 ? EB ? 8B 74 24");
            auto waterInput = find_pattern("80 F9 ? 0F 86 ? ? ? ? 8B 4E", "3C ? 0F 86 ? ? ? ? 8B 4B");
            auto waterScan = find_pattern("E8 ? ? ? ? 83 C4 ? 85 C0 74 ? 8B 87 ? ? ? ? 81 8F", "E8 ? ? ? ? 83 C4 ? 85 C0 74 ? 81 8E");
            auto scanRadius = find_pattern("F3 0F 10 05 ? ? ? ? F3 0F 51 CC", "F3 0F 10 25 ? ? ? ? 0F 28 C1 0F 28 D3");
            auto ladderCleanup = find_pattern("56 57 8B 7C 24 ? 57 8B F1 E8 ? ? ? ? 57 8B CE E8 ? ? ? ? 8B 8F", "56 8B 74 24 ? 57 56 8B F9 E8 ? ? ? ? 56 8B CF E8 ? ? ? ? 8B 8E");
            if (onFootInput.size() != 1 || onFootScan.size() != 1 || waterInput.size() != 1 || waterScan.size() != 1 || scanRadius.size() != 1 || ladderCleanup.size() != 1)
                return;

            auto sections = hook::pattern("FF B6 ? ? ? ? 51 E8 ? ? ? ? 83 C4 14 84 C0 0F 84");
            auto ladderProcess = hook::pattern("55 8B EC 83 E4 F0 83 EC 68 56 8B 75 08 57 8B 46 38");
            auto positions = hook::pattern("55 8B EC 83 E4 F0 81 EC ? ? ? ? 56 57 8B 7D 0C FF 75 10");
            auto leftRight = hook::pattern("56 57 8B F9 8D B7 58 27 00 00 56 E8");
            auto upDown = hook::pattern("56 57 8B F9 8D B7 78 27 00 00 56 E8");
            auto cameraHeading = hook::pattern("F3 0F 58 05 ? ? ? ? F3 0F 11 04 24 E8 ? ? ? ? D9 5C 24 0C");
            if (ladderProcess.size() != 1 || sections.size() != 1 || positions.size() != 1 || leftRight.size() != 1 || upDown.size() != 1 || cameraHeading.size() != 1)
                return;

            CTaskComplexClimbLadder::GetPedWalkLeftRight = reinterpret_cast<decltype(CTaskComplexClimbLadder::GetPedWalkLeftRight)>(leftRight.get_first(0));
            CTaskComplexClimbLadder::GetPedWalkUpDown = reinterpret_cast<decltype(CTaskComplexClimbLadder::GetPedWalkUpDown)>(upDown.get_first(0));
            CTaskComplexClimbLadder::GetAxisValue = reinterpret_cast<decltype(CTaskComplexClimbLadder::GetAxisValue)>(injector::GetBranchDestination(leftRight.get_first(11)).as_int());
            CTaskComplexClimbLadder::controlCameraHeading = *cameraHeading.get_first<const float*>(4);

            CTaskComplexClimbLadder::FindTopAndBottomOfAllLadderSections = reinterpret_cast<decltype(CTaskComplexClimbLadder::FindTopAndBottomOfAllLadderSections)>(injector::GetBranchDestination(sections.get_first(7)).as_int());
            CTaskComplexClimbLadder::CalculatePositionsForThisLadder = reinterpret_cast<decltype(CTaskComplexClimbLadder::CalculatePositionsForThisLadder)>(positions.get_first(0));

            auto cameraGate = hook::pattern("0F 2F C1 77 ? F6 81 ? ? ? ? 04");
            auto mount = hook::pattern("55 8B EC 83 E4 F0 83 EC 18 56 57 8B 7D 08 8B F1 81 FF 87 03 00 00");
            auto activeTask = hook::pattern("53 55 56 33 F6 57 8B E9 33 C0 8D 9B 00 00 00 00");
            auto lookX = hook::pattern("57 8D BE 98 27 00 00 57 E8 ? ? ? ?");
            auto lookY = hook::pattern("57 8D BE B8 27 00 00 57 E8 ? ? ? ?");
            if (cameraGate.size() != 1 || mount.size() != 1
                || activeTask.size() != 1 || lookX.size() != 1 || lookY.size() != 1)
                return;

            // Validate the function entries before installing detours.
            const auto entry = cameraGate.get_first<uint8_t>(-0x3F);
            const uint8_t prologue[]{ 0x55, 0x8B, 0xEC, 0x83, 0xE4, 0xF0, 0x83, 0xEC, 0x28 };
            const auto xEntry = lookX.get_first<uint8_t>(-0x14);
            const auto yEntry = lookY.get_first<uint8_t>(-0x14);
            const uint8_t getterPrologue[]{ 0x00, 0x56, 0x8B, 0xF1, 0x74, 0x08, 0x8D, 0x86 };
            const auto axis = injector::GetBranchDestination(lookX.get_first(8)).as_int();
            const bool gettersValid = xEntry[0] == 0x83 && xEntry[1] == 0x3D
                && yEntry[0] == 0x83 && yEntry[1] == 0x3D
                && std::memcmp(xEntry + 6, getterPrologue, sizeof(getterPrologue)) == 0
                && std::memcmp(yEntry + 6, getterPrologue, sizeof(getterPrologue)) == 0
                && axis == injector::GetBranchDestination(lookY.get_first(8)).as_int();
            if (std::memcmp(entry, prologue, sizeof(prologue)) != 0 || !gettersValid)
                return;

            LadderCamera::FindActiveTaskByType = reinterpret_cast<decltype(LadderCamera::FindActiveTaskByType)>(activeTask.get_first(0));
            LadderCamera::GetPedAimWeaponLeftRight = reinterpret_cast<decltype(LadderCamera::GetPedAimWeaponLeftRight)>(xEntry);
            LadderCamera::GetPedLookUpDown = reinterpret_cast<decltype(LadderCamera::GetPedLookUpDown)>(yEntry);
            LadderCamera::GetAxisValue = reinterpret_cast<decltype(LadderCamera::GetAxisValue)>(axis);
            LadderCamera::shTargetBetaCalc = safetyhook::create_inline(entry, LadderCamera::TargetBetaCalc);
            if (LadderCamera::shTargetBetaCalc)
                CTaskComplexClimbLadder::shCreateSubTask = safetyhook::create_inline(mount.get_first(0), CTaskComplexClimbLadder::CreateSubTask);
            if (!LadderCamera::shTargetBetaCalc || !CTaskComplexClimbLadder::shCreateSubTask)
            {
                CTaskComplexClimbLadder::shCreateSubTask.reset();
                LadderCamera::shTargetBetaCalc.reset();
                return;
            }
            ShortLadderTraversal::shProcessPed = safetyhook::create_inline(ladderProcess.get_first(0), ShortLadderTraversal::ProcessPed);
            if (!ShortLadderTraversal::shProcessPed)
            {
                CTaskComplexClimbLadder::shCreateSubTask.reset();
                LadderCamera::shTargetBetaCalc.reset();
                return;
            }

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
        };
    }
} AutoLadder;
