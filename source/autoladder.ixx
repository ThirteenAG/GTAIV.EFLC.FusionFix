module;

#include <common.hxx>
#include <cmath>
#include <utility>
#include <algorithm>
#include <array>

export module autoladder;

import common;
import comvars;
import natives;
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

namespace CTaskComplexClimbLadder::RearApproach
{
    static bool IsActiveForPed(uintptr_t ped);
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

static bool IsAutoClimbEnabled()
{
    static auto AutoClimbLadders = FusionFixSettings.GetRef("PREF_AUTOCLIMBLADDERS");
    return AutoClimbLadders->get();
}

namespace LadderCamera
{
    // Camera options
    constexpr bool UseMenuSetting = true; // Expose menu options and load saved CFG values
    constexpr bool UseIniSetting = true; // Read optional INI defaults before CFG values
    constexpr int32_t DefaultLadderCameraTilt = 1; // Enable native downward tilt
    constexpr int32_t DefaultAlternateLadderCamera = 0; // Enable the alternate height-follow camera
    constexpr auto TiltPreferenceName = "PREF_LADDERCAMERATILT";
    constexpr auto AlternatePreferenceName = "PREF_ALTERNATELADDERCAMERA";
    static int32_t tiltPreference = -1;
    static int32_t alternatePreference = -1;
    static bool ladderCameraTilt = DefaultLadderCameraTilt != 0;
    static bool alternateLadderCamera = DefaultAlternateLadderCamera != 0;

    static void ReadIniSettings()
    {
        ladderCameraTilt = DefaultLadderCameraTilt != 0;
        alternateLadderCamera = DefaultAlternateLadderCamera != 0;
        if constexpr (UseIniSetting)
        {
            CIniReader iniReader("");
            ladderCameraTilt = std::clamp(iniReader.ReadInteger("MISC", "LadderCameraTilt", DefaultLadderCameraTilt), 0, 1) != 0;
            alternateLadderCamera = std::clamp(iniReader.ReadInteger("MISC", "AlternateLadderCamera", DefaultAlternateLadderCamera), 0, 1) != 0;
        }
    }

    static bool RegisterMenuPreference()
    {
        if constexpr (UseMenuSetting)
        {
            tiltPreference = FusionFixSettings.RegisterPreference(TiltPreferenceName, 1,
                static_cast<int32_t>(ladderCameraTilt), "MISC", "LadderCameraTilt",
                [](int32_t value) { ladderCameraTilt = value != 0; });
            alternatePreference = FusionFixSettings.RegisterPreference(AlternatePreferenceName, 1,
                static_cast<int32_t>(alternateLadderCamera), "MISC", "AlternateLadderCamera",
                [](int32_t value) { alternateLadderCamera = value != 0; });
            return tiltPreference >= 0 && alternatePreference >= 0;
        }
        else
            return true;
    }

    static void ApplyMenuSetting()
    {
        if constexpr (UseMenuSetting)
        {
            FusionFixSettings.Set(tiltPreference, FusionFixSettings.Get(tiltPreference));
            FusionFixSettings.Set(alternatePreference, FusionFixSettings.Get(alternatePreference));
            for (const auto screen : { CSettings::MenuScreen::Controls, CSettings::MenuScreen::TitleControls })
            {
                FusionFixSettings.AddToggle(screen, "FF_LADDERTILT", TiltPreferenceName, "Action Cam");
                FusionFixSettings.AddToggle(screen, "FF_ALTLADDERCAM", AlternatePreferenceName, "FF_LADDERTILT");
            }
        }
    }

    // Native camera bindings
    static SafetyHookInline shTargetBetaCalc;
    static SafetyHookMid shBeginUpdate;
    static SafetyHookMid shOrbitDistance;
    static SafetyHookMid shDistanceHistory;
    static const float* viewDistanceOffsets = nullptr;
    static SafetyHookMid shOrbitHeading;
    static SafetyHookInline shLadderPitch;
    static SafetyHookMid shVerticalFollow;
    static SafetyHookMid shMountPitch;
    static SafetyHookMid shPitchLevel;
    static SafetyHookMid shBeforeCollision;
    static SafetyHookMid shAfterCollision;
    static SafetyHookInline shPostSmoothTarget;
    static rage::Vector4* (__cdecl* GetMoverTrackDisplacement)(rage::Vector4*, float, int, int);
    static uintptr_t (__thiscall* FindActiveTaskByType)(uintptr_t, int);
    static uintptr_t (__thiscall* GetPedAimWeaponLeftRight)(uintptr_t);
    static uintptr_t (__thiscall* GetPedLookUpDown)(uintptr_t);
    static int (__cdecl* GetAxisValue)(uintptr_t);
    static uintptr_t (__stdcall* GetRootBone)(uintptr_t, rage::Vector4*);
    static uintptr_t cameraPed = 0;
    static bool IsAlternateEnabled() { return alternateLadderCamera; }

    // Camera tuning
    static constexpr float pi = 3.14159265358979323846f;
    static constexpr float fMinOrbitHeading = -90.0f * pi / 180.0f; // Lower ladder-relative yaw limit
    static constexpr float fMaxOrbitHeading = 80.0f * pi / 180.0f; // Upper ladder-relative yaw limit
    static constexpr float fDesiredPullAroundHeadingOffset = 75.0f * pi / 180.0f; // Side-view yaw target
    static constexpr float fOrbitPitchLimit = 75.0f * pi / 180.0f; // Maximum up/down viewing angle
    static constexpr float fMinOrbitDistanceRatioForCustomPullAround = 0.85f; // Pull rearward below this collision distance ratio
    static constexpr float fRearEntryCutDistanceRatio = 0.5f; // Rear-entry camera cut threshold
    static constexpr float fLimitBlendTime = 0.75f; // Orbit-limit and rear-entry blend time (s)
    static constexpr float fHeadingPullAroundMaxMoveSpeed = 1.0f; // Speed for full yaw pull (m/s)
    static constexpr float fHeadingPullAroundSpringConstant = 4.0f; // Yaw spring strength
    static constexpr float fHeadingPullAroundSpringDampingRatio = 0.95f; // Yaw spring damping
    static constexpr float fPitchLevelSmoothRate = 0.1f; // Pitch error removed per 30 Hz step
    static constexpr float fPitchLevelTolerance = 1.0f * pi / 180.0f; // Pitch leveling completion tolerance

    static constexpr float fLadderScreenRatioForFootRoom = -0.02f; // Foot-room screen offset from center
    static constexpr float fFootRoomPivotHeightRatio = 0.85f; // Framing pivot as a fraction of model height
    static constexpr float fFootRoomAttackTime = 0.75f; // Framing blend-in time (s)
    static constexpr float fFootRoomReleaseTime = 1.0f; // Framing blend-out time (s)
    static constexpr float fMaxFootRoomScale = 2.0f; // Maximum native distance multiplier

    // Camera state and input blending
    struct LookInputEnvelope
    {
        enum class State { Off, Attack, Hold, Release };
        State state = State::Off;
        float elapsed = 0.0f;
        float level = 0.0f;
        float releaseLevel = 0.0f;

        void Update(bool input, bool controller, float seconds)
        {
            elapsed += seconds;
            if (input && (state == State::Off || state == State::Release))
            {
                state = State::Attack;
                elapsed = controller ? level * 0.5f : 0.0f;
            }
            else if (input && !controller && state == State::Hold)
                elapsed = 0.0f;
            else if (!input && controller && state != State::Off && state != State::Release)
            {
                state = State::Release;
                releaseLevel = level;
                elapsed = 0.0f;
            }
            if (state == State::Attack)
            {
                level = controller ? std::clamp(elapsed / 0.5f, 0.0f, 1.0f) : 1.0f;
                if (level == 1.0f)
                {
                    state = State::Hold;
                    elapsed = 0.0f;
                }
            }
            else if (state == State::Hold && elapsed >= 1.0f)
            {
                state = State::Release;
                releaseLevel = level;
                elapsed = 0.0f;
            }
            if (state == State::Release)
            {
                level = elapsed < 6.0f ? releaseLevel * std::exp(-7.0f * elapsed / 6.0f) : 0.0f;
                if (elapsed >= 6.0f)
                    state = State::Off;
            }
        }
    };

    enum class Phase { None, Mount, Climb, Dismount, Approach };
    enum class YawOwner { NativeFollow, LadderAlignment, RearApproach, ObstructionRecovery };
    struct LadderAlignment
    {
        uintptr_t camera = 0;
        uintptr_t ped = 0;
        uintptr_t task = 0;
        float heading = 0.0f;
        bool seenClimbTask = false;
        bool approaching = false;
        bool rearEntryCut = false;
        rage::Vector4 rearEntryTarget{};
        bool rearOrbitChecked = false;
        bool rearOrbitActive = false;
        float rearOrbitStart = 0.0f;
        float rearOrbitDelta = 0.0f;
        float rearOrbitTime = 0.0f;
        rage::Vector4 previousPosition{};
        bool hasPreviousPosition = false;
        float headingVelocity = 0.0f;
        float level = 0.0f;
        float orbitLevel = 0.0f;
        bool exitAtTop = false;
        float orbitDistanceRatio = 1.0f;
        bool collisionSampleValid = false;
        bool inhibitCustomPullAround = false;
        bool inhibitPullAround = false;
        bool pitchLevelRequested = false;
        bool pitchLevelActive = false;
        bool topPitchHandoff = false;
        float pitchHandoffTime = 0.0f;
        float pitchHandoffQuietTime = 0.0f;
        ULONGLONG lastInputTime = 0;
        LookInputEnvelope lookEnvelope;
    };
    static thread_local LadderAlignment alignment;
    struct CameraUpdate
    {
        uintptr_t camera = 0;
        Phase phase = Phase::None;
        YawOwner yawOwner = YawOwner::NativeFollow;
        int action = -1;
        int lookX = 0;
        int lookY = 0;
        float desiredDistance = 0.0f;
        float previousOrbitDistanceRatio = 1.0f;
        bool collisionConstrained = false;
        bool collisionSampleValid = false;
        bool collisionRecovery = false;
        bool canAlign = false;
        float seconds = 0.0f;
        float moveSpeed = 0.0f;
        bool topDismount = false;
        bool onLadder = false;
        bool approaching = false;
        bool controller = true;
        float blend = 0.0f;
        float orbitBlend = 0.0f;
        float pullWeight = 0.0f;
        float targetZ = 0.0f;
        float targetDeltaZ = 0.0f;
        bool verticalSample = false;
        bool verticalCarried = false;
        bool mountPitchPreserved = false;
        float previousPitch = 0.0f;
        float neutralPitch = 0.0f;
        bool pitchReferenceValid = false;
        bool finishPitchHandoff = false;
    };
    static thread_local CameraUpdate update;

    struct VerticalFollowState
    {
        uintptr_t camera = 0;
        uintptr_t ped = 0;
        uintptr_t task = 0;
        float targetZ = 0.0f;
        float baseZ = 0.0f;
        float distanceBaseZ = 0.0f;
        ULONGLONG sampleTime = 0;
    };
    static thread_local VerticalFollowState verticalFollow;

    struct MovementSample
    {
        uintptr_t camera = 0, ped = 0;
        bool moving = false;
    };
    static thread_local MovementSample movementSample;

    struct TopExitTarget
    {
        uintptr_t camera = 0, ped = 0, task = 0;
        ULONGLONG lastSample = 0, exitTime = 0;
        float phase = 0.0f;
    };
    static thread_local TopExitTarget topExitTarget;

    struct LadderHeightReference
    {
        uintptr_t camera = 0, ped = 0, task = 0;
        ULONGLONG lastSample = 0;
        float animatedWeight = 0.0f;

        float Update(uintptr_t currentCamera, uintptr_t currentPed, uintptr_t currentTask,
            ULONGLONG now, bool useNativeHeight)
        {
            const float desiredWeight = useNativeHeight ? 0.0f : 1.0f;
            if (camera != currentCamera || ped != currentPed || task != currentTask || now - lastSample >= 500)
                animatedWeight = desiredWeight;
            else
            {
                const float step = static_cast<float>(now - lastSample) / 350.0f;
                animatedWeight += std::clamp(desiredWeight - animatedWeight, -step, step);
            }
            camera = currentCamera;
            ped = currentPed;
            task = currentTask;
            lastSample = now;
            return animatedWeight * animatedWeight * (3.0f - 2.0f * animatedWeight);
        }
    };
    static thread_local LadderHeightReference ladderHeightReference;

    struct FootRoomState
    {
        uintptr_t camera = 0, ped = 0, task = 0;
        Vector3 boundsMin{}, boundsMax{};
        float level = 0.0f;
        float framingScale = 1.0f;
        ULONGLONG sampleTime = 0;
        bool boundsValid = false;
    };
    static thread_local FootRoomState footRoom;
    struct FootRoomUpdate
    {
        uintptr_t camera = 0;
        float scale = 1.0f;
    };
    static thread_local FootRoomUpdate footRoomUpdate;

    struct MountPitchState
    {
        uintptr_t camera = 0;
        uintptr_t ped = 0;
        uintptr_t task = 0;
        float target = 0.0f;
        ULONGLONG sampleTime = 0;
    };
    static thread_local MountPitchState mountPitchState;

    // Top-mount visibility repair
    namespace TopMountBounds
    {
        static void (__thiscall* SetInstanceMatrix)(uintptr_t, const void*);
        static void (__thiscall* UpdateSkeleton)(uintptr_t);
        static void (__thiscall* CalculateCompositeExtents)(uintptr_t, bool);
        static void (__thiscall* UpdateObjectLocation)(uintptr_t, int, const void*);
        static void (__thiscall* UpdateObjectLocationAndRadius)(uintptr_t, int, float, const void*);
        static uintptr_t* physicsLevel = nullptr;
        static bool ready = false;

        static void Refresh(uintptr_t ped, uintptr_t instanceBefore, uintptr_t root, bool rebased)
        {
            if (!ready)
                return;
            if (!rebased || !root)
                return;
            const auto instance = *reinterpret_cast<const uintptr_t*>(ped + 1972);
            if (!instance || instance != instanceBefore)
                return;
            if (*reinterpret_cast<const int*>(ped + 1976) != 2 || !(*reinterpret_cast<const uint8_t*>(ped + 244) & 2))
                return;
            const auto* pedVtable = *reinterpret_cast<const uintptr_t**>(ped);
            const auto getInstance = reinterpret_cast<uintptr_t(__thiscall*)(uintptr_t)>(pedVtable[0xA0 / 4]);
            if (getInstance(ped) != instance)
                return;
            const auto* instanceVtable = *reinterpret_cast<const uintptr_t**>(instance);
            const auto getSkeleton = reinterpret_cast<uintptr_t(__thiscall*)(uintptr_t)>(instanceVtable[0xE0 / 4]);
            const auto matrix = *reinterpret_cast<const uintptr_t*>(ped + 32);
            const auto archetype = *reinterpret_cast<const uintptr_t*>(instance + 4);
            const auto bound = archetype ? *reinterpret_cast<const uintptr_t*>(archetype + 12) : 0;
            if (!matrix || !bound || !getSkeleton(instance)
                || !*reinterpret_cast<const uintptr_t*>(bound + 132) || !*reinterpret_cast<const uintptr_t*>(bound + 136)
                || !*reinterpret_cast<const uint16_t*>(bound + 146))
                return;
            const auto level = *physicsLevel;
            const auto index = *reinterpret_cast<const uint16_t*>(instance + 8);
            if (!level || index == 0xFFFF || index >= *reinterpret_cast<const uint16_t*>(level + 38))
                return;
            const auto entries = *reinterpret_cast<const uintptr_t*>(level + 112);
            if (!entries || (*reinterpret_cast<const uintptr_t*>(entries + index * 8 + 4) & ~uintptr_t{ 0xF }) != instance)
                return;

            const auto copyMatrixHistory = reinterpret_cast<void(__thiscall*)(uintptr_t, const void*)>(instanceVtable[0x10 / 4]);
            copyMatrixHistory(instance, reinterpret_cast<const void*>(instance + 16));
            SetInstanceMatrix(instance, reinterpret_cast<const void*>(matrix));
            copyMatrixHistory(instance, reinterpret_cast<const void*>(instance + 16));
            UpdateSkeleton(ped);
            const auto poseBounds = reinterpret_cast<void(__thiscall*)(uintptr_t)>(instanceVtable[0xBC / 4]);
            poseBounds(instance);
            poseBounds(instance);
            const float oldRadius = *reinterpret_cast<const float*>(bound + 8);
            CalculateCompositeExtents(bound, true);
            if (oldRadius == *reinterpret_cast<const float*>(bound + 8))
                UpdateObjectLocation(level, index, nullptr);
            else
                UpdateObjectLocationAndRadius(level, index, oldRadius, nullptr);
        }

        static void Resolve()
        {
            auto matrix = hook::pattern("FF 52 10 FF 77 20 8B 8F B4 07 00 00 E8 ? ? ? ?");
            auto skeleton = hook::pattern("FF 50 04 8B CE E8 ? ? ? ? 81 8E 9C 02 00 00");
            auto extents = hook::pattern("55 8B EC 83 E4 F8 83 EC 08 80 7D 08 00 56 8B D1");
            auto radius = hook::pattern("55 8B EC 83 E4 F8 83 EC 18 83 3D ? ? ? ? 00 56 8B D1");
            auto location = hook::pattern("8B 0D ? ? ? ? 50 E8 ? ? ? ? 81 A7 18 01 00 00");
            SetInstanceMatrix = (decltype(SetInstanceMatrix))injector::GetBranchDestination(matrix.get_first(12)).as_int();
            UpdateSkeleton = (decltype(UpdateSkeleton))injector::GetBranchDestination(skeleton.get_first(5)).as_int();
            CalculateCompositeExtents = (decltype(CalculateCompositeExtents))extents.get_first(0);
            UpdateObjectLocationAndRadius = (decltype(UpdateObjectLocationAndRadius))radius.get_first(0);
            UpdateObjectLocation = (decltype(UpdateObjectLocation))injector::GetBranchDestination(location.get_first(7)).as_int();
            physicsLevel = *location.get_first<uintptr_t*>(2);
            ready = true;
        }
    }

    namespace TopMountHandoff
    {
        static SafetyHookInline shOffsetPedPosition;
        static SafetyHookMid shLocalRoot;
        static uintptr_t preRenderAddress = 0;
        struct OffsetContext
        {
            uintptr_t ped = 0, root = 0;
        };
        static thread_local OffsetContext offsetContext;

        static void __fastcall OffsetPedPosition(uintptr_t task, void* edx, uintptr_t ped)
        {
            bool topMount = false;
            if (FindActiveTaskByType
                && CPlayer::getLocalPlayerPed && ped && ped == CPlayer::getLocalPlayerPed())
            {
                const auto intelligence = *reinterpret_cast<const uintptr_t*>(ped + 548);
                const auto complex = intelligence ? FindActiveTaskByType(intelligence + 68, std::to_underlying(TaskID::TaskComplexClimbLadder)) : 0;
                const auto* vtable = *reinterpret_cast<const uintptr_t**>(ped);
                if (complex && *reinterpret_cast<const int*>(complex + 20) == 4 && vtable[0xCC / 4] == preRenderAddress)
                    topMount = true;
            }
            const auto previous = offsetContext;
            offsetContext = topMount ? OffsetContext{ ped, 0 } : OffsetContext{};
            const auto instanceBefore = topMount ? *reinterpret_cast<const uintptr_t*>(ped + 1972) : 0;
            const bool pendingRebase = topMount && (*reinterpret_cast<const uint8_t*>(task + 25) & 1);
            shOffsetPedPosition.unsafe_fastcall<void>(task, edx, ped);
            if (topMount)
            {
                const bool rebased = pendingRebase && !(*reinterpret_cast<const uint8_t*>(task + 25) & 1);
                TopMountBounds::Refresh(ped, instanceBefore, offsetContext.root, rebased);
            }
            offsetContext = previous;
        }

        static void LocalRoot(SafetyHookContext& regs)
        {
            if (offsetContext.ped && offsetContext.ped == regs.edi)
                offsetContext.root = regs.eax;
        }

        static void Install()
        {
            auto offset = hook::pattern("55 8B EC 83 E4 F0 81 EC 78 01 00 00 56 57 8B F1");
            auto render = hook::pattern("55 8B EC 83 E4 F0 81 EC E4 00 00 00 53 56 57 8B F9 E8 ? ? ? ? 80 3D");
            auto root = hook::pattern("8B F0 8B 44 24 3C F6 40 19 02 0F 84");
            preRenderAddress = reinterpret_cast<uintptr_t>(render.get_first(0));
            TopMountBounds::Resolve();
            shLocalRoot = safetyhook::create_mid(root.get_first(0), LocalRoot);
            shOffsetPedPosition = safetyhook::create_inline(offset.get_first(0), OffsetPedPosition);
        }
    }

    static float WrapHeading(float heading)
    {
        return std::remainder(heading, 2.0f * pi);
    }

    static float SlowInOut(float value)
    {
        return 0.5f * (1.0f - std::cos(std::clamp(value, 0.0f, 1.0f) * pi));
    }

    // Rear-entry camera obstruction checks
    namespace OrbitProbe
    {
        struct alignas(16) Segment { rage::Vector4 start{}, end{}; };
        struct alignas(16) Intersection { std::array<uint32_t, 24> words{}; };
        static_assert(sizeof(Segment) == 32 && sizeof(Intersection) == 96);
        static uintptr_t* worldProbe = nullptr;
        static int (__thiscall* TestLine)(uintptr_t, const Segment*, Intersection*, uintptr_t, int, int, int, int, int);
        static void (__cdecl* FilterIntersections)(Intersection*, int*);

        static void Resolve()
        {
            auto query = hook::pattern("6A 07 6A FF 52 50 FF 75 14 8D 44 24 1C 50 E8");
            auto filter = hook::pattern("50 8D 44 24 60 50 E8 ? ? ? ? 33 F6 83 C4 24");
            worldProbe = *query.get_first<uintptr_t*>(-9);
            TestLine = (decltype(TestLine))injector::GetBranchDestination(query.get_first(14)).as_int();
            FilterIntersections = (decltype(FilterIntersections))injector::GetBranchDestination(filter.get_first(6)).as_int();
        }

        static int IsBlocked(uintptr_t manager, uintptr_t ignore, int mask,
            const rage::Vector4& start, const rage::Vector4& end)
        {
            Segment line{ start, end };
            std::array<Intersection, 64> results{};
            for (auto& result : results)
                result.words[19] = 0xFFFF;
            int count = TestLine(manager, &line, results.data(), ignore, mask, 4096, 7, 64, 0);
            if (count < 0 || count > 64)
                return -1;
            const bool full = count == 64;
            FilterIntersections(results.data(), &count);
            if (count < 0 || count > 64)
                return -1;
            return count != 0 || full;
        }

        static int ScoreArc(uintptr_t manager, uintptr_t ignore, int mask, const rage::Vector4& pivot,
            const rage::Vector4& destination, const rage::Vector4& position, float delta)
        {
            constexpr int samples = 6;
            const float x = position.x - pivot.x, y = position.y - pivot.y;
            auto previous = position;
            int blocked = 0;
            for (int i = 1; i <= samples; ++i)
            {
                const float angle = delta * static_cast<float>(i) / samples;
                const float sine = std::sin(angle), cosine = std::cos(angle);
                auto candidate = position;
                candidate.x = pivot.x + x * cosine - y * sine;
                candidate.y = pivot.y + x * sine + y * cosine;
                auto atDestination = candidate;
                atDestination.x += destination.x - pivot.x;
                atDestination.y += destination.y - pivot.y;
                const int sight = IsBlocked(manager, ignore, mask, pivot, candidate);
                const int futureSight = IsBlocked(manager, ignore, mask, destination, atDestination);
                const int sweep = IsBlocked(manager, ignore, mask, previous, candidate);
                if (sight < 0 || futureSight < 0 || sweep < 0)
                    return -1;
                blocked += sight + futureSight + sweep;
                previous = candidate;
            }
            return blocked;
        }

        static void SelectRearArc(uintptr_t camera)
        {
            if (!update.approaching || alignment.rearOrbitChecked || update.lookX || update.lookY)
                return;
            alignment.rearOrbitChecked = true;
            const float beta = *reinterpret_cast<const float*>(camera + 776);
            const float relative = WrapHeading(beta - alignment.heading);
            if (!std::isfinite(relative) || (relative >= fMinOrbitHeading && relative <= fMaxOrbitHeading))
                return;
            const auto collider = *reinterpret_cast<const uintptr_t*>(camera + 300);
            const auto matrix = *reinterpret_cast<const rage::Matrix44**>(alignment.ped + 0x20);
            if (!TestLine || !FilterIntersections || !worldProbe || !*worldProbe || !collider || !matrix
                || !(*reinterpret_cast<const uint8_t*>(camera + 316) & 0x10))
                return;
            const auto pivot = *reinterpret_cast<const rage::Vector4*>(camera + 80);
            const auto position = *reinterpret_cast<const rage::Vector4*>(camera + 64);
            const auto& target = *reinterpret_cast<const rage::Vector4*>(alignment.task + 64);
            auto destination = pivot;
            destination.x += target.x - matrix->pos.x;
            destination.y += target.y - matrix->pos.y;
            for (const auto* value : std::array<const rage::Vector4*, 3>{ &pivot, &position, &destination })
                if (!std::isfinite(value->x) || !std::isfinite(value->y) || !std::isfinite(value->z))
                    return;
            const float dx = position.x - pivot.x, dy = position.y - pivot.y;
            const float radiusSquared = dx * dx + dy * dy;
            if (!std::isfinite(radiusSquared) || radiusSquared < 0.25f || radiusSquared > 10000.0f)
                return;
            const auto flags = *reinterpret_cast<const uint8_t*>(collider + 5248);
            const int mask = ((flags & 1) ? 24 : 0) | ((flags & 2) ? 128 : 0) | ((flags & 4) ? 6 : 0);
            if (!mask)
                return;
            const auto getInstance = reinterpret_cast<uintptr_t (__thiscall*)(uintptr_t)>(
                (*reinterpret_cast<const uintptr_t**>(alignment.ped))[0xA0 / 4]);
            const auto ignore = getInstance(alignment.ped);
            if (!ignore)
                return;
            const float negativeDelta = fMinOrbitHeading - relative + (relative > fMaxOrbitHeading ? 2.0f * pi : 0.0f);
            const float positiveDelta = fMaxOrbitHeading - relative - (relative < fMinOrbitHeading ? 2.0f * pi : 0.0f);
            const int negative = ScoreArc(*worldProbe, ignore, mask, pivot, destination, position, negativeDelta);
            const int positive = ScoreArc(*worldProbe, ignore, mask, pivot, destination, position, positiveDelta);
            const bool preferNegative = std::abs(negativeDelta) <= std::abs(positiveDelta);
            const int preferred = preferNegative ? negative : positive;
            const int alternate = preferNegative ? positive : negative;
            const bool chooseAlternate = preferred >= 0 && alternate >= 0 && alternate + 2 <= preferred;
            if (chooseAlternate)
            {
                alignment.rearOrbitStart = beta;
                alignment.rearOrbitDelta = preferNegative ? positiveDelta : negativeDelta;
                alignment.rearOrbitTime = 0.0f;
                alignment.rearOrbitActive = true;
                alignment.headingVelocity = 0.0f;
            }
        }
    }

    // Camera input and orbit limits
    static void ReadLookInput(uintptr_t control)
    {
        update.lookX = GetAxisValue(GetPedAimWeaponLeftRight(control));
        update.lookY = GetAxisValue(GetPedLookUpDown(control));
        if (IsAlternateEnabled())
        {
            update.controller = Natives::IsUsingController();
            if (!update.controller)
            {
                int32_t x = 0, y = 0;
                Natives::GetMouseInput(&x, &y);
                if (x) update.lookX = x;
                if (y) update.lookY = y;
            }
        }
    }

    static void UpdateOrbitLimits()
    {
        alignment.level = update.onLadder ? std::min(1.0f, alignment.level + update.seconds / fLimitBlendTime) : 0.0f;
        update.blend = SlowInOut(alignment.level);
        alignment.orbitLevel = update.onLadder || update.approaching
            ? std::min(1.0f, alignment.orbitLevel + update.seconds / fLimitBlendTime) : 0.0f;
        update.orbitBlend = SlowInOut(alignment.orbitLevel);
    }

    static void ClearObstructionHold()
    {
        alignment.inhibitCustomPullAround = false;
        alignment.inhibitPullAround = false;
    }

    static void UpdateAlignment()
    {
        if (update.onLadder || update.approaching)
        {
            update.previousOrbitDistanceRatio = alignment.orbitDistanceRatio;
            update.collisionSampleValid = alignment.collisionSampleValid;
            update.collisionConstrained = alignment.collisionSampleValid
                && alignment.orbitDistanceRatio < fMinOrbitDistanceRatioForCustomPullAround;
            if (IsAlternateEnabled() && update.phase == Phase::Dismount)
            {
                ClearObstructionHold();
            }
            else if (update.collisionConstrained)
            {
                alignment.inhibitCustomPullAround = true;
                alignment.inhibitPullAround = false;
            }
            else if (alignment.inhibitCustomPullAround)
            {
                alignment.inhibitPullAround = !update.lookX && !update.lookY;
                alignment.inhibitCustomPullAround = alignment.inhibitPullAround;
            }
            alignment.orbitDistanceRatio = 1.0f;
            alignment.collisionSampleValid = false;
        }
        else
            alignment.headingVelocity = 0.0f;
        UpdateOrbitLimits();
        const auto now = GetTickCount64();
        const float seconds = alignment.lastInputTime ? static_cast<float>(now - alignment.lastInputTime) * 0.001f : 0.0f;
        alignment.lastInputTime = now;
        alignment.lookEnvelope.Update(update.lookX || update.lookY, update.controller, seconds);
        update.pullWeight = 1.0f - SlowInOut(alignment.lookEnvelope.level);
    }

    // Automatic yaw steering
    static float PullAround(float error, float speedScale, float seconds, float blend)
    {
        const float target = error * speedScale;
        const float stiffness = 7.0f + (fHeadingPullAroundSpringConstant - 7.0f) * blend;
        const float damping = 2.5f + (fHeadingPullAroundSpringDampingRatio - 2.5f) * blend;
        const float decay = damping * std::sqrt(stiffness);
        const float frequencySquared = stiffness - decay * decay;
        float displacement;
        if (frequencySquared > 0.00001f)
        {
            const float frequency = std::sqrt(frequencySquared);
            const float sine = std::sin(frequency * seconds);
            const float cosine = std::cos(frequency * seconds);
            const float attenuation = std::exp(-decay * seconds);
            const float coefficient = (alignment.headingVelocity - decay * target) / frequency;
            displacement = attenuation * (-target * cosine + coefficient * sine);
            alignment.headingVelocity = -decay * displacement
                + attenuation * frequency * (coefficient * cosine + target * sine);
        }
        else
        {
            const float exponent = -decay + std::sqrt(std::max(-frequencySquared, 0.0f));
            displacement = -target * std::exp(exponent * seconds);
            alignment.headingVelocity = exponent * displacement;
        }
        return target + displacement;
    }

    // Ladder phase detection
    static bool IsFinalStep(uintptr_t task, int action)
    {
        if (!task || (action != 1 && action != 2 && action != 7))
            return false;
        const float step = *reinterpret_cast<const float*>(task + 168);
        const float startZ = *reinterpret_cast<const float*>(task + 164);
        const float bottomZ = *reinterpret_cast<const float*>(task + 72);
        const float topExitZ = *reinterpret_cast<const float*>(task + 152);
        if (!std::isfinite(step) || step <= 0.0f || !std::isfinite(startZ)
            || !std::isfinite(bottomZ) || !std::isfinite(topExitZ))
            return false;
        if (action == 1)
            return *reinterpret_cast<const uint8_t*>(task + 203) && startZ + step >= topExitZ - 0.75f;
        if (action == 2)
            return startZ - step <= bottomZ + 1.5f;
        const auto* matrix = *reinterpret_cast<const rage::Matrix44**>(alignment.ped + 0x20);
        return matrix && matrix->pos.z <= bottomZ + 1.5f + step;
    }

    static int GetEntryAlignmentTaskType(uintptr_t task)
    {
        auto child = *reinterpret_cast<const uintptr_t*>(task + 8);
        if (!child)
            return -1;
        const auto getType = reinterpret_cast<int (__thiscall*)(uintptr_t)>((*reinterpret_cast<const uintptr_t**>(child))[3]);
        const int type = getType(child);
        if (type != std::to_underlying(TaskID::TaskComplexControlMovement))
            return type;
        child = *reinterpret_cast<const uintptr_t*>(child + 0x14);
        if (!child)
            return -1;
        const auto getMovementType = reinterpret_cast<int (__thiscall*)(uintptr_t)>((*reinterpret_cast<const uintptr_t**>(child))[3]);
        return getMovementType(child);
    }

    // Begin camera update
    static void BeginUpdate(SafetyHookContext& regs)
    {
        update = {};
        static thread_local bool appliedTilt = DefaultLadderCameraTilt != 0;
        static thread_local bool appliedAlternate = DefaultAlternateLadderCamera != 0;
        if (appliedTilt != ladderCameraTilt || appliedAlternate != alternateLadderCamera)
        {
            alignment = {};
            mountPitchState = {};
            verticalFollow = {};
            topExitTarget = {};
            ladderHeightReference = {};
            footRoom = {};
            footRoomUpdate = {};
            movementSample = {};
            appliedTilt = ladderCameraTilt;
            appliedAlternate = alternateLadderCamera;
        }
        cameraPed = regs.edi;
        if (!cameraPed)
        {
            alignment = {};
            mountPitchState = {};
            return;
        }
        if (!IsAlternateEnabled())
        {
            alignment = {};
            mountPitchState = {};
            return;
        }
        if (!(regs.eflags & 0x40))
        {
            footRoom = {};
            alignment.hasPreviousPosition = false;
            alignment.headingVelocity = 0.0f;
            alignment.collisionSampleValid = false;
            alignment.rearOrbitActive = false;
            alignment.pitchLevelActive = false;
            alignment.lastInputTime = 0;

            mountPitchState = {};
            verticalFollow = {};
            return;
        }

        const auto intelligence = *reinterpret_cast<const uintptr_t*>(cameraPed + 0x224);
        const auto task = intelligence
            ? FindActiveTaskByType(intelligence + 68, std::to_underlying(TaskID::TaskComplexClimbLadder)) : 0;
        const bool canFinishPitch = alignment.camera == regs.esi
            && alignment.ped == cameraPed && alignment.pitchLevelActive;
        if (!task && !canFinishPitch)
        {
            alignment = {};
            return;
        }
        const auto control = CPlayerPed::GetControlFromPlayer ? CPlayerPed::GetControlFromPlayer(cameraPed) : 0;
        if (!control)
        {
            alignment = {};
            return;
        }
        if (!task)
        {
            ClearObstructionHold();
            alignment.collisionSampleValid = false;
            alignment.headingVelocity = 0.0f;
            update.camera = regs.esi;
            update.phase = Phase::Dismount;
            update.topDismount = alignment.exitAtTop;
            ReadLookInput(control);
            if (alignment.exitAtTop)
            {
                if (update.lookX || update.lookY || movementSample.camera != regs.esi
                    || movementSample.ped != cameraPed || movementSample.moving)
                {
                    alignment = {};
                    update.camera = 0;
                }
                else
                    alignment.topPitchHandoff = true;
            }
            return;
        }
        const float heading = *reinterpret_cast<const float*>(task + 112);
        if (!std::isfinite(heading))
        {
            alignment = {};
            return;
        }
        const auto simple = FindActiveTaskByType(intelligence + 68, std::to_underlying(TaskID::TaskSimpleClimbLadder));
        const int action = simple ? *reinterpret_cast<const int*>(simple + CTaskSimpleClimbLadder::nActionOffset) : -1;
        bool approaching = false;
        if (!simple && (alignment.task != task || !alignment.seenClimbTask)
            && !FindActiveTaskByType(intelligence + 68, std::to_underlying(TaskID::TaskSimpleRunNamedAnim)))
        {
            const bool continuing = alignment.task == task && alignment.ped == cameraPed
                && alignment.camera == regs.esi && alignment.approaching;
            const bool navigating = CTaskComplexClimbLadder::RearApproach::IsActiveForPed(cameraPed);
            const int entryTaskType = continuing && !navigating ? GetEntryAlignmentTaskType(task) : -1;
            const bool cancelled = (*reinterpret_cast<const uint32_t*>(task + 12) & 2) != 0;
            approaching = IsAutoClimbEnabled() && !cancelled && (navigating
                || (continuing && (entryTaskType == std::to_underlying(TaskID::TaskSimpleSlideToCoord)
                    || entryTaskType == std::to_underlying(TaskID::TaskSimpleMoveAchieveHeading))));
            if (!approaching)
            {
                alignment = {};
                return;
            }
        }
        if (alignment.task != task || alignment.ped != cameraPed || alignment.camera != regs.esi)
        {
            alignment = {};
            alignment.camera = regs.esi;
            alignment.ped = cameraPed;
            alignment.task = task;
        }
        alignment.heading = heading;
        if (alignment.approaching != approaching)
            alignment.hasPreviousPosition = false;
        alignment.approaching = approaching;
        update.camera = regs.esi;
        update.onLadder = !approaching;
        update.approaching = approaching;
        update.action = action;
        update.phase = approaching ? Phase::Approach : simple ? (action == 3 ? Phase::Dismount
            : (action == 4 || action == 5 || action == 6) ? Phase::Mount : Phase::Climb)
            : alignment.seenClimbTask ? Phase::Dismount : Phase::Mount;
        alignment.seenClimbTask |= simple != 0;
        if (update.phase == Phase::Climb && IsFinalStep(simple, action))
            update.phase = Phase::Dismount;
        if (update.phase == Phase::Dismount && simple)
            alignment.exitAtTop = action == 1 || action == 3;
        update.topDismount = update.phase == Phase::Dismount && alignment.exitAtTop;
        ReadLookInput(control);
        OrbitProbe::SelectRearArc(regs.esi);
    }

    static YawOwner SelectYawOwner()
    {
        if (!IsAlternateEnabled() || !update.canAlign || (!update.onLadder && !update.approaching))
            return YawOwner::NativeFollow;
        if (update.onLadder)
            return YawOwner::LadderAlignment;
        if (update.lookX || update.lookY)
            return YawOwner::NativeFollow;
        if (update.approaching && (alignment.rearOrbitActive || alignment.rearEntryCut))
            return YawOwner::RearApproach;
        if (update.collisionRecovery || (update.collisionSampleValid && alignment.inhibitCustomPullAround))
            return YawOwner::ObstructionRecovery;
        return YawOwner::NativeFollow;
    }

    // Apply yaw steering and limits
    static void OrbitHeading(SafetyHookContext& regs)
    {
        if (!IsAlternateEnabled() || update.camera != regs.esi || (!update.onLadder && !update.approaching) || !update.canAlign)
            return;
        auto& beta = *reinterpret_cast<float*>(regs.esi + 776);
        auto& velocity = *reinterpret_cast<float*>(regs.esi + 780);
        if (!std::isfinite(beta) || !std::isfinite(velocity))
            return;
        const bool customYaw = update.yawOwner != YawOwner::NativeFollow;
        const bool ladderYaw = update.yawOwner == YawOwner::LadderAlignment;
        const bool emergencyCut = customYaw && update.approaching && update.collisionRecovery
            && update.previousOrbitDistanceRatio < fRearEntryCutDistanceRatio;
        if (!alignment.rearEntryCut && emergencyCut)
        {
            const auto& target = *reinterpret_cast<const rage::Vector4*>(alignment.task + 64);
            if (std::isfinite(target.x) && std::isfinite(target.y) && std::isfinite(target.z))
            {
                alignment.rearEntryTarget = target;
                alignment.rearEntryCut = true;
                alignment.rearOrbitActive = false;
                beta = WrapHeading(alignment.heading);
                velocity = 0.0f;
                alignment.headingVelocity = 0.0f;
                return;
            }
        }
        if (update.yawOwner == YawOwner::RearApproach && alignment.rearOrbitActive)
        {
            if (update.moveSpeed > 0.0f)
                alignment.rearOrbitTime = std::min(fLimitBlendTime, alignment.rearOrbitTime + update.seconds);
            const float planned = WrapHeading(alignment.rearOrbitStart
                + alignment.rearOrbitDelta * SlowInOut(alignment.rearOrbitTime / fLimitBlendTime));
            beta = planned;
            velocity = 0.0f;
            alignment.headingVelocity = 0.0f;
            alignment.rearOrbitActive = alignment.rearOrbitTime < fLimitBlendTime;
            return;
        }
        const float blend = update.collisionRecovery ? update.orbitBlend : update.blend;
        float desiredHeadingOffset = 0.0f;
        const bool transition = update.phase == Phase::Mount || update.phase == Phase::Dismount;
        if (ladderYaw && !transition && !alignment.inhibitCustomPullAround)
            desiredHeadingOffset = WrapHeading(beta - alignment.heading) >= 0.0f
                ? fDesiredPullAroundHeadingOffset : -fDesiredPullAroundHeadingOffset;
        const bool shouldPull = customYaw && (update.collisionRecovery || (ladderYaw
            && (transition || !alignment.inhibitPullAround)))
            && (!update.collisionConstrained || (!update.lookX && !update.lookY));
        const float moveSpeed = update.moveSpeed;
        const bool evaluateLadderSpring = ladderYaw && update.seconds > 0.0f;
        if (blend > 0.0f && (evaluateLadderSpring || (shouldPull && moveSpeed > 0.0f && update.pullWeight > 0.0f)))
        {
            const float error = WrapHeading(alignment.heading + desiredHeadingOffset - beta);
            const float maxSpeed = 6.0f + (fHeadingPullAroundMaxMoveSpeed - 6.0f) * blend;
            const float speedScale = shouldPull ? SlowInOut(moveSpeed / maxSpeed) * update.pullWeight : 0.0f;
            const float headingDelta = PullAround(error, speedScale, update.seconds, blend) * blend;
            beta = WrapHeading(beta + headingDelta);
        }
        else
            alignment.headingVelocity = 0.0f;
        const float minHeading = -pi + (fMinOrbitHeading + pi) * update.orbitBlend;
        const float maxHeading = pi + (fMaxOrbitHeading - pi) * update.orbitBlend;
        const float relative = WrapHeading(beta - alignment.heading);
        float limited = relative;
        if (relative < minHeading || relative > maxHeading)
            limited = std::abs(WrapHeading(minHeading - relative)) <= std::abs(WrapHeading(maxHeading - relative))
                ? minHeading : maxHeading;
        if (limited != relative)
        {
            beta = WrapHeading(alignment.heading + limited);
            if ((limited == minHeading && velocity < 0.0f) || (limited == maxHeading && velocity > 0.0f))
                velocity = 0.0f;
            if ((limited == minHeading && alignment.headingVelocity < 0.0f)
                || (limited == maxHeading && alignment.headingVelocity > 0.0f))
                alignment.headingVelocity = 0.0f;
        }
    }

    static float OrbitDistance(uintptr_t camera)
    {
        const auto* position = reinterpret_cast<const float*>(camera + 64);
        const auto* pivot = reinterpret_cast<const float*>(camera + 80);
        const float x = position[0] - pivot[0], y = position[1] - pivot[1], z = position[2] - pivot[2];
        return std::sqrt(x * x + y * y + z * z);
    }

    // Native ladder tilt
    static uintptr_t __fastcall LadderPitch(uintptr_t camera, void* edx, uintptr_t ped, uintptr_t intelligence,
        float* vertical, float* horizontal)
    {
        const float input = *vertical;
        const auto result = shLadderPitch.unsafe_fastcall<uintptr_t>(camera, edx, ped, intelligence, vertical, horizontal);
        if (!ladderCameraTilt)
            *vertical = input;
        return result;
    }

    // Height tracking and mount/dismount transitions
    static void __fastcall PostSmoothTarget(uintptr_t camera, void* edx, uintptr_t ped, uintptr_t intelligence,
        rage::Vector4* target, rage::Vector4* root)
    {
        const auto now = GetTickCount64();
        const float inputRootZ = root->z, inputTargetZ = target->z;
        float destinationZ = inputRootZ, blend = 0.0f;
        bool applied = false, afterExit = false;
        const bool local = IsAlternateEnabled() && ped && intelligence && GetMoverTrackDisplacement && CPlayer::getLocalPlayerPed
            && ped == CPlayer::getLocalPlayerPed();
        const bool reset = (*reinterpret_cast<const uint8_t*>(camera + 908) & 0x88) != 0
            || (*reinterpret_cast<const uint8_t*>(camera + 909) & 0x1C) != 0;
        const auto task = local && intelligence
            ? FindActiveTaskByType(intelligence + 68, std::to_underlying(TaskID::TaskSimpleClimbLadder)) : 0;
        const int action = task ? *reinterpret_cast<const int*>(task + CTaskSimpleClimbLadder::nActionOffset) : -1;
        const auto complex = local ? FindActiveTaskByType(intelligence + 68,
            std::to_underlying(TaskID::TaskComplexClimbLadder)) : 0;
        float referenceRootZ = inputRootZ;
        float animatedRootZ = inputRootZ, animatedWeight = 0.0f;
        bool trackingRoot = false;
        if (complex && !reset && std::isfinite(inputRootZ) && std::isfinite(inputTargetZ)
            && GetRootBone && (task || FindActiveTaskByType(intelligence + 68,
            std::to_underlying(TaskID::TaskSimpleRunNamedAnim))))
        {
            alignas(16) rage::Vector4 animatedRoot{};
            GetRootBone(ped, &animatedRoot);
            if (std::isfinite(animatedRoot.z) && std::abs(animatedRoot.z - inputRootZ) <= 3.0f)
            {
                const bool useNativeHeight = action == CTaskSimpleClimbLadder::nIdleAction
                    || action == 1 || action == 2 || action == 4 || action == 5 || action == 6 || action == 7;
                animatedWeight = ladderHeightReference.Update(camera, ped, complex, now, useNativeHeight);
                animatedRootZ = animatedRoot.z;
                referenceRootZ += (animatedRootZ - inputRootZ) * animatedWeight;
                trackingRoot = true;
            }
        }
        if (!trackingRoot)
            ladderHeightReference = {};
        const bool continuing = topExitTarget.camera == camera && topExitTarget.ped == ped
            && now - topExitTarget.lastSample < 500;
        if (!local || reset || !std::isfinite(inputRootZ) || !std::isfinite(inputTargetZ))
            topExitTarget = {};
        else if (action == 3)
        {
            if (!continuing || topExitTarget.task != task)
                topExitTarget = { camera, ped, task };
            const auto player = *reinterpret_cast<const uintptr_t*>(task + 96);
            const bool finished = *reinterpret_cast<const uint8_t*>(task + 204) != 0;
            const float phase = finished ? 1.0f : player && *reinterpret_cast<const uint16_t*>(player + 68) == 1
                ? *reinterpret_cast<const float*>(player + 76) : -1.0f;
            destinationZ = *reinterpret_cast<const float*>(task + 152);
            if (!finished)
            {
                const auto* matrix = *reinterpret_cast<const rage::Matrix44**>(ped + 0x20);
                if (matrix)
                {
                    alignas(16) rage::Vector4 displacement{};
                    GetMoverTrackDisplacement(&displacement, 1.0f, 9, 134);
                    destinationZ = matrix->pos.z + matrix->right.z * displacement.x
                        + matrix->up.z * displacement.y + matrix->at.z * displacement.z;
                }
                else
                    destinationZ = NAN;
            }
            if (std::isfinite(phase) && phase >= 0.0f && phase <= 1.01f
                && std::isfinite(destinationZ) && std::abs(destinationZ - inputRootZ) <= 3.0f)
            {
                topExitTarget.phase = std::max(topExitTarget.phase, std::clamp(phase, 0.0f, 1.0f));
                blend = SlowInOut(topExitTarget.phase);
                topExitTarget.lastSample = now;
                applied = true;
            }
            else
                topExitTarget = {};
        }
        else if (continuing && !task && topExitTarget.phase >= 0.95f)
        {
            const auto* matrix = *reinterpret_cast<const rage::Matrix44**>(ped + 0x20);
            if (!topExitTarget.exitTime)
                topExitTarget.exitTime = now;
            const bool interrupted = movementSample.camera != camera || movementSample.ped != ped
                || movementSample.moving || update.lookX || update.lookY
                || FindActiveTaskByType(intelligence + 68, std::to_underlying(TaskID::TaskComplexClimbLadder));
            if (!interrupted && matrix && std::isfinite(matrix->pos.z) && now - topExitTarget.exitTime < 1500
                && std::abs(matrix->pos.z - inputRootZ) <= 0.5f)
            {
                destinationZ = matrix->pos.z;
                blend = 1.0f;
                topExitTarget.lastSample = now;
                applied = afterExit = true;
            }
            else
                topExitTarget = {};
        }
        else
            topExitTarget = {};
        if (applied || trackingRoot)
        {
            const float desiredRootZ = referenceRootZ + (destinationZ - referenceRootZ) * blend;
            const float delta = desiredRootZ - inputRootZ;
            root->z += delta;
            target->z += delta;
        }
        const float requestedRootZ = root->z, requestedTargetZ = target->z;
        shPostSmoothTarget.thiscall<void>(camera, ped, intelligence, target, root);
        if (!applied)
            return;
        root->z += (requestedRootZ - root->z) * blend;
        target->z += (requestedTargetZ - target->z) * blend;
        *reinterpret_cast<float*>(camera + 680) = target->z;
        *reinterpret_cast<float*>(camera + 696) = root->z;
        if (afterExit && std::abs(destinationZ - inputRootZ) < 0.0001f)
            topExitTarget = {};
    }

    static void VerticalFollow(SafetyHookContext& regs)
    {
        if (!IsAlternateEnabled() || update.camera != regs.esi
            || alignment.ped != regs.edi)
        {
            verticalFollow = {};
            return;
        }

        const float targetZ = regs.xmm0.f32[0];
        auto& baseZ = *reinterpret_cast<float*>(regs.esi + 568);
        auto& distanceBaseZ = *reinterpret_cast<float*>(regs.esi + 584);
        const bool reset = (*reinterpret_cast<const uint8_t*>(regs.esi + 908) & 8) != 0
            || (*reinterpret_cast<const uint8_t*>(regs.esi + 909) & 0x18) != 0;
        if (reset || !std::isfinite(targetZ) || !std::isfinite(baseZ) || !std::isfinite(distanceBaseZ))
        {
            verticalFollow = {};
            alignment.pitchLevelActive = false;
            alignment.topPitchHandoff = false;
            return;
        }
        update.targetZ = targetZ;
        update.neutralPitch = -*reinterpret_cast<const float*>(regs.esp + 0x50);
        update.previousPitch = *reinterpret_cast<const float*>(regs.esi + 772);
        update.pitchReferenceValid = std::isfinite(update.neutralPitch) && std::isfinite(update.previousPitch);
        if (!update.onLadder && !alignment.topPitchHandoff)
        {
            verticalFollow = {};
            return;
        }
        update.verticalSample = true;
        const bool continuous = verticalFollow.camera == regs.esi && verticalFollow.ped == regs.edi
            && verticalFollow.task == alignment.task && GetTickCount64() - verticalFollow.sampleTime < 500;
        if (!continuous)
            return;

        const float targetDelta = targetZ - verticalFollow.targetZ;
        const float nativeShift = baseZ - verticalFollow.baseZ;
        const float nativeDistanceShift = distanceBaseZ - verticalFollow.distanceBaseZ;
        update.targetDeltaZ = targetDelta;

        if (std::abs(targetDelta) > 2.0f || std::abs(nativeShift) > 2.0f
            || std::abs(nativeDistanceShift - nativeShift) > 0.01f)
        {
            if (alignment.topPitchHandoff)
            {
                alignment.pitchLevelActive = false;
                alignment.topPitchHandoff = false;
            }
            return;
        }

        baseZ += targetDelta - nativeShift;
        distanceBaseZ += targetDelta - nativeDistanceShift;
        update.verticalCarried = true;
    }

    // Pitch limits, mount preservation and dismount leveling
    static void ApplyLadderPitchLimits(SafetyHookContext& regs)
    {
        if (!IsAlternateEnabled() || update.camera != regs.esi
            || !update.onLadder || !update.canAlign || !update.pitchReferenceValid)
            return;
        auto& downLimit = *reinterpret_cast<float*>(regs.esp + 0x6C);
        auto& upLimit = *reinterpret_cast<float*>(regs.esp + 0x94);
        if (!std::isfinite(downLimit) || !std::isfinite(upLimit) || downLimit < 0.0f || upLimit < 0.0f)
            return;
        downLimit += (fOrbitPitchLimit - downLimit) * update.blend;
        upLimit += (fOrbitPitchLimit - upLimit) * update.blend;
    }

    static void MountPitch(SafetyHookContext& regs)
    {
        ApplyLadderPitchLimits(regs);
        if (!IsAlternateEnabled() || update.camera != regs.esi
            || !update.onLadder || update.phase != Phase::Mount || !update.canAlign || !update.pitchReferenceValid)
        {
            mountPitchState = {};
            return;
        }

        auto& target = *reinterpret_cast<float*>(regs.esp + 0x24);
        const float baseAlpha = *reinterpret_cast<const float*>(regs.esp + 0x50);
        const bool continuous = mountPitchState.camera == regs.esi && mountPitchState.ped == alignment.ped
            && mountPitchState.task == alignment.task && GetTickCount64() - mountPitchState.sampleTime < 500;
        const float desired = (continuous ? mountPitchState.target : update.previousPitch) + baseAlpha;
        if (!std::isfinite(target) || !std::isfinite(desired))
        {
            mountPitchState = {};
            return;
        }

        update.mountPitchPreserved = true;
        target = desired;
    }

    static void PitchLevel(SafetyHookContext& regs)
    {
        mountPitchState = {};
        if (IsAlternateEnabled() && update.camera == regs.esi && update.mountPitchPreserved)
        {
            const bool directInput = !regs.ebx || *reinterpret_cast<const uint8_t*>(regs.ebx + 0x3289) != 0;
            const float target = directInput ? *reinterpret_cast<const float*>(regs.esi + 772)
                : *reinterpret_cast<const float*>(regs.esp + 0x1C);
            if (std::isfinite(target))
                mountPitchState = { regs.esi, alignment.ped, alignment.task, target, GetTickCount64() };
        }
        if (!IsAlternateEnabled() || update.camera != regs.esi)
        {
            alignment.pitchLevelActive = false;
            return;
        }
        if (update.onLadder && update.phase == Phase::Dismount && !alignment.pitchLevelRequested)
        {
            alignment.pitchLevelRequested = true;
            alignment.pitchLevelActive = true;
        }
        if (update.lookX || update.lookY || !update.canAlign || !update.pitchReferenceValid)
        {
            alignment.pitchLevelActive = false;
            return;
        }
        if (!alignment.pitchLevelActive)
            return;

        auto& pitch = *reinterpret_cast<float*>(regs.esi + 772);
        if (!std::isfinite(pitch) || !std::isfinite(update.seconds) || update.seconds <= 0.0f)
        {
            alignment.pitchLevelActive = false;
            return;
        }
        const float blend = 1.0f - std::pow(1.0f - fPitchLevelSmoothRate, 30.0f * update.seconds);
        float leveled = update.previousPitch + (update.neutralPitch - update.previousPitch) * blend;
        const bool reached = std::abs(update.neutralPitch - leveled) <= fPitchLevelTolerance;
        if (update.topDismount && (reached || alignment.topPitchHandoff))
            leveled = update.neutralPitch;

        pitch = leveled;
        *reinterpret_cast<float*>(regs.esi + 784) = 0.0f;
        *reinterpret_cast<float*>(regs.esp + 0x1C) = leveled;
        if (reached && !update.topDismount)
            alignment.pitchLevelActive = false;
        if (alignment.topPitchHandoff)
        {
            alignment.pitchHandoffTime += update.seconds;
            alignment.pitchHandoffQuietTime = update.verticalCarried && std::abs(update.targetDeltaZ) <= 0.01f * update.seconds
                ? alignment.pitchHandoffQuietTime + update.seconds : 0.0f;
            update.finishPitchHandoff = alignment.pitchHandoffQuietTime >= 0.15f || alignment.pitchHandoffTime >= 2.0f;
        }
    }

    // Screen-space framing and camera distance
    static float ComputeFootRoomDistance(float height, float radius, float pitch, float fov)
    {
        const float sine = std::sin(pitch), cosine = std::cos(pitch);
        const float baseDistance = radius * cosine + height * sine;
        const float projectedHeight = height * cosine - radius * sine;
        const float angle = std::min(fov * (3.141592654f / 180.0f) * (0.5f - fLadderScreenRatioForFootRoom), pi * 0.5f);
        return baseDistance + projectedHeight / std::tan(angle);
    }

    static float ComputeModelFootRoomDistance(const Vector3& lo, const Vector3& hi, float pitch, float fov, float& height, float& radius)
    {
        height = (hi.fZ - lo.fZ) * fFootRoomPivotHeightRatio;
        radius = std::max({ std::abs(lo.fX), std::abs(hi.fX), std::abs(lo.fY), std::abs(hi.fY) });
        return ComputeFootRoomDistance(height, radius, pitch, fov);
    }

    static float AdvanceFootRoomLevel(float level, bool onLadder, bool moving, float seconds)
    {
        return onLadder ? std::min(1.0f, level + seconds / fFootRoomAttackTime)
            : moving ? std::max(0.0f, level - seconds / fFootRoomReleaseTime) : level;
    }

    static float ComputeFootRoomScale(float framingDistance, float referenceDistance, float level)
    {
        if (!std::isfinite(framingDistance) || !std::isfinite(referenceDistance) || referenceDistance < 0.1f)
            return 1.0f;
        return 1.0f + (std::clamp(framingDistance / referenceDistance, 1.0f, fMaxFootRoomScale) - 1.0f) * SlowInOut(level);
    }

    static float ComputeFootRoomMinimum(float nativeMinimum, float requestedDistance, float level)
    {
        const float minimum = std::min(nativeMinimum, requestedDistance);
        return minimum + (requestedDistance - minimum) * SlowInOut(level);
    }

    static void OrbitDistanceRequest(SafetyHookContext& regs)
    {
        footRoomUpdate = {};
        const auto camera = regs.esi, ped = regs.edi;
        movementSample = { camera, ped, *reinterpret_cast<const uint8_t*>(regs.esp + 0x60) != 0 };
        const float timestep = *reinterpret_cast<const float*>(regs.esp + 0x2C);
        const auto flags = *reinterpret_cast<const uint8_t*>(camera + 908);
        const auto forceFlags = *reinterpret_cast<const uint8_t*>(camera + 909);
        if (!IsAlternateEnabled() || !ped || !regs.ebx || !viewDistanceOffsets
            || !CPlayer::getLocalPlayerPed || CPlayer::getLocalPlayerPed() != ped
            || (flags & 0x80) || (forceFlags & 4)
            || (*reinterpret_cast<const uint8_t*>(camera + 910) & 0x80)
            || !std::isfinite(timestep) || timestep < 0.0f || timestep > 12.5f)
        {
            footRoom = {};
            return;
        }
        const auto now = GetTickCount64();
        const auto intelligence = *reinterpret_cast<const uintptr_t*>(ped + 0x224);
        const auto task = intelligence
            ? FindActiveTaskByType(intelligence + 68, std::to_underlying(TaskID::TaskComplexClimbLadder)) : 0;
        const bool continuous = footRoom.camera == camera && footRoom.ped == ped && now - footRoom.sampleTime <= 500;
        const bool onLadder = task && (FindActiveTaskByType(intelligence + 68, std::to_underlying(TaskID::TaskSimpleClimbLadder))
            || FindActiveTaskByType(intelligence + 68, std::to_underlying(TaskID::TaskSimpleRunNamedAnim))
            || (continuous && footRoom.task == task));
        if (!continuous || (onLadder && footRoom.task != task) || (flags & 8) || (forceFlags & 0x18))
        {
            const float inheritedLevel = continuous && !(flags & 8) && !(forceFlags & 0x18) ? footRoom.level : 0.0f;
            footRoom = {};
            if (!onLadder)
                return;
            footRoom.camera = camera;
            footRoom.ped = ped;
            footRoom.task = task;
            footRoom.level = inheritedLevel;
            Ped handle = 0;
            uint32_t model = 0;
            Natives::GetPlayerChar(Natives::GetPlayerId(), &handle);
            if (handle)
            {
                Natives::GetCharModel(handle, &model);
                if (model)
                    Natives::GetModelDimensions(model, &footRoom.boundsMin, &footRoom.boundsMax);
            }
            const auto& lo = footRoom.boundsMin;
            const auto& hi = footRoom.boundsMax;
            footRoom.boundsValid = std::isfinite(lo.fX) && std::isfinite(lo.fY) && std::isfinite(lo.fZ)
                && std::isfinite(hi.fX) && std::isfinite(hi.fY) && std::isfinite(hi.fZ)
                && hi.fX > lo.fX && hi.fY > lo.fY && hi.fZ - lo.fZ > 0.5f && hi.fZ - lo.fZ < 3.0f;
        }
        footRoom.sampleTime = now;
        const bool moving = *reinterpret_cast<const uint8_t*>(regs.esp + 0x60) != 0;
        footRoom.level = AdvanceFootRoomLevel(footRoom.level, onLadder, moving, timestep * 0.02f);
        if (!footRoom.boundsValid || footRoom.level == 0.0f)
            return;
        const float distance = regs.xmm0.f32[0];
        static auto FieldOfView = FusionFixSettings.GetRef("PREF_CUSTOMFOV");
        const float baseFov = *reinterpret_cast<const float*>(camera + 96);
        const float fov = baseFov + FieldOfView->get() * 5.0f;
        const float pitch = -*reinterpret_cast<const float*>(regs.esp + 0x50);
        const float referenceDistance = *reinterpret_cast<const float*>(regs.esp + 0x20)
            + *reinterpret_cast<const float*>(camera + 812) * viewDistanceOffsets[2];
        if (!std::isfinite(distance) || distance < 0.1f || !std::isfinite(fov) || fov < 10.0f || fov > 170.0f
            || !std::isfinite(pitch) || std::abs(pitch) > pi * 0.5f)
            return;
        float height, radius;
        const float framingDistance = ComputeModelFootRoomDistance(footRoom.boundsMin, footRoom.boundsMax, pitch, fov, height, radius);
        if (!std::isfinite(height) || !std::isfinite(radius) || height < 0.1f || height > 3.0f || radius > 2.0f)
            return;
        if (onLadder)
            footRoom.framingScale = ComputeFootRoomScale(framingDistance, referenceDistance, 1.0f);
        const float scale = 1.0f + (footRoom.framingScale - 1.0f) * SlowInOut(footRoom.level);
        footRoomUpdate = { camera, scale };
        auto& historyDistance = *reinterpret_cast<float*>(regs.esp + 0x68);
        historyDistance = std::max(historyDistance, distance) * scale;
    }

    static void DistanceHistory(SafetyHookContext& regs)
    {
        if (footRoomUpdate.camera != regs.esi)
            return;
        auto& distance = **reinterpret_cast<float**>(regs.esp + 4);
        auto& threshold = *reinterpret_cast<float*>(regs.esp + 8);
        auto& minimum = *reinterpret_cast<float*>(regs.esp + 12);
        distance *= footRoomUpdate.scale;
        threshold *= footRoomUpdate.scale;
        minimum = ComputeFootRoomMinimum(minimum * footRoomUpdate.scale, distance, footRoom.level);
    }

    // Native collision handoff
    static void BeforeCollision(SafetyHookContext& regs)
    {
        if (IsAlternateEnabled() && update.camera == regs.esi && update.canAlign && alignment.rearEntryCut
            && (update.approaching || update.phase == Phase::Mount) && regs.ebx == regs.esi + 64 && regs.eax)
        {
            auto* collisionPivot = reinterpret_cast<float*>(regs.eax);
            auto* cameraPosition = reinterpret_cast<float*>(regs.ebx);
            auto* pivot = reinterpret_cast<float*>(regs.esi + 80);
            const float x = alignment.rearEntryTarget.x - collisionPivot[0];
            const float y = alignment.rearEntryTarget.y - collisionPivot[1];
            if (std::isfinite(x) && std::isfinite(y) && std::isfinite(cameraPosition[0])
                && std::isfinite(cameraPosition[1]) && std::isfinite(pivot[0]) && std::isfinite(pivot[1]))
            {
                cameraPosition[0] += x;
                cameraPosition[1] += y;
                pivot[0] += x;
                pivot[1] += y;
                collisionPivot[0] += x;
                collisionPivot[1] += y;
            }
        }
        verticalFollow = {};
        if (IsAlternateEnabled() && update.camera == regs.esi && (update.onLadder || update.approaching))
            update.desiredDistance = OrbitDistance(regs.esi);
        if (IsAlternateEnabled() && update.camera == regs.esi)
        {
            if ((update.onLadder || alignment.topPitchHandoff) && update.verticalSample)
            {
                const float baseZ = *reinterpret_cast<const float*>(regs.esi + 568);
                const float distanceBaseZ = *reinterpret_cast<const float*>(regs.esi + 584);
                if (std::isfinite(baseZ) && std::isfinite(distanceBaseZ))
                    verticalFollow = { regs.esi, alignment.ped, alignment.task, update.targetZ,
                        baseZ, distanceBaseZ, GetTickCount64() };
            }
        }
    }

    static void AfterCollision(SafetyHookContext& regs)
    {
        if (update.camera == regs.esi && update.finishPitchHandoff)
        {
            alignment.pitchLevelActive = false;
            alignment.topPitchHandoff = false;
        }

        if (!IsAlternateEnabled() || update.camera != regs.esi)
            return;
        const float distance = OrbitDistance(regs.esi);
        const bool valid = std::isfinite(distance) && std::isfinite(update.desiredDistance) && update.desiredDistance > 0.01f;
        if (update.onLadder || update.approaching)
        {
            alignment.orbitDistanceRatio = valid ? std::clamp(distance / update.desiredDistance, 0.0f, 1.0f) : 1.0f;
            alignment.collisionSampleValid = valid;
        }
    }

    // Movement sampling and native yaw handoff
    static void UpdateMovementSpeed(uintptr_t ped)
    {
        const auto* matrix = *reinterpret_cast<const rage::Matrix44**>(ped + 0x20);
        if ((update.onLadder || update.approaching) && matrix && GetRootBone)
        {
            alignas(16) rage::Vector4 root = matrix->pos;
            if (update.onLadder)
                GetRootBone(ped, &root);
            const bool valid = std::isfinite(root.x) && std::isfinite(root.y) && std::isfinite(root.z);
            root.w = 0.0f;
            if (valid && alignment.hasPreviousPosition)
            {
                const float x = root.x - alignment.previousPosition.x;
                const float y = root.y - alignment.previousPosition.y;
                const float z = root.z - alignment.previousPosition.z;
                const float distance = std::sqrt(x * x + y * y + z * z);
                if (std::isfinite(distance) && distance < 2.0f)
                    update.moveSpeed = distance / update.seconds;
                else
                    alignment.headingVelocity = 0.0f;
            }
            alignment.previousPosition = root;
            alignment.hasPreviousPosition = valid;
        }
        else
            alignment.hasPreviousPosition = false;
    }

    static void __fastcall TargetBetaCalc(uintptr_t camera, void* edx, float* beta, float target,
        float force, float rate, float cap, uintptr_t ped, uintptr_t support, float motion,
        uint32_t mode, float timestep)
    {
        cameraPed = ped;
        const bool enabled = IsAlternateEnabled();
        const auto flags = *reinterpret_cast<const uint8_t*>(camera + 910);
        const auto forceFlags = *reinterpret_cast<const uint8_t*>(camera + 909);
        const float before = *beta;
        const float nativeHeading = *reinterpret_cast<const float*>(camera + 776);
        if (update.camera == camera && enabled)
        {
            const auto control = ped && CPlayerPed::GetControlFromPlayer ? CPlayerPed::GetControlFromPlayer(ped) : 0;
            update.canAlign = control || update.phase == Phase::Mount || update.phase == Phase::Dismount;
            update.canAlign = update.canAlign && !(flags & 0x80) && !(forceFlags & 4) && force == 0.0f
                && std::isfinite(before) && std::isfinite(nativeHeading) && std::isfinite(timestep) && timestep > 0.0f;
            if (update.canAlign)
            {
                update.seconds = timestep * 0.02f;
                UpdateMovementSpeed(ped);
                UpdateAlignment();
                update.collisionRecovery = update.collisionConstrained
                    && (update.onLadder || update.approaching)
                    && update.action != CTaskSimpleClimbLadder::nIdleAction
                    && update.moveSpeed > 0.0f && update.pullWeight > 0.0f
                    && !update.lookX && !update.lookY;
            }
            else
            {
                alignment.hasPreviousPosition = false;
                alignment.collisionSampleValid = false;
                alignment.headingVelocity = 0.0f;
            }
            if (!update.canAlign || !update.approaching || update.lookX || update.lookY)
                alignment.rearOrbitActive = false;
            update.yawOwner = SelectYawOwner();
        }
        const bool customYaw = enabled && update.camera == camera && update.canAlign
            && update.yawOwner != YawOwner::NativeFollow;
        const bool skipNativeYaw = customYaw && !support;
        if (!skipNativeYaw)
            shTargetBetaCalc.unsafe_fastcall<void>(camera, edx, beta, target, force, rate, cap, ped, support, motion, mode, timestep);
        if (customYaw)
            *beta = nativeHeading;
    }

}

namespace CTaskComplexClimbLadder
{
    static constexpr int nBottomEntry = 1;
    static constexpr int nTopEntry = 4;
    static constexpr ptrdiff_t nEntryModeOffset = 20;

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

        static bool IsActiveForPed(uintptr_t ped)
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
            auto first = hook::pattern("51 A0 ? ? ? ? 56 8B F1 0F B6 D0 B9 01 00 00 00");
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
        if (!result || ped != LadderCamera::cameraPed || !IsAutoClimbEnabled())
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

    static bool ReadWorldMovementDirection(Direction& direction)
    {
        if (!onFootControl || !controlCameraHeading || !std::isfinite(*controlCameraHeading))
            return false;

        const float x = static_cast<float>(GetAxisValue(GetPedWalkLeftRight(onFootControl)));
        const float y = -static_cast<float>(GetAxisValue(GetPedWalkUpDown(onFootControl)));
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

    static bool CanAutoMountLadder(uintptr_t ped, uintptr_t ladder, int index, const Direction* movement, bool* rearEntry = nullptr)
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
        const int mode = std::abs(feetZ - bottom.z) <= std::abs(feetZ - top.z) ? nBottomEntry : nTopEntry;
        float heading = 0.0f;
        const bool wrongSide = !CalculatePositionsForThisLadder(ped, ladder, index, &top, &bottom, &position, &heading, mode, &normal, true);
        if (wrongSide)
        {
            if (!rearEntry || !movement || !RearApproach::installed || mode != nBottomEntry
                || !CalculatePositionsForThisLadder(ped, ladder, index, &top, &bottom, &position, &heading, mode, &normal, false))
                return false;
        }

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
                if (!wrongSide)
                {
                    const float x = position.x - matrix->pos.x, y = position.y - matrix->pos.y;
                    const float distance = std::sqrt(x * x + y * y);
                    const float ratio = GetDesiredMoveBlendRatio(ped);
                    const float limit = 0.8f + ratio * 0.1f;
                    if (!std::isfinite(distance) || distance >= limit)
                        return false;
                }
                if (Dot(desired, facing) <= std::cos(2.0f)
                    || Dot(*movement, toward) <= fMinMovementToLadderDot)
                    return false;

                const float sideDot = Dot(ladderNormal, toward);
                if (wrongSide && (Dot(toLadder, toLadder) > 1.0f || sideDot < 0.25f
                    || std::abs(feetZ - bottom.z) > 1.0f))
                    return false;
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
                const bool intended = headingWithinWidth || movementWithinWidth;
                if (rearEntry)
                    *rearEntry = wrongSide && intended;
                return intended;
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
        if constexpr (!InWater)
        {
            if (!ReadWorldMovementDirection(movement))
            {
                RearApproach::blocked = {};
                return 0;
            }
            if (movement.x * RearApproach::blocked.directionX + movement.y * RearApproach::blocked.directionY < 0.5f)
                RearApproach::blocked = {};
        }

        fLadderScanRadius = fLadderCandidateRadius;
        auto ladder = hbScanForLadderToClimb<InWater>.fun(ped, index, !InWater);
        fLadderScanRadius = 4.0f;

        bool rearEntry = false;
        if (ladder && !CanAutoMountLadder(ped, ladder, static_cast<int>(*index), InWater ? nullptr : &movement, InWater ? nullptr : &rearEntry))
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

// Task and camera hooks
static void InstallLadderTaskAndCameraHooks(void* ladderProcess)
{
    auto cameraGate = hook::pattern("55 8B EC 83 E4 F0 83 EC 28 F6 81 8E 03 00 00 80 56 57 8B 7D 08 89 4C 24 0C 0F 57 D2");
    auto mount = hook::pattern("55 8B EC 83 E4 F0 83 EC 18 56 57 8B 7D 08 8B F1 81 FF 87 03 00 00");
    auto activeTask = hook::pattern("68 19 04 00 00 83 C1 44 E8 ? ? ? ? 80");
    auto lookX = hook::pattern("57 8D BE 98 27 00 00 57 E8 ? ? ? ?");
    auto lookY = hook::pattern("57 8D BE B8 27 00 00 57 E8 ? ? ? ?");
    auto beginUpdate = hook::pattern("74 ? 57 8D 84 24 84 00 00 00 50 FF 74 24 6C 8B CE E8");
    auto orbitDistance = hook::pattern("F3 0F 11 44 24 20 F3 0F 11 44 24 44 57 8B CE E8");
    auto distanceHistory = hook::pattern("E8 ? ? ? ? 8D 44 24 30 50 E8 ? ? ? ? F3 0F 10 8C 24 AC 00 00 00");
    auto viewDistances = hook::pattern("8B 91 60 03 00 00 8B 04 95 ? ? ? ? 89 81 1C 03 00 00");
    auto orbitHeading = hook::pattern("83 C4 04 85 DB 74 ? 80 BB 89 32 00 00 00");
    auto beforeCollision = hook::pattern("E8 ? ? ? ? 8D 44 24 30 50 8D 4E 10 E8 ? ? ? ? 8B 8E 2C 01 00 00");
    auto afterCollision = hook::pattern("8D 44 24 30 50 8D 4E 10 E8 ? ? ? ? 8B 8E 2C 01 00 00 56");
    auto postSmoothTarget = hook::pattern("E8 ? ? ? ? 8D 84 24 80 00 00 00 50 FF 74 24 58");
    auto topExitDisplacement = hook::pattern("C6 86 CC 00 00 00 01 E8 ? ? ? ? 8B 45 08");
    auto cameraRoot = hook::pattern("E8 ? ? ? ? F3 0F 10 B6 80 02 00 00 F3 0F 10 BE");
    auto ladderPitch = hook::pattern("E8 ? ? ? ? 8B 84 24 ? ? ? ? 85 C0 74 11 8D 4C 24 18");
    auto verticalFollow = hook::pattern("F3 0F 5C 86 38 02 00 00 F3 0F 10 94 24");
    auto mountPitch = hook::pattern("F3 0F 10 44 24 24 F3 0F 5C 44 24 50");
    auto pitchLevel = hook::pattern("F3 0F 10 44 24 6C 83 EC 08 8B CE");

    const auto xEntry = lookX.get_first<uint8_t>(-0x14);
    const auto yEntry = lookY.get_first<uint8_t>(-0x14);
    const auto axis = injector::GetBranchDestination(lookX.get_first(8)).as_int();

    LadderCamera::FindActiveTaskByType = (decltype(LadderCamera::FindActiveTaskByType))injector::GetBranchDestination(activeTask.get_first(8)).as_int();
    LadderCamera::GetPedAimWeaponLeftRight = (decltype(LadderCamera::GetPedAimWeaponLeftRight))xEntry;
    LadderCamera::GetPedLookUpDown = (decltype(LadderCamera::GetPedLookUpDown))yEntry;
    LadderCamera::GetAxisValue = (decltype(LadderCamera::GetAxisValue))axis;
    LadderCamera::viewDistanceOffsets = *viewDistances.get_first<const float*>(9);
    LadderCamera::GetRootBone = (decltype(LadderCamera::GetRootBone))injector::GetBranchDestination(cameraRoot.get_first(0)).as_int();

    LadderCamera::GetMoverTrackDisplacement = (decltype(LadderCamera::GetMoverTrackDisplacement))injector::GetBranchDestination(topExitDisplacement.get_first(7)).as_int();

    LadderCamera::shBeginUpdate = safetyhook::create_mid(beginUpdate.get_first(0), LadderCamera::BeginUpdate);
    LadderCamera::shOrbitDistance = safetyhook::create_mid(orbitDistance.get_first(0), LadderCamera::OrbitDistanceRequest);
    LadderCamera::shDistanceHistory = safetyhook::create_mid(distanceHistory.get_first(0), LadderCamera::DistanceHistory);
    LadderCamera::shOrbitHeading = safetyhook::create_mid(orbitHeading.get_first(0), LadderCamera::OrbitHeading);
    LadderCamera::shBeforeCollision = safetyhook::create_mid(beforeCollision.get_first(0), LadderCamera::BeforeCollision);
    LadderCamera::shAfterCollision = safetyhook::create_mid(afterCollision.get_first(0), LadderCamera::AfterCollision);
    LadderCamera::shPostSmoothTarget = safetyhook::create_inline(
        injector::GetBranchDestination(postSmoothTarget.get_first(0)).as_int(), LadderCamera::PostSmoothTarget);

    LadderCamera::shLadderPitch = safetyhook::create_inline(
        injector::GetBranchDestination(ladderPitch.get_first(0)).as_int(), LadderCamera::LadderPitch);
    LadderCamera::shVerticalFollow = safetyhook::create_mid(verticalFollow.get_first(0), LadderCamera::VerticalFollow);
    LadderCamera::shMountPitch = safetyhook::create_mid(mountPitch.get_first(0), LadderCamera::MountPitch);
    LadderCamera::shPitchLevel = safetyhook::create_mid(pitchLevel.get_first(0), LadderCamera::PitchLevel);
    LadderCamera::shTargetBetaCalc = safetyhook::create_inline(cameraGate.get_first(0), LadderCamera::TargetBetaCalc);
    CTaskComplexClimbLadder::shCreateSubTask = safetyhook::create_inline(mount.get_first(0), CTaskComplexClimbLadder::CreateSubTask);
    ShortLadderTraversal::shProcessPed = safetyhook::create_inline(ladderProcess, ShortLadderTraversal::ProcessPed);
}

// Module initialization
class AutoLadder
{
public:
    AutoLadder()
    {
        FusionFix::onInitEvent() += []()
        {
            LadderCamera::ReadIniSettings();

            LadderCamera::OrbitProbe::Resolve();

            auto onFootInput = find_pattern("76 ? 8A 81 ? ? ? ? 32 C2 3C ? 76 ? F7 87 ? ? ? ? ? ? ? ? 74", "76 ? 8A 96 ? ? ? ? 32 D0 80 FA ? 76 ? F7 83 ? ? ? ? ? ? ? ? 74 ? 8B 44 24");
            auto onFootScan = find_pattern("E8 ? ? ? ? 83 C4 ? 89 44 24 ? F6 87", "E8 ? ? ? ? 83 C4 ? 89 44 24 ? EB ? 8B 74 24");
            auto waterInput = find_pattern("80 F9 ? 0F 86 ? ? ? ? 8B 4E", "3C ? 0F 86 ? ? ? ? 8B 4B");
            auto waterScan = find_pattern("E8 ? ? ? ? 83 C4 ? 85 C0 74 ? 8B 87 ? ? ? ? 81 8F", "E8 ? ? ? ? 83 C4 ? 85 C0 74 ? 81 8E");
            auto scanRadius = find_pattern("F3 0F 10 05 ? ? ? ? F3 0F 51 CC", "F3 0F 10 25 ? ? ? ? 0F 28 C1 0F 28 D3");
            auto ladderCleanup = find_pattern("56 57 8B 7C 24 ? 57 8B F1 E8 ? ? ? ? 57 8B CE E8 ? ? ? ? 8B 8F", "56 8B 74 24 ? 57 56 8B F9 E8 ? ? ? ? 56 8B CF E8 ? ? ? ? 8B 8E");

            auto sections = hook::pattern("FF B6 ? ? ? ? 51 E8 ? ? ? ? 83 C4 14 84 C0 0F 84");
            auto ladderProcess = hook::pattern("55 8B EC 83 E4 F0 83 EC 68 56 8B 75 08 57 8B 46 38");
            auto positions = hook::pattern("55 8B EC 83 E4 F0 81 EC ? ? ? ? 56 57 8B 7D 0C FF 75 10");
            auto leftRight = hook::pattern("56 57 8B F9 8D B7 58 27 00 00 56 E8");
            auto upDown = hook::pattern("56 57 8B F9 8D B7 78 27 00 00 56 E8");
            auto cameraHeading = hook::pattern("F3 0F 58 05 ? ? ? ? F3 0F 11 04 24 E8 ? ? ? ? D9 5C 24 0C");

            CTaskComplexClimbLadder::GetPedWalkLeftRight = (decltype(CTaskComplexClimbLadder::GetPedWalkLeftRight))leftRight.get_first(0);
            CTaskComplexClimbLadder::GetPedWalkUpDown = (decltype(CTaskComplexClimbLadder::GetPedWalkUpDown))upDown.get_first(0);
            CTaskComplexClimbLadder::GetAxisValue = (decltype(CTaskComplexClimbLadder::GetAxisValue))injector::GetBranchDestination(leftRight.get_first(11)).as_int();
            CTaskComplexClimbLadder::controlCameraHeading = *cameraHeading.get_first<const float*>(4);
            CTaskComplexClimbLadder::ResolveMoveBlendLayout();

            CTaskComplexClimbLadder::FindTopAndBottomOfAllLadderSections = (decltype(CTaskComplexClimbLadder::FindTopAndBottomOfAllLadderSections))injector::GetBranchDestination(sections.get_first(7)).as_int();
            CTaskComplexClimbLadder::CalculatePositionsForThisLadder = (decltype(CTaskComplexClimbLadder::CalculatePositionsForThisLadder))positions.get_first(0);

            InstallLadderTaskAndCameraHooks(ladderProcess.get_first(0));

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
            LadderCamera::TopMountHandoff::Install();

            if (LadderCamera::RegisterMenuPreference())
                LadderCamera::ApplyMenuSetting();
        };
    }
} AutoLadder;
