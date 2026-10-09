module;

#include <common.hxx>
#include <cmath>
#include <utility>
#include <cstring>
#include <cstdio>
#include <filesystem>
#include <algorithm>
#include <array>
#include <atomic>
#include <mutex>

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

static constexpr int32_t nRegrabCooldownMs = 2500;
static constexpr int32_t nOnFootRegrabCooldownMs = 350;
static constexpr float fLadderEntryHalfWidth = 0.775f;
static constexpr float fLadderEntryHalfWidthStill = 0.575f;
static constexpr float fCloseBottomEntryDistance = 0.9f;
static constexpr float fSideEntryMaxSpeed = 1.5f;
static constexpr float fMinMovementToLadderDot = 0.2f;
static constexpr float fTopEntryHeadingDot = 0.4f;

static float fLadderScanRadius = 4.0f;
static constexpr float fLadderCandidateRadius = 1.5f;

static int32_t nLastLadderExitTime = -nRegrabCooldownMs;

static bool IsAutoClimbEnabled()
{
    static auto AutoClimbLadders = FusionFixSettings.GetRef("PREF_AUTOCLIMBLADDERS");
    return AutoClimbLadders->get();
}

namespace LadderCamera
{
    static SafetyHookInline shTargetBetaCalc;
    static SafetyHookMid shHeadingLimit;
    static SafetyHookMid shBeginUpdate;
    static SafetyHookMid shLookBehindInput;
    static SafetyHookMid shOrbitHeading;
    static SafetyHookInline shLadderPitch;
    static SafetyHookMid shVerticalFollow;
    static SafetyHookMid shMountPitch;
    static SafetyHookMid shPitchLevel;
    static SafetyHookMid shBeforeCollision;
    static SafetyHookMid shAfterCollision;
    static uintptr_t (__thiscall* FindActiveTaskByType)(uintptr_t, int);
    static uintptr_t (__thiscall* GetPedAimWeaponLeftRight)(uintptr_t);
    static uintptr_t (__thiscall* GetPedLookUpDown)(uintptr_t);
    static int (__cdecl* GetAxisValue)(uintptr_t);
    static uintptr_t (__stdcall* GetRootBone)(uintptr_t, rage::Vector4*);
    static uintptr_t cameraPed = 0;
    enum class CameraStyle { Native = 0, PitchFollow = 1, HeightFollow = 2 };
    static CameraStyle cameraStyle = CameraStyle::Native;

    static void SetStyle(int value)
    {
        switch (value)
        {
        case 1: cameraStyle = CameraStyle::PitchFollow; break;
        case 2: cameraStyle = CameraStyle::HeightFollow; break;
        default: cameraStyle = CameraStyle::Native; break;
        }
    }

    static bool IsEnabled() { return cameraStyle != CameraStyle::Native; }
    static bool IsHeightFollow() { return cameraStyle == CameraStyle::HeightFollow; }

    static constexpr float pi = 3.14159265358979323846f;
    static constexpr float fMinOrbitHeading = -90.0f * pi / 180.0f;
    static constexpr float fMaxOrbitHeading = 80.0f * pi / 180.0f;
    static constexpr float fDesiredPullAroundHeadingOffset = 75.0f * pi / 180.0f;
    static constexpr float fOrbitPitchLimit = 75.0f * pi / 180.0f;
    static constexpr float fMinOrbitDistanceRatioForCustomPullAround = 0.85f;
    // Severe rear-entry compression needs a cut before the orbit collapses into the player.
    static constexpr float fRearEntryCutDistanceRatio = 0.5f;
    static constexpr float fLimitBlendTime = 0.75f;
    // DEFAULT_FOLLOW_PED_CAMERA retail ladder tuning; simple spring mode ignores the rate/error controls.
    static constexpr float fHeadingPullAroundMaxMoveSpeed = 1.0f;
    static constexpr float fHeadingPullAroundSpringConstant = 4.0f;
    static constexpr float fHeadingPullAroundSpringDampingRatio = 0.95f;
    // Fraction of remaining pitch error removed per 30 Hz reference step, not a duration.
    static constexpr float fPitchLevelSmoothRate = 0.1f;
    static constexpr float fPitchLevelTolerance = 1.0f * pi / 180.0f;

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
        unsigned limitHits = 0;
        rage::Vector4 previousPosition{};
        bool hasPreviousPosition = false;
        float lastHeadingDelta = 0.0f;
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
        ULONGLONG lastInputTime = 0;
        LookInputEnvelope lookEnvelope;
    };
    static thread_local LadderAlignment alignment;
    struct CameraUpdate
    {
        uintptr_t camera = 0;
        uintptr_t pitchCamera = 0;
        uintptr_t pitchPed = 0;
        bool hasLadderTask = false;
        Phase phase = Phase::None;
        int action = -1;
        int lookX = 0;
        int lookY = 0;
        float desiredDistance = 0.0f;
        float orbitDistanceRatio = 1.0f;
        float previousOrbitDistanceRatio = 1.0f;
        bool collisionConstrained = false;
        bool collisionSampleValid = false;
        bool collisionRecovery = false;
        bool rearEntryFramed = false;
        float desiredHeadingOffset = 0.0f;
        bool pitchLimitsApplied = false;
        bool canAlign = false;
        float seconds = 0.0f;
        float moveSpeed = 0.0f;
        float verticalSpeed = 0.0f;
        bool finalStep = false;
        bool topDismount = false;
        bool onLadder = false;
        bool approaching = false;
        bool controller = true;
        bool rootValid = false;
        float rootZ = 0.0f;
        float blend = 0.0f;
        float orbitBlend = 0.0f;
        float pullWeight = 0.0f;
        float targetZ = 0.0f;
        float targetDeltaZ = 0.0f;
        float nativeCarryZ = 0.0f;
        float verticalCarryZ = 0.0f;
        bool verticalSample = false;
        bool verticalCarried = false;
        bool nativePitchSample = false;
        float nativePitchInputDelta = 0.0f;
        bool pitchSuppressed = false;
        bool mountPitchPreserved = false;
        float mountPitchTargetDelta = 0.0f;
        float previousPitch = 0.0f;
        float neutralPitch = 0.0f;
        float pitchLevelDelta = 0.0f;
        bool pitchReferenceValid = false;
        bool pitchLevelApplied = false;
        float cameraZBeforeCollision = 0.0f;
        float pivotZBeforeCollision = 0.0f;
        bool logFrame = false;
        ULONGLONG sampleTime = 0;
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

    struct MountPitchState
    {
        uintptr_t camera = 0;
        uintptr_t ped = 0;
        uintptr_t task = 0;
        float target = 0.0f;
        ULONGLONG sampleTime = 0;
    };
    static thread_local MountPitchState mountPitchState;

    static FILE* log = nullptr;

    // Diagnostic snapshots only: none of these fields are written back to the game.
    struct MountVisibilitySnapshot
    {
        rage::Vector4 position{}, camera{}, pivot{}, boundCenter{};
        float boundRadius = 0.0f;
        float pitch = 0.0f, yaw = 0.0f;
        uint32_t entityFlags = 0;
        int onScreen = -1;
        bool positionValid = false, boundsValid = false;
    };
    struct MountVisibilitySample
    {
        bool autoClimb = false;
        ULONGLONG time = 0;
        LONGLONG ticks = 0;
        uint32_t frame = 0;
        uintptr_t task = 0, simple = 0;
        int action = -1, phase = 0;
        bool completed = false;
        float gameStep = 0.0f, cameraStep = 0.0f, seconds = 0.0f;
        float carry = 0.0f, targetDelta = 0.0f, yawStep = 0.0f;
        float desiredDistance = 0.0f, distance = 0.0f;
        bool held = false, suppressed = false;
        bool collisionEntry = false;
        rage::Vector4 cameraBeforeCollision{}, pivotBeforeCollision{};
        MountVisibilitySnapshot before{}, after{};
    };
    struct MountVisibilityTrace
    {
        uintptr_t camera = 0, ped = 0, observedTask = 0;
        Ped handle = 0;
        ULONGLONG until = 0;
        unsigned capture = 0, count = 0;
        bool active = false, pending = false;
        MountVisibilitySample current{};
        std::array<MountVisibilitySample, 4096> samples{};
    };
    static thread_local MountVisibilityTrace mountVisibility;

    // Refresh visibility bounds after the native top-mount root rebase.
    namespace TopMountBounds
    {
        static void (__thiscall* SetInstanceMatrix)(uintptr_t, const void*);
        static void (__thiscall* UpdateSkeleton)(uintptr_t);
        static void (__thiscall* CalculateCompositeExtents)(uintptr_t, bool);
        static void (__thiscall* UpdateObjectLocation)(uintptr_t, int, const void*);
        static void (__thiscall* UpdateObjectLocationAndRadius)(uintptr_t, int, float, const void*);
        static uintptr_t* physicsLevel = nullptr;
        static bool ready = false;

        enum class Result { Disabled, Applied, BindingsUnavailable, NoRebase, InstanceMismatch, UnsupportedState, MissingGeometry, InvalidLevelEntry };

        static Result Refresh(uintptr_t ped, uintptr_t instanceBefore, uintptr_t root, bool rebased)
        {
            if (!IsEnabled())
                return Result::Disabled;
            if (!ready)
                return Result::BindingsUnavailable;
            if (!rebased || !root)
                return Result::NoRebase;
            const auto instance = *reinterpret_cast<const uintptr_t*>(ped + 1972);
            if (!instance || instance != instanceBefore)
                return Result::InstanceMismatch;
            // Only the animated secondary instance observed at the native top-mount handoff.
            if (*reinterpret_cast<const int*>(ped + 1976) != 2 || !(*reinterpret_cast<const uint8_t*>(ped + 244) & 2))
                return Result::UnsupportedState;
            const auto* pedVtable = *reinterpret_cast<const uintptr_t**>(ped);
            const auto getInstance = reinterpret_cast<uintptr_t(__thiscall*)(uintptr_t)>(pedVtable[0xA0 / 4]);
            if (getInstance(ped) != instance)
                return Result::InstanceMismatch;
            const auto* instanceVtable = *reinterpret_cast<const uintptr_t**>(instance);
            const auto getSkeleton = reinterpret_cast<uintptr_t(__thiscall*)(uintptr_t)>(instanceVtable[0xE0 / 4]);
            const auto matrix = *reinterpret_cast<const uintptr_t*>(ped + 32);
            const auto archetype = *reinterpret_cast<const uintptr_t*>(instance + 4);
            const auto bound = archetype ? *reinterpret_cast<const uintptr_t*>(archetype + 12) : 0;
            if (!matrix || !bound || !getSkeleton(instance)
                || !*reinterpret_cast<const uintptr_t*>(bound + 132) || !*reinterpret_cast<const uintptr_t*>(bound + 136)
                || !*reinterpret_cast<const uint16_t*>(bound + 146))
                return Result::MissingGeometry;
            const auto level = *physicsLevel;
            const auto index = *reinterpret_cast<const uint16_t*>(instance + 8);
            if (!level || index == 0xFFFF || index >= *reinterpret_cast<const uint16_t*>(level + 38))
                return Result::InvalidLevelEntry;
            const auto entries = *reinterpret_cast<const uintptr_t*>(level + 112);
            if (!entries || (*reinterpret_cast<const uintptr_t*>(entries + index * 8 + 4) & ~uintptr_t{ 0xF }) != instance)
                return Result::InvalidLevelEntry;

            // Use the native relocation refresh sequence, without Teleport's task/physics changes.
            const auto copyMatrixHistory = reinterpret_cast<void(__thiscall*)(uintptr_t, const void*)>(instanceVtable[0x10 / 4]);
            copyMatrixHistory(instance, reinterpret_cast<const void*>(instance + 16));
            SetInstanceMatrix(instance, reinterpret_cast<const void*>(matrix));
            copyMatrixHistory(instance, reinterpret_cast<const void*>(instance + 16));
            UpdateSkeleton(ped);
            const auto poseBounds = reinterpret_cast<void(__thiscall*)(uintptr_t)>(instanceVtable[0xBC / 4]);
            // Both component histories must use the rebased coordinate system.
            poseBounds(instance);
            poseBounds(instance);
            const float oldRadius = *reinterpret_cast<const float*>(bound + 8);
            CalculateCompositeExtents(bound, true);
            if (oldRadius == *reinterpret_cast<const float*>(bound + 8))
                UpdateObjectLocation(level, index, nullptr);
            else
                UpdateObjectLocationAndRadius(level, index, oldRadius, nullptr);
            return Result::Applied;
        }

        static void Resolve()
        {
            auto matrix = hook::pattern("FF 52 10 FF 77 20 8B 8F B4 07 00 00 E8 ? ? ? ?");
            auto skeleton = hook::pattern("FF 50 04 8B CE E8 ? ? ? ? 81 8E 9C 02 00 00");
            auto extents = hook::pattern("55 8B EC 83 E4 F8 83 EC 08 80 7D 08 00 56 8B D1");
            auto radius = hook::pattern("55 8B EC 83 E4 F8 83 EC 18 83 3D ? ? ? ? 00 56 8B D1");
            auto location = hook::pattern("8B 0D ? ? ? ? 50 E8 ? ? ? ? 81 A7 18 01 00 00");
            if (matrix.size() != 1 || skeleton.size() != 1 || extents.size() != 1 || radius.size() != 1 || location.size() != 1)
            {
                if (log)
                    std::fprintf(log, "Top-mount bounds refresh unavailable: matrix=%zu skeleton=%zu extents=%zu radius=%zu location=%zu\n",
                        matrix.size(), skeleton.size(), extents.size(), radius.size(), location.size());
                return;
            }
            SetInstanceMatrix = reinterpret_cast<decltype(SetInstanceMatrix)>(injector::GetBranchDestination(matrix.get_first(12)).as_int());
            UpdateSkeleton = reinterpret_cast<decltype(UpdateSkeleton)>(injector::GetBranchDestination(skeleton.get_first(5)).as_int());
            CalculateCompositeExtents = reinterpret_cast<decltype(CalculateCompositeExtents)>(extents.get_first(0));
            UpdateObjectLocationAndRadius = reinterpret_cast<decltype(UpdateObjectLocationAndRadius)>(radius.get_first(0));
            UpdateObjectLocation = reinterpret_cast<decltype(UpdateObjectLocation)>(injector::GetBranchDestination(location.get_first(7)).as_int());
            physicsLevel = *location.get_first<uintptr_t*>(2);
            ready = true;
            if (log)
                std::fprintf(log, "Top-mount bounds refresh resolved: matrix=%p skeleton=%p extents=%p radius=%p location=%p level=%p\n",
                    reinterpret_cast<void*>(SetInstanceMatrix), reinterpret_cast<void*>(UpdateSkeleton), reinterpret_cast<void*>(CalculateCompositeExtents),
                    reinterpret_cast<void*>(UpdateObjectLocationAndRadius), reinterpret_cast<void*>(UpdateObjectLocation), physicsLevel);
        }
    }

    // Root-rebase hooks drive the repair; optional diagnostics also observe pre-render.
    namespace HandoffTrace
    {
        static SafetyHookInline shOffsetPedPosition, shPreRender2;
        static SafetyHookMid shLocalRoot;
        static uintptr_t preRenderAddress = 0;
        static std::atomic<uintptr_t> watchedPed = 0;
        static std::mutex mutex;
        enum class Stage { OffsetBefore, RootBeforeWrite, OffsetAfter, PreRenderBefore, PreRenderAfter, BoundsRefreshAfter };
        struct Sample
        {
            LONGLONG ticks = 0;
            uint32_t frame = 0, thread = 0, entityFlags = 0, physicsFlags = 0;
            uintptr_t ped = 0, task = 0, instance = 0, bound = 0, root = 0;
            Stage stage{};
            int refreshResult = -1;
            std::array<float, 16> instanceMatrix{};
            rage::Vector4 position{}, localCenter{}, worldCenter{}, rootTranslation{};
            float radius = 0.0f;
            uint8_t skeletonFlags = 0, alpha = 0;
            int instanceState = 0;
            bool positionValid = false, boundsValid = false, rootValid = false;
        };
        static std::array<Sample, 256> samples{};
        static unsigned count = 0, dropped = 0, capture = 0;
        static uint32_t firstFrame = 0;
        static ULONGLONG started = 0;
        static uintptr_t complexTask = 0;
        static bool captureAutoClimb = false;
        struct OffsetContext
        {
            uintptr_t ped = 0, task = 0, root = 0;
        };
        static thread_local OffsetContext offsetContext;

        static void Record(Stage stage, uintptr_t ped, uintptr_t task = 0, uintptr_t root = 0, int refreshResult = -1)
        {
            if (!log || watchedPed.load(std::memory_order_relaxed) != ped)
                return;
            std::lock_guard lock(mutex);
            if (!ped || watchedPed.load(std::memory_order_relaxed) != ped || !CTimer::m_frameCount
                || static_cast<uint32_t>(*CTimer::m_frameCount - firstFrame) > 12)
                return;
            if (count == samples.size())
            {
                ++dropped;
                return;
            }
            auto& s = samples[count++];
            s = {};
            LARGE_INTEGER ticks{};
            QueryPerformanceCounter(&ticks);
            s.ticks = ticks.QuadPart;
            s.frame = *CTimer::m_frameCount;
            s.thread = GetCurrentThreadId();
            s.stage = stage;
            s.refreshResult = refreshResult;
            s.ped = ped;
            s.task = task;
            s.entityFlags = *reinterpret_cast<const uint32_t*>(ped + 36);
            s.physicsFlags = *reinterpret_cast<const uint32_t*>(ped + 668);
            s.skeletonFlags = *reinterpret_cast<const uint8_t*>(ped + 244);
            s.alpha = *reinterpret_cast<const uint8_t*>(ped + 91);
            s.instanceState = *reinterpret_cast<const int*>(ped + 1976);
            const auto matrix = *reinterpret_cast<const uintptr_t*>(ped + 32);
            if (matrix)
            {
                std::memcpy(&s.position, reinterpret_cast<const void*>(matrix + 48), 12);
                s.positionValid = true;
            }
            s.instance = *reinterpret_cast<const uintptr_t*>(ped + 1972);
            if (!s.instance)
                s.instance = *reinterpret_cast<const uintptr_t*>(ped + 56);
            if (s.instance)
            {
                std::memcpy(s.instanceMatrix.data(), reinterpret_cast<const void*>(s.instance + 16), 64);
                const auto archetype = *reinterpret_cast<const uintptr_t*>(s.instance + 4);
                s.bound = archetype ? *reinterpret_cast<const uintptr_t*>(archetype + 12) : 0;
                if (s.bound)
                {
                    std::memcpy(&s.localCenter, reinterpret_cast<const void*>(s.bound + 48), 12);
                    s.radius = *reinterpret_cast<const float*>(s.bound + 8) + *reinterpret_cast<const float*>(ped + 252);
                    const auto& m = s.instanceMatrix;
                    s.worldCenter.x = m[12] + m[0] * s.localCenter.x + m[4] * s.localCenter.y + m[8] * s.localCenter.z;
                    s.worldCenter.y = m[13] + m[1] * s.localCenter.x + m[5] * s.localCenter.y + m[9] * s.localCenter.z;
                    s.worldCenter.z = m[14] + m[2] * s.localCenter.x + m[6] * s.localCenter.y + m[10] * s.localCenter.z;
                    s.boundsValid = true;
                }
            }
            // Only use the matrix returned by the game's own getter during this call.
            // Retaining it across frames could outlive the skeleton or change its identity.
            s.root = root;
            if (root)
            {
                std::memcpy(&s.rootTranslation, reinterpret_cast<const void*>(root + 48), 12);
                s.rootValid = true;
            }
        }

        static void Flush(bool cancel = false)
        {
            if (!watchedPed.load(std::memory_order_relaxed))
                return;
            std::lock_guard lock(mutex);
            if (!watchedPed.load(std::memory_order_relaxed))
                return;
            const auto now = GetTickCount64();
            if (!cancel && CTimer::m_frameCount && static_cast<uint32_t>(*CTimer::m_frameCount - firstFrame) <= 12
                && now - started < 2000)
                return;
            watchedPed.store(0, std::memory_order_relaxed);
            if (!log)
                return;
            std::fprintf(log, "handoffCapture id=%u firstFrame=%u complex=%p samples=%u dropped=%u cancelled=%d autoClimb=%d style=%d\n",
                capture, firstFrame, reinterpret_cast<void*>(complexTask), count, dropped, cancel, captureAutoClimb, std::to_underlying(cameraStyle));
            static constexpr const char* names[]{ "offsetBefore", "rootBeforeWrite", "offsetAfter", "preRenderBefore", "preRenderAfter", "boundsRefreshAfter" };
            for (unsigned i = 0; i < count; ++i)
            {
                const auto& s = samples[i];
                std::fprintf(log, "handoff id=%u sample=%u frame=%u qpc=%lld thread=%u stage=%s ped=%p task=%p entityFlags=%08X physicsFlags=%08X skeletonFlags=%02X alpha=%u instanceState=%d posValid=%d pos=%.5f/%.5f/%.5f instance=%p bound=%p boundsValid=%d local=%.5f/%.5f/%.5f world=%.5f/%.5f/%.5f radius=%.5f root=%p rootValid=%d rootLocal=%.5f/%.5f/%.5f refresh=%d\n",
                    capture, i, s.frame, s.ticks, s.thread, names[static_cast<unsigned>(s.stage)],
                    reinterpret_cast<void*>(s.ped), reinterpret_cast<void*>(s.task), s.entityFlags, s.physicsFlags,
                    static_cast<unsigned>(s.skeletonFlags), static_cast<unsigned>(s.alpha), s.instanceState,
                    s.positionValid, s.position.x, s.position.y, s.position.z,
                    reinterpret_cast<void*>(s.instance), reinterpret_cast<void*>(s.bound), s.boundsValid,
                    s.localCenter.x, s.localCenter.y, s.localCenter.z, s.worldCenter.x, s.worldCenter.y, s.worldCenter.z,
                    s.radius, reinterpret_cast<void*>(s.root), s.rootValid, s.rootTranslation.x, s.rootTranslation.y, s.rootTranslation.z, s.refreshResult);
                const auto& m = s.instanceMatrix;
                std::fprintf(log, "handoffMatrix id=%u sample=%u x=%.5f/%.5f/%.5f y=%.5f/%.5f/%.5f z=%.5f/%.5f/%.5f pos=%.5f/%.5f/%.5f\n",
                    capture, i, m[0], m[1], m[2], m[4], m[5], m[6], m[8], m[9], m[10], m[12], m[13], m[14]);
            }
            std::fprintf(log, "handoffCaptureEnd id=%u\n", capture);
            std::fflush(log);
        }

        static void __fastcall OffsetPedPosition(uintptr_t task, void* edx, uintptr_t ped)
        {
            bool trace = false;
            bool topMount = false;
            if (IsEnabled() && FindActiveTaskByType
                && CPlayer::getLocalPlayerPed && ped && ped == CPlayer::getLocalPlayerPed())
            {
                const auto intelligence = *reinterpret_cast<const uintptr_t*>(ped + 548);
                const auto complex = intelligence ? FindActiveTaskByType(intelligence + 68, std::to_underlying(TaskID::TaskComplexClimbLadder)) : 0;
                const auto* vtable = *reinterpret_cast<const uintptr_t**>(ped);
                if (complex && *reinterpret_cast<const int*>(complex + 20) == 4 && vtable[0xCC / 4] == preRenderAddress)
                {
                    topMount = true;
                    if (log && CTimer::m_frameCount)
                    {
                        std::lock_guard lock(mutex);
                        if (!watchedPed.load(std::memory_order_relaxed))
                        {
                            firstFrame = *CTimer::m_frameCount;
                            started = GetTickCount64();
                            complexTask = complex;
                            captureAutoClimb = IsAutoClimbEnabled();
                            count = dropped = 0;
                            ++capture;
                            watchedPed.store(ped, std::memory_order_relaxed);
                            trace = true;
                        }
                    }
                }
            }
            const auto previous = offsetContext;
            offsetContext = topMount ? OffsetContext{ ped, task, 0 } : OffsetContext{};
            const auto instanceBefore = topMount ? *reinterpret_cast<const uintptr_t*>(ped + 1972) : 0;
            const bool pendingRebase = topMount && (*reinterpret_cast<const uint8_t*>(task + 25) & 1);
            if (trace)
            {
                Record(Stage::OffsetBefore, ped, task);
            }
            shOffsetPedPosition.unsafe_fastcall<void>(task, edx, ped);
            if (trace)
            {
                Record(Stage::OffsetAfter, ped, task, offsetContext.root);
            }
            if (topMount)
            {
                const bool rebased = pendingRebase && !(*reinterpret_cast<const uint8_t*>(task + 25) & 1);
                const auto result = TopMountBounds::Refresh(ped, instanceBefore, offsetContext.root, rebased);
                Record(Stage::BoundsRefreshAfter, ped, task, 0, static_cast<int>(result));
            }
            offsetContext = previous;
        }

        static void LocalRoot(SafetyHookContext& regs)
        {
            if (offsetContext.ped && offsetContext.ped == regs.edi)
            {
                offsetContext.root = regs.eax;
                Record(Stage::RootBeforeWrite, regs.edi, offsetContext.task, regs.eax);
            }
        }

        static void __fastcall PreRender2(uintptr_t ped, void* edx)
        {
            Record(Stage::PreRenderBefore, ped);
            shPreRender2.unsafe_fastcall<void>(ped, edx);
            Record(Stage::PreRenderAfter, ped);
        }

        static void Install()
        {
            if (!IsEnabled())
                return;
            auto offset = hook::pattern("55 8B EC 83 E4 F0 81 EC 78 01 00 00 56 57 8B F1");
            auto render = hook::pattern("55 8B EC 83 E4 F0 81 EC E4 00 00 00 53 56 57 8B F9 E8 ? ? ? ? 80 3D");
            auto root = hook::pattern("6A 00 8B CF E8 ? ? ? ? 8B F0 8B 44 24 3C F6 40 19 02");
            if (offset.size() != 1 || render.size() != 1 || root.size() != 1)
            {
                if (log)
                {
                    std::fprintf(log, "Top-mount handoff bindings unavailable: offset=%zu preRender=%zu root=%zu; existing camera hooks unchanged.\n", offset.size(), render.size(), root.size());
                    std::fflush(log);
                }
                return;
            }
            preRenderAddress = reinterpret_cast<uintptr_t>(render.get_first(0));
            TopMountBounds::Resolve();
            shLocalRoot = safetyhook::create_mid(root.get_first(9), LocalRoot);
            shOffsetPedPosition = safetyhook::create_inline(offset.get_first(0), OffsetPedPosition);
            if (!shLocalRoot || !shOffsetPedPosition)
            {
                shOffsetPedPosition.reset();
                shLocalRoot.reset();
                if (log)
                {
                    std::fprintf(log, "Top-mount repair hook failure; repair hooks removed, existing camera hooks unchanged.\n");
                    std::fflush(log);
                }
                return;
            }
            // This observer is not required to capture the native root or refresh bounds.
            if (log)
            {
                shPreRender2 = safetyhook::create_inline(render.get_first(0), PreRender2);
                std::fprintf(log, "Top-mount handoff installed: offset=%p preRender=%p root=%p preRenderTrace=%d boundsReady=%d window=13_frames capacity=256\n",
                    offset.get_first(0), render.get_first(0), root.get_first(9), static_cast<bool>(shPreRender2), TopMountBounds::ready);
                std::fflush(log);
            }
        }
    }
    // End handoff diagnostics.

    static MountVisibilitySnapshot ReadMountVisibility(uintptr_t camera, uintptr_t ped, Ped handle)
    {
        MountVisibilitySnapshot s;
        const auto* matrix = *reinterpret_cast<const rage::Matrix44**>(ped + 0x20);
        if (matrix)
        {
            s.position = matrix->pos;
            s.positionValid = true;
        }
        std::memcpy(&s.camera, reinterpret_cast<const void*>(camera + 64), 12);
        std::memcpy(&s.pivot, reinterpret_cast<const void*>(camera + 80), 12);
        s.pitch = *reinterpret_cast<const float*>(camera + 772);
        s.yaw = *reinterpret_cast<const float*>(camera + 776);
        s.entityFlags = *reinterpret_cast<const uint32_t*>(ped + 0x24);
        // PC IS_CHAR_ON_SCREEN uses these virtual sphere getters (+54/+58), not rendered-pixel visibility.
        const auto* vtable = *reinterpret_cast<const uintptr_t**>(ped);
        if (vtable && vtable[21] && vtable[22])
        {
            rage::Vector4 center{};
            const auto* result = reinterpret_cast<const rage::Vector4* (__thiscall*)(uintptr_t, rage::Vector4*)>(vtable[21])(ped, &center);
            s.boundRadius = reinterpret_cast<float (__thiscall*)(uintptr_t)>(vtable[22])(ped);
            if (result)
            {
                std::memcpy(&s.boundCenter, result, 12);
                s.boundsValid = true;
            }
        }
        if (handle && Natives::DoesCharExist(handle))
            s.onScreen = Natives::IsCharOnScreen(handle) ? 1 : 0;
        return s;
    }

    static void FlushMountVisibility(const char* reason)
    {
        auto& trace = mountVisibility;
        if (!trace.active)
            return;
        if (trace.pending && trace.count < trace.samples.size())
            trace.samples[trace.count++] = trace.current;
        trace.pending = false;
        trace.active = false;
        if (!log)
            return;
        LARGE_INTEGER frequency{};
        QueryPerformanceFrequency(&frequency);
        std::fprintf(log, "mountCapture id=%u camera=%p ped=%p samples=%u reason=%s qpcFrequency=%lld\n",
            trace.capture, reinterpret_cast<void*>(trace.camera), reinterpret_cast<void*>(trace.ped),
            trace.count, reason, frequency.QuadPart);
        for (unsigned i = 0; i < trace.count; ++i)
        {
            const auto& s = trace.samples[i];
            const double wallMs = i && frequency.QuadPart ? 1000.0 * static_cast<double>(s.ticks - trace.samples[i - 1].ticks) / frequency.QuadPart : 0.0;
            std::fprintf(log, "mountFrame id=%u sample=%u t=%llu frame=%u wallMs=%.3f task=%p simple=%p action=%d phase=%d post=%d gameStep=%.6f cameraStep=%.6f seconds=%.6f carry=%.6f targetDelta=%.6f yawStep=%.6f desiredDist=%.5f actualDist=%.5f held=%d suppressed=%d autoClimb=%d\n",
                trace.capture, i, static_cast<unsigned long long>(s.time), s.frame, wallMs,
                reinterpret_cast<void*>(s.task), reinterpret_cast<void*>(s.simple), s.action, s.phase, s.completed,
                s.gameStep, s.cameraStep, s.seconds, s.carry, s.targetDelta, s.yawStep,
                s.desiredDistance, s.distance, s.held, s.suppressed, s.autoClimb);
            const auto writeSnapshot = [&](const MountVisibilitySnapshot& p, const char* stage)
            {
                std::fprintf(log, "mountVisibility id=%u sample=%u stage=%s visibleBit=%d entityFlags=%08X onScreen=%d posValid=%d pos=%.5f/%.5f/%.5f camera=%.5f/%.5f/%.5f pivot=%.5f/%.5f/%.5f pitch=%.6f yaw=%.6f boundsValid=%d bound=%.5f/%.5f/%.5f radius=%.5f\n",
                    trace.capture, i, stage, (p.entityFlags & 0x20) != 0, p.entityFlags, p.onScreen,
                    p.positionValid, p.position.x, p.position.y, p.position.z,
                    p.camera.x, p.camera.y, p.camera.z, p.pivot.x, p.pivot.y, p.pivot.z,
                    p.pitch, p.yaw, p.boundsValid, p.boundCenter.x, p.boundCenter.y, p.boundCenter.z, p.boundRadius);
            };
            writeSnapshot(s.before, "cameraEntry");
            if (s.collisionEntry)
                std::fprintf(log, "mountCollisionEntry id=%u sample=%u camera=%.5f/%.5f/%.5f pivot=%.5f/%.5f/%.5f\n",
                    trace.capture, i, s.cameraBeforeCollision.x, s.cameraBeforeCollision.y, s.cameraBeforeCollision.z,
                    s.pivotBeforeCollision.x, s.pivotBeforeCollision.y, s.pivotBeforeCollision.z);
            if (s.completed)
                writeSnapshot(s.after, "afterCollision");
        }
        std::fprintf(log, "mountCaptureEnd id=%u\n", trace.capture);
        std::fflush(log);
    }

    static void BeginMountVisibility(uintptr_t camera, uintptr_t ped, uintptr_t task, uintptr_t intelligence)
    {
        HandoffTrace::Flush();
        auto& trace = mountVisibility;
        const auto now = GetTickCount64();
        const bool entered = task && (task != trace.observedTask || camera != trace.camera || ped != trace.ped);
        trace.observedTask = task;
        const bool identityChanged = trace.camera != camera || trace.ped != ped;
        if (trace.active && (entered || identityChanged || now >= trace.until || trace.count + 1 >= trace.samples.size()))
            FlushMountVisibility(identityChanged ? "identityChanged" : entered ? "newTask" : now >= trace.until ? "windowComplete" : "capacity");
        if (!IsEnabled() || !log)
            return;
        if (entered)
        {
            trace.camera = camera;
            trace.ped = ped;
            trace.handle = 0;
            if (CPlayer::getLocalPlayerPed && CPlayer::getLocalPlayerPed() == ped)
                Natives::GetPlayerChar(Natives::GetPlayerId(), &trace.handle);
            trace.until = now + 6000;
            ++trace.capture;
            trace.count = 0;
            trace.active = true;
        }
        if (!trace.active || trace.camera != camera || trace.ped != ped)
            return;
        // Retain entry-only records if native processing skipped our post-collision hook.
        if (trace.pending)
            trace.samples[trace.count++] = trace.current;
        auto& s = trace.current;
        s = {};
        s.time = now;
        s.autoClimb = IsAutoClimbEnabled();
        LARGE_INTEGER ticks{};
        QueryPerformanceCounter(&ticks);
        s.ticks = ticks.QuadPart;
        s.frame = CTimer::m_frameCount ? *CTimer::m_frameCount : 0;
        s.gameStep = CTimer::fTimeStep ? *CTimer::fTimeStep : 0.0f;
        s.cameraStep = CTimer::fCamTimeStep ? *CTimer::fCamTimeStep : 0.0f;
        s.task = task;
        s.simple = intelligence ? FindActiveTaskByType(intelligence + 68, std::to_underlying(TaskID::TaskSimpleClimbLadder)) : 0;
        s.action = s.simple ? *reinterpret_cast<const int*>(s.simple + CTaskSimpleClimbLadder::nActionOffset) : -1;
        s.before = ReadMountVisibility(camera, ped, trace.handle);
        trace.pending = true;
    }

    static void EndMountVisibility(uintptr_t camera)
    {
        auto& trace = mountVisibility;
        if (!trace.active || !trace.pending || trace.camera != camera || trace.ped != cameraPed)
            return;
        auto& s = trace.current;
        s.after = ReadMountVisibility(camera, trace.ped, trace.handle);
        s.completed = true;
        s.phase = static_cast<int>(update.phase);
        s.seconds = update.seconds;
        s.carry = update.verticalCarryZ;
        s.targetDelta = update.targetDeltaZ;
        s.yawStep = alignment.lastHeadingDelta;
        s.desiredDistance = update.desiredDistance;
        const float x = s.after.camera.x - s.after.pivot.x;
        const float y = s.after.camera.y - s.after.pivot.y;
        const float z = s.after.camera.z - s.after.pivot.z;
        s.distance = std::sqrt(x * x + y * y + z * z);
        s.held = update.mountPitchPreserved;
        s.suppressed = update.pitchSuppressed;
        trace.samples[trace.count++] = s;
        trace.pending = false;
    }

    // Keep pre-entry samples without continuously writing ordinary on-foot camera updates.
    struct PitchTraceSample
    {
        ULONGLONG time = 0;
        uintptr_t task = 0;
        int action = -1;
        int lookX = 0;
        int lookY = 0;
        int phase = 0;
        unsigned stages = 0;
        float entryPitch = 0.0f;
        float followPitch = 0.0f;
        float targetPitch = 0.0f;
        float input = 0.0f;
        float nativeInputDelta = 0.0f;
        float integratedPitch = 0.0f;
        float historyPitch = 0.0f;
        float finalPitch = 0.0f;
        float cameraZ = 0.0f;
        float pivotZ = 0.0f;
        bool held = false;
        bool suppressed = false;
    };
    struct PitchTraceState
    {
        uintptr_t camera = 0;
        uintptr_t ped = 0;
        uintptr_t task = 0;
        ULONGLONG lastSample = 0;
        ULONGLONG until = 0;
        bool active = false;
        unsigned next = 0;
        unsigned count = 0;
        PitchTraceSample current;
        PitchTraceSample previous[64]{};
    };
    static thread_local PitchTraceState pitchTrace;

    static void WritePitchTrace(const PitchTraceSample& s, bool beforeEntry)
    {
        static unsigned rows = 0;
        if (!log || rows >= 4000)
            return;
        std::fprintf(log, "pitchTrace t=%llu preEntry=%d task=%p action=%d phase=%d stages=%02X look=%d/%d entry=%.6f follow=%.6f target=%.6f input=%.6f nativeDelta=%.6f integrated=%.6f history=%.6f final=%.6f cameraZ=%.4f pivotZ=%.4f held=%d suppressed=%d\n",
            static_cast<unsigned long long>(s.time), beforeEntry, reinterpret_cast<void*>(s.task), s.action,
            s.phase, s.stages, s.lookX, s.lookY, s.entryPitch, s.followPitch, s.targetPitch, s.input,
            s.nativeInputDelta, s.integratedPitch, s.historyPitch, s.finalPitch, s.cameraZ, s.pivotZ,
            s.held, s.suppressed);
        if (++rows == 4000)
            std::fprintf(log, "Pitch trace sample limit reached.\n");
    }

    static void BeginPitchTrace(uintptr_t camera, uintptr_t ped, uintptr_t task, uintptr_t intelligence)
    {
        if (!IsHeightFollow() || !log)
            return;
        const auto now = GetTickCount64();
        if (pitchTrace.camera != camera || pitchTrace.ped != ped || now - pitchTrace.lastSample > 500)
            pitchTrace = {};
        pitchTrace.camera = camera;
        pitchTrace.ped = ped;
        const bool entered = task && task != pitchTrace.task;
        const bool changed = task != pitchTrace.task;
        pitchTrace.task = task;
        pitchTrace.active = changed || now - pitchTrace.lastSample >= 50;
        if (!pitchTrace.active)
            return;
        if (entered)
        {
            for (unsigned i = 0; i < pitchTrace.count; ++i)
                WritePitchTrace(pitchTrace.previous[(pitchTrace.next + 64 - pitchTrace.count + i) % 64], true);
            pitchTrace.until = now + 6000;
        }
        pitchTrace.lastSample = now;
        auto& s = pitchTrace.current;
        s = {};
        s.time = now;
        s.task = task;
        s.entryPitch = *reinterpret_cast<const float*>(camera + 772);
        const auto simple = intelligence
            ? FindActiveTaskByType(intelligence + 68, std::to_underlying(TaskID::TaskSimpleClimbLadder)) : 0;
        s.action = simple ? *reinterpret_cast<const int*>(simple + CTaskSimpleClimbLadder::nActionOffset) : -1;
        const auto control = CPlayerPed::GetControlFromPlayer ? CPlayerPed::GetControlFromPlayer(ped) : 0;
        if (control)
        {
            s.lookX = GetAxisValue(GetPedAimWeaponLeftRight(control));
            s.lookY = GetAxisValue(GetPedLookUpDown(control));
        }
    }

    static void EndPitchTrace(uintptr_t camera)
    {
        if (!pitchTrace.active || pitchTrace.camera != camera || !IsHeightFollow())
            return;
        auto& s = pitchTrace.current;
        s.stages |= 16;
        s.finalPitch = *reinterpret_cast<const float*>(camera + 772);
        s.cameraZ = *reinterpret_cast<const float*>(camera + 72);
        s.pivotZ = *reinterpret_cast<const float*>(camera + 88);
        s.phase = static_cast<int>(update.phase);
        s.held = update.mountPitchPreserved;
        s.suppressed = update.pitchSuppressed;
        pitchTrace.previous[pitchTrace.next] = s;
        pitchTrace.next = (pitchTrace.next + 1) % 64;
        pitchTrace.count = std::min(pitchTrace.count + 1, 64u);
        if (s.time <= pitchTrace.until)
        {
            WritePitchTrace(s, false);
            std::fflush(log);
        }
        pitchTrace.active = false;
    }

    static float WrapHeading(float heading)
    {
        return std::remainder(heading, 2.0f * pi);
    }

    static float SlowInOut(float value)
    {
        return 0.5f * (1.0f - std::cos(std::clamp(value, 0.0f, 1.0f) * pi));
    }

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
            if (query.size() != 1 || filter.size() != 1)
            {
                if (log)
                    std::fprintf(log, "rearOrbitProbe unavailable: query=%zu filter=%zu\n", query.size(), filter.size());
                return;
            }
            const auto manager = query.get_first<uint8_t>(-11);
            if (manager[0] != 0x8B || manager[1] != 0x0D)
                return;
            worldProbe = *query.get_first<uintptr_t*>(-9);
            TestLine = reinterpret_cast<decltype(TestLine)>(injector::GetBranchDestination(query.get_first(14)).as_int());
            FilterIntersections = reinterpret_cast<decltype(FilterIntersections)>(injector::GetBranchDestination(filter.get_first(6)).as_int());
            if (log)
                std::fprintf(log, "rearOrbitProbe ready: query=%p filter=%p manager=%p\n",
                    reinterpret_cast<void*>(TestLine), reinterpret_cast<void*>(FilterIntersections), static_cast<void*>(worldProbe));
        }

        static int IsBlocked(uintptr_t manager, uintptr_t ignore, int mask,
            const rage::Vector4& start, const rage::Vector4& end)
        {
            Segment line{ start, end };
            std::array<Intersection, 64> results{};
            for (auto& result : results)
                result.words[19] = 0xFFFF;
            // Match the native camera geometry mask/filter, without running its stateful collision solver.
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
            // Require a meaningful advantage; ties keep the normal, shorter turn.
            const bool chooseAlternate = preferred >= 0 && alternate >= 0 && alternate + 2 <= preferred;
            if (chooseAlternate)
            {
                alignment.rearOrbitStart = beta;
                alignment.rearOrbitDelta = preferNegative ? positiveDelta : negativeDelta;
                alignment.rearOrbitTime = 0.0f;
                alignment.rearOrbitActive = true;
                alignment.headingVelocity = 0.0f;
            }
            if (log)
                std::fprintf(log, "rearOrbitPlan t=%llu task=%p negative=%d positive=%d preferNegative=%d alternate=%d delta=%.6f mask=%d\n",
                    GetTickCount64(), reinterpret_cast<void*>(alignment.task), negative, positive, preferNegative,
                    chooseAlternate, chooseAlternate ? alignment.rearOrbitDelta : (preferNegative ? negativeDelta : positiveDelta), mask);
        }
    }

    static void ReadLookInput(uintptr_t control)
    {
        update.lookX = GetAxisValue(GetPedAimWeaponLeftRight(control));
        update.lookY = GetAxisValue(GetPedLookUpDown(control));
        if (IsEnabled())
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

    static void UpdateAlignment()
    {
        if (update.onLadder || update.approaching)
        {
            update.previousOrbitDistanceRatio = alignment.orbitDistanceRatio;
            update.collisionSampleValid = alignment.collisionSampleValid;
            update.collisionConstrained = alignment.collisionSampleValid
                && alignment.orbitDistanceRatio < fMinOrbitDistanceRatioForCustomPullAround;
            if (update.collisionConstrained)
            {
                alignment.inhibitCustomPullAround = true;
                alignment.inhibitPullAround = false;
            }
            else if (alignment.inhibitCustomPullAround)
            {
                // Once clear, wait for look input before trying a side view again to avoid oscillation.
                alignment.inhibitPullAround = !update.lookX && !update.lookY;
                alignment.inhibitCustomPullAround = alignment.inhibitPullAround;
            }
            alignment.orbitDistanceRatio = 1.0f;
            alignment.collisionSampleValid = false;
        }
        else
        {
            alignment.headingVelocity = 0.0f;
            alignment.lastHeadingDelta = 0.0f;
        }
        UpdateOrbitLimits();
        // V's input envelope uses real time; physical motion and the spring use game time.
        const auto now = GetTickCount64();
        const float seconds = alignment.lastInputTime ? static_cast<float>(now - alignment.lastInputTime) * 0.001f : 0.0f;
        alignment.lastInputTime = now;
        alignment.lookEnvelope.Update(update.lookX || update.lookY, update.controller, seconds);
        update.pullWeight = 1.0f - SlowInOut(alignment.lookEnvelope.level);
    }

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
            // V uses this single exponential for both critical and overdamped springs.
            const float exponent = -decay + std::sqrt(std::max(-frequencySquared, 0.0f));
            displacement = -target * std::exp(exponent * seconds);
            alignment.headingVelocity = exponent * displacement;
        }
        return target + displacement;
    }

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
        // Match IV's native exit thresholds, using its authored rung-cycle displacement.
        if (action == 1)
            return *reinterpret_cast<const uint8_t*>(task + 203) && startZ + step >= topExitZ - 0.75f;
        if (action == 2)
            return startZ - step <= bottomZ + 1.5f;
        const auto* matrix = *reinterpret_cast<const rage::Matrix44**>(alignment.ped + 0x20);
        return matrix && matrix->pos.z <= bottomZ + 1.5f + step;
    }

    static void LookBehindInput(SafetyHookContext& regs)
    {
        if (!IsEnabled())
            return;
        const auto ped = *reinterpret_cast<const uintptr_t*>(regs.ebp + 900);
        if (!ped || !CPlayer::getLocalPlayerPed || ped != CPlayer::getLocalPlayerPed())
            return;
        const auto intelligence = *reinterpret_cast<const uintptr_t*>(ped + 0x224);
        const auto task = intelligence
            ? FindActiveTaskByType(intelligence + 68, std::to_underlying(TaskID::TaskComplexClimbLadder)) : 0;
        if (!task)
            return;
        const auto simple = FindActiveTaskByType(intelligence + 68, std::to_underlying(TaskID::TaskSimpleClimbLadder));
        if (!simple && (alignment.task != task || !alignment.seenClimbTask)
            && !FindActiveTaskByType(intelligence + 68, std::to_underlying(TaskID::TaskSimpleRunNamedAnim)))
            return;

        // Ignore the command before CCamGame creates a reversed camera; native release handles an existing one.
        regs.eax &= ~uintptr_t{ 0xFF };
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
        // IV's ladder dispatcher reads the requested movement, not the controller's active child.
        child = *reinterpret_cast<const uintptr_t*>(child + 0x14);
        if (!child)
            return -1;
        const auto getMovementType = reinterpret_cast<int (__thiscall*)(uintptr_t)>((*reinterpret_cast<const uintptr_t**>(child))[3]);
        return getMovementType(child);
    }

    // This branch precedes both the look-behind early return and normal yaw processing.
    static void BeginUpdate(SafetyHookContext& regs)
    {
        update = {};
        cameraPed = regs.edi;
        if (!cameraPed)
        {
            FlushMountVisibility("noPed");
            HandoffTrace::Flush(true);
            mountVisibility.observedTask = 0;
            alignment = {};
            pitchTrace = {};
            mountPitchState = {};
            return;
        }
        const auto intelligence = *reinterpret_cast<const uintptr_t*>(cameraPed + 0x224);
        const auto task = intelligence
            ? FindActiveTaskByType(intelligence + 68, std::to_underlying(TaskID::TaskComplexClimbLadder)) : 0;
        BeginMountVisibility(regs.esi, cameraPed, task, intelligence);
        if (!IsEnabled())
        {
            alignment = {};
            pitchTrace = {};
            mountPitchState = {};
            return;
        }
        // Native ladder pitch starts during approach, before our mount camera policy applies.
        update.pitchCamera = regs.esi;
        update.pitchPed = cameraPed;
        update.hasLadderTask = task != 0;
        BeginPitchTrace(regs.esi, cameraPed, task, intelligence);
        const bool canFinishPitch = IsHeightFollow() && alignment.camera == regs.esi
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
            // Finish an existing pitch request without retaining ladder yaw rules or blocking look-behind.
            if (regs.eflags & 0x40)
            {
                update.camera = regs.esi;
                update.phase = Phase::Dismount;
                ReadLookInput(control);
            }
            else
                alignment = {};
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
            // IV slides to the entry, achieves its heading, then starts the mount.
            const bool continuing = alignment.task == task && alignment.ped == cameraPed
                && alignment.camera == regs.esi && alignment.approaching;
            const bool navigating = CTaskComplexClimbLadder::RearApproach::IsActiveForPed(cameraPed);
            const int entryTaskType = continuing && !navigating ? GetEntryAlignmentTaskType(task) : -1;
            const bool cancelled = (*reinterpret_cast<const uint32_t*>(task + 12) & 2) != 0;
            approaching = IsAutoClimbEnabled() && !cancelled && (navigating
                || (continuing && (entryTaskType == std::to_underlying(TaskID::TaskSimpleSlideToCoord)
                    || entryTaskType == std::to_underlying(TaskID::TaskSimpleMoveAchieveHeading))));
            if (log && continuing && !navigating)
                std::fprintf(log, "rearCameraHandoff t=%llu task=%p entryTask=%d cancelled=%d accepted=%d\n",
                    GetTickCount64(), reinterpret_cast<void*>(task), entryTaskType, cancelled, approaching);
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
        if (IsHeightFollow() && update.phase == Phase::Climb && IsFinalStep(simple, action))
        {
            update.finalStep = true;
            update.phase = Phase::Dismount;
        }
        if (update.phase == Phase::Dismount && simple)
            alignment.exitAtTop = action == 1 || action == 3;
        update.topDismount = update.phase == Phase::Dismount && alignment.exitAtTop;
        ReadLookInput(control);
        OrbitProbe::SelectRearArc(regs.esi);
    }

    // Run after native stick input is integrated, before camera vectors and collision are built.
    static void OrbitHeading(SafetyHookContext& regs)
    {
        if (!IsEnabled() || update.camera != regs.esi || (!update.onLadder && !update.approaching) || !update.canAlign)
            return;
        auto& beta = *reinterpret_cast<float*>(regs.esi + 776);
        auto& velocity = *reinterpret_cast<float*>(regs.esi + 780);
        if (!std::isfinite(beta) || !std::isfinite(velocity))
            return;
        if (alignment.rearOrbitActive && (!update.approaching || update.lookX || update.lookY))
            alignment.rearOrbitActive = false;
        if (update.approaching && update.collisionRecovery && !alignment.rearEntryCut
            && update.previousOrbitDistanceRatio < fRearEntryCutDistanceRatio)
        {
            const auto& target = *reinterpret_cast<const rage::Vector4*>(alignment.task + 64);
            if (std::isfinite(target.x) && std::isfinite(target.y) && std::isfinite(target.z))
            {
                alignment.rearEntryTarget = target;
                alignment.rearEntryCut = true;
                alignment.rearOrbitActive = false;
                alignment.lastHeadingDelta = WrapHeading(alignment.heading - beta);
                beta = WrapHeading(alignment.heading);
                velocity = 0.0f;
                alignment.headingVelocity = 0.0f;
                if (log)
                    std::fprintf(log, "rearCameraCut t=%llu task=%p ratio=%.4f yawStep=%.6f target=%.4f/%.4f\n",
                        GetTickCount64(), reinterpret_cast<void*>(alignment.task), update.previousOrbitDistanceRatio,
                        alignment.lastHeadingDelta, target.x, target.y);
                return;
            }
        }
        if (alignment.rearOrbitActive)
        {
            if (update.moveSpeed > 0.0f)
                alignment.rearOrbitTime = std::min(fLimitBlendTime, alignment.rearOrbitTime + update.seconds);
            const float planned = WrapHeading(alignment.rearOrbitStart
                + alignment.rearOrbitDelta * SlowInOut(alignment.rearOrbitTime / fLimitBlendTime));
            alignment.lastHeadingDelta = WrapHeading(planned - beta);
            beta = planned;
            velocity = 0.0f;
            alignment.headingVelocity = 0.0f;
            alignment.rearOrbitActive = alignment.rearOrbitTime < fLimitBlendTime;
            return;
        }
        const float blend = update.collisionRecovery ? update.orbitBlend : update.blend;
        alignment.lastHeadingDelta = 0.0f;
        const bool transition = update.phase == Phase::Mount || update.phase == Phase::Dismount;
        if (IsHeightFollow() && update.onLadder && !transition && !alignment.inhibitCustomPullAround)
            update.desiredHeadingOffset = WrapHeading(beta - alignment.heading) >= 0.0f
                ? fDesiredPullAroundHeadingOffset : -fDesiredPullAroundHeadingOffset;
        const bool shouldPull = (update.collisionRecovery || (IsHeightFollow() && update.onLadder
            && update.action != CTaskSimpleClimbLadder::nIdleAction && (transition || !alignment.inhibitPullAround)))
            && (!update.collisionConstrained || (!update.lookX && !update.lookY));
        const float moveSpeed = update.moveSpeed;
        if (blend > 0.0f && shouldPull && moveSpeed > 0.0f && update.pullWeight > 0.0f)
        {
            const float error = WrapHeading(alignment.heading + update.desiredHeadingOffset - beta);
            const float maxSpeed = 6.0f + (fHeadingPullAroundMaxMoveSpeed - 6.0f) * blend;
            const float speedScale = SlowInOut(moveSpeed / maxSpeed) * update.pullWeight;
            alignment.lastHeadingDelta = PullAround(error, speedScale, update.seconds, blend) * blend;
            beta = WrapHeading(beta + alignment.lastHeadingDelta);
        }
        else
            // Idle and collision holds must not continue a turn on stored spring velocity.
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
            ++alignment.limitHits;
        }
    }

    static float OrbitDistance(uintptr_t camera)
    {
        const auto* position = reinterpret_cast<const float*>(camera + 64);
        const auto* pivot = reinterpret_cast<const float*>(camera + 80);
        const float x = position[0] - pivot[0], y = position[1] - pivot[1], z = position[2] - pivot[2];
        return std::sqrt(x * x + y * y + z * z);
    }

    static uintptr_t __fastcall LadderPitch(uintptr_t camera, void* edx, uintptr_t ped, uintptr_t intelligence,
        float* vertical, float* horizontal)
    {
        const float input = *vertical;
        const auto result = shLadderPitch.unsafe_fastcall<uintptr_t>(camera, edx, ped, intelligence, vertical, horizontal);
        if (pitchTrace.active && pitchTrace.camera == camera)
        {
            pitchTrace.current.stages |= 4;
            pitchTrace.current.input = input;
            pitchTrace.current.nativeInputDelta = *vertical - input;
        }
        if (IsHeightFollow() && update.pitchCamera == camera && update.pitchPed == ped)
        {
            update.nativePitchSample = true;
            update.nativePitchInputDelta = *vertical - input;
            if (update.hasLadderTask || (update.camera == camera && alignment.pitchLevelActive))
            {
                // Keep native bookkeeping, but discard its added pitch input, not the player's input.
                *vertical = input;
                update.pitchSuppressed = true;
            }
        }
        return result;
    }

    static void VerticalFollow(SafetyHookContext& regs)
    {
        if (pitchTrace.active && pitchTrace.camera == regs.esi)
        {
            pitchTrace.current.stages |= 1;
            pitchTrace.current.followPitch = *reinterpret_cast<const float*>(regs.esi + 772);
        }
        if (!IsHeightFollow() || update.camera != regs.esi
            || alignment.ped != regs.edi)
        {
            verticalFollow = {};
            return;
        }

        // The displaced subtraction uses this processed target and the previous base position to derive pitch.
        const float targetZ = regs.xmm0.f32[0];
        auto& baseZ = *reinterpret_cast<float*>(regs.esi + 568);
        auto& distanceBaseZ = *reinterpret_cast<float*>(regs.esi + 584);
        const bool reset = (*reinterpret_cast<const uint8_t*>(regs.esi + 908) & 8) != 0
            || (*reinterpret_cast<const uint8_t*>(regs.esi + 909) & 0x18) != 0;
        if (reset || !std::isfinite(targetZ) || !std::isfinite(baseZ) || !std::isfinite(distanceBaseZ))
        {
            verticalFollow = {};
            alignment.pitchLevelActive = false;
            return;
        }
        update.targetZ = targetZ;
        // ResetStatics starts with a level front; Process then subtracts this frame's GetBaseAlpha result.
        update.neutralPitch = -*reinterpret_cast<const float*>(regs.esp + 0x50);
        update.previousPitch = *reinterpret_cast<const float*>(regs.esi + 772);
        update.pitchReferenceValid = std::isfinite(update.neutralPitch) && std::isfinite(update.previousPitch);
        if (!update.onLadder)
        {
            // The pitch request can outlive the ladder; vertical translation cannot.
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
        update.nativeCarryZ = nativeShift;
        // Reseed after discontinuities instead of carrying a camera cut or teleport into the orbit.
        if (std::abs(targetDelta) > 2.0f || std::abs(nativeShift) > 2.0f
            || std::abs(nativeDistanceShift - nativeShift) > 0.01f)
            return;

        // OnPhysicalProcess may already have translated both histories with a moving support.
        update.verticalCarryZ = targetDelta - nativeShift;
        baseZ += update.verticalCarryZ;
        distanceBaseZ += targetDelta - nativeDistanceShift;
        update.verticalCarried = true;
    }

    static void ApplyLadderPitchLimits(SafetyHookContext& regs)
    {
        if (!IsHeightFollow() || update.camera != regs.esi
            || !update.onLadder || !update.canAlign || !update.pitchReferenceValid)
            return;
        // Both native target limiting and the later rendered-pitch clamp use these local bounds.
        auto& downLimit = *reinterpret_cast<float*>(regs.esp + 0x6C);
        auto& upLimit = *reinterpret_cast<float*>(regs.esp + 0x94);
        if (!std::isfinite(downLimit) || !std::isfinite(upLimit) || downLimit < 0.0f || upLimit < 0.0f)
            return;
        downLimit += (fOrbitPitchLimit - downLimit) * update.blend;
        upLimit += (fOrbitPitchLimit - upLimit) * update.blend;
        update.pitchLimitsApplied = true;
    }

    static void MountPitch(SafetyHookContext& regs)
    {
        ApplyLadderPitchLimits(regs);
        if (pitchTrace.active && pitchTrace.camera == regs.esi)
        {
            pitchTrace.current.stages |= 2;
            pitchTrace.current.targetPitch = *reinterpret_cast<const float*>(regs.esp + 0x24)
                - *reinterpret_cast<const float*>(regs.esp + 0x50);
        }
        if (!IsHeightFollow() || update.camera != regs.esi
            || !update.onLadder || update.phase != Phase::Mount || !update.canAlign || !update.pitchReferenceValid)
        {
            mountPitchState = {};
            return;
        }

        // Controller look input is carried in the target history, ahead of the rendered pitch.
        // Retain that history instead of canceling input by reseeding from the rendered angle.
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
        update.mountPitchTargetDelta = desired - target;
        update.mountPitchPreserved = true;
        target = desired;
    }

    static void PitchLevel(SafetyHookContext& regs)
    {
        if (pitchTrace.active && pitchTrace.camera == regs.esi)
        {
            pitchTrace.current.stages |= 8;
            pitchTrace.current.integratedPitch = *reinterpret_cast<const float*>(regs.esi + 772);
            pitchTrace.current.historyPitch = *reinterpret_cast<const float*>(regs.esp + 0x1C);
        }
        mountPitchState = {};
        if (IsHeightFollow() && update.camera == regs.esi && update.mountPitchPreserved)
        {
            // Mirror Process's input path: direct input updates rendered alpha; controller input updates history.
            const bool directInput = !regs.ebx || *reinterpret_cast<const uint8_t*>(regs.ebx + 0x3289) != 0;
            const float target = directInput ? *reinterpret_cast<const float*>(regs.esi + 772)
                : *reinterpret_cast<const float*>(regs.esp + 0x1C);
            if (std::isfinite(target))
                mountPitchState = { regs.esi, alignment.ped, alignment.task, target, GetTickCount64() };
        }
        if (!IsHeightFollow() || update.camera != regs.esi)
        {
            alignment.pitchLevelActive = false;
            return;
        }
        if (update.onLadder && update.phase == Phase::Dismount && !alignment.pitchLevelRequested)
        {
            // One request per traversal: canceling with look input must not restart it on the next exit subtask.
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
        const float leveled = update.previousPitch + (update.neutralPitch - update.previousPitch) * blend;
        update.pitchLevelDelta = leveled - pitch;
        update.pitchLevelApplied = true;
        pitch = leveled;
        *reinterpret_cast<float*>(regs.esi + 784) = 0.0f;
        // Update the base-history pitch too, or native follow would recover the old angle on the next frame.
        *reinterpret_cast<float*>(regs.esp + 0x1C) = leveled;
        if (std::abs(update.neutralPitch - leveled) <= fPitchLevelTolerance)
            alignment.pitchLevelActive = false;
        // The displaced native path still clamps pitch and rebuilds vectors before collision.
    }

    static void BeforeCollision(SafetyHookContext& regs)
    {
        if (IsEnabled() && update.camera == regs.esi && update.canAlign && alignment.rearEntryCut
            && (update.approaching || update.phase == Phase::Mount) && regs.ebx == regs.esi + 64 && regs.eax)
        {
            // Frame the destination during a constrained rear mount, preserving native height and pitch.
            // EAX is the native collision pivot argument; EBX points at the desired camera position.
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
                update.rearEntryFramed = true;
            }
        }
        if (mountVisibility.active && mountVisibility.pending && mountVisibility.camera == regs.esi
            && mountVisibility.ped == cameraPed)
        {
            auto& s = mountVisibility.current;
            std::memcpy(&s.cameraBeforeCollision, reinterpret_cast<const void*>(regs.esi + 64), 12);
            std::memcpy(&s.pivotBeforeCollision, reinterpret_cast<const void*>(regs.esi + 80), 12);
            s.collisionEntry = true;
        }
        verticalFollow = {};
        if (IsEnabled() && update.camera == regs.esi && (update.onLadder || update.approaching))
            update.desiredDistance = OrbitDistance(regs.esi);
        if (IsHeightFollow() && update.camera == regs.esi)
        {
            update.cameraZBeforeCollision = *reinterpret_cast<const float*>(regs.esi + 72);
            update.pivotZBeforeCollision = *reinterpret_cast<const float*>(regs.esi + 88);
            if (update.onLadder && update.verticalSample)
            {
                // Native input has rebuilt both base positions; collision has not modified the rendered camera yet.
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
        EndMountVisibility(regs.esi);
        EndPitchTrace(regs.esi);
        if (!IsEnabled() || update.camera != regs.esi)
            return;
        const float distance = OrbitDistance(regs.esi);
        const bool valid = std::isfinite(distance) && std::isfinite(update.desiredDistance) && update.desiredDistance > 0.01f;
        if (valid)
            update.orbitDistanceRatio = std::clamp(distance / update.desiredDistance, 0.0f, 1.0f);
        if (update.onLadder || update.approaching)
        {
            alignment.orbitDistanceRatio = update.orbitDistanceRatio;
            alignment.collisionSampleValid = valid;
        }
        if (log && update.logFrame)
            std::fprintf(log, "obstruction t=%llu phase=%d previousValid=%d currentValid=%d previousRatio=%.4f orbitRatio=%.4f constrained=%d recovery=%d hold=%d speed=%.4f autoYaw=%.6f look=%d/%d rearCut=%d rearFramed=%d\n",
                static_cast<unsigned long long>(update.sampleTime), static_cast<int>(update.phase),
                update.collisionSampleValid, valid, update.previousOrbitDistanceRatio, update.orbitDistanceRatio,
                update.collisionConstrained, update.collisionRecovery, alignment.inhibitPullAround,
                update.moveSpeed, alignment.lastHeadingDelta, update.lookX, update.lookY,
                alignment.rearEntryCut, update.rearEntryFramed);
        if (!IsHeightFollow())
        {
            if (log && update.logFrame)
                std::fflush(log);
            update.logFrame = false;
            return;
        }
        if (log && update.logFrame)
        {
            std::fprintf(log, "frame t=%llu cameraZ=%.4f pivotZ=%.4f cameraZPre=%.4f pivotZPre=%.4f pitch=%.6f yaw=%.6f autoYaw=%.6f orbitRatio=%.4f targetZ=%.4f targetDeltaZ=%.6f nativeCarryZ=%.6f carryZ=%.6f carried=%d nativePitchSample=%d nativePitchInputDelta=%.6f pitchSuppressed=%d mountPitchPreserved=%d mountPitchTargetDelta=%.6f pitchOverride=%d neutralPitch=%.6f pitchLevelDelta=%.6f pitchLevelApplied=%d pitchLevelActive=%d\n",
                static_cast<unsigned long long>(update.sampleTime),
                *reinterpret_cast<const float*>(regs.esi + 72), *reinterpret_cast<const float*>(regs.esi + 88),
                update.cameraZBeforeCollision, update.pivotZBeforeCollision,
                *reinterpret_cast<const float*>(regs.esi + 772), *reinterpret_cast<const float*>(regs.esi + 776),
                alignment.lastHeadingDelta, update.orbitDistanceRatio, update.targetZ, update.targetDeltaZ,
                update.nativeCarryZ, update.verticalCarryZ, update.verticalCarried, update.nativePitchSample, update.nativePitchInputDelta,
                update.pitchSuppressed, update.mountPitchPreserved, update.mountPitchTargetDelta,
                (*reinterpret_cast<const uint8_t*>(regs.esi + 910) & 2) != 0,
                update.neutralPitch, update.pitchLevelDelta, update.pitchLevelApplied, alignment.pitchLevelActive);
            std::fprintf(log, "policy t=%llu targetYawOffset=%.6f previousOrbitRatio=%.4f sideBlocked=%d collisionHold=%d pitchLimits=%d\n",
                static_cast<unsigned long long>(update.sampleTime), update.desiredHeadingOffset,
                update.previousOrbitDistanceRatio, alignment.inhibitCustomPullAround, alignment.inhibitPullAround,
                update.pitchLimitsApplied);
            std::fflush(log);
            update.logFrame = false;
        }
    }

    struct HeadingLimitContext
    {
        uintptr_t camera = 0;
        bool bypass = false;
        bool applied = false;
    };
    static thread_local HeadingLimitContext headingLimit;

    static void OpenLog()
    {
        HMODULE module = nullptr;
        wchar_t path[MAX_PATH]{};
        if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            reinterpret_cast<LPCWSTR>(&OpenLog), &module))
            return;
        const auto length = GetModuleFileNameW(module, path, MAX_PATH);
        if (!length || length >= MAX_PATH)
            return;
        const auto filename = std::filesystem::path(path).parent_path() / L"FusionFix-LadderCamera.log";
        _wfopen_s(&log, filename.c_str(), L"w");
        if (log)
        {
            std::fprintf(log, "Ladder camera styles test 32: LadderCameraStyle=%d\n"
                "Rear-entry orbit previews compare both yaw arcs with native camera-filtered world rays before turning. Six samples per arc, current/destination sight lines and camera chords; alternate requires two fewer blocked tests. Preview tuning is local, not V retail. rearOrbitPlan reports scores and selected delta. Native collision and severe-compression fallback remain active.\n"
                "Rear-entry handoff reads the ladder-owned movement request inside CTaskComplexControlMovement, as IV's dispatcher does. rearCameraHandoff reports the resolved entry task and acceptance; cancelled/unrelated tasks do not retain approach camera state.\n"
                "Rear-entry continuity includes IV's slide -> achieve-heading -> mount transition. Severe compression below 0.50 requests one rear-yaw cut and destination XY framing through approach/mount; pitch, height and native collision remain active. rearCameraCut records every cut; rearFramed identifies destination framing. The 0.50 threshold is local test tuning, not a V value.\n"
                "Styles 1 and 2: native collision compression below 0.85 requests rear-heading recovery during approach, mount, traversal and dismount. Recovery yields to look input and pauses with motion; a cleared view does not resume the blocked side automatically. obstruction rows report the shared policy.\n"
                "Styles 1 and 2: ladder-relative yaw limits begin during automatic rear navigation and continue through slide alignment into mounting. Phase 4=approach; approachLimits/orbitBlend identify the early envelope. No movement-input override or rear yaw-cutoff bypass.\n"
                "Entry ownership diagnostics only: no new entry rejection. entryControl rows snapshot native movement task, on-foot child, cover input/task and ragdoll before automatic entry. -1 means unavailable.\n"
                "Rear bottom-entry assist: native navigation only, 4 seconds/5 metres maximum, normal side validation before alignment. rearApproach rows record outcomes.\n"
                "Top-mount repair does not require logging, a frame counter, or the optional pre-render trace hook.\n"
                "Camera policy is independent of the Controls-menu Auto Climb setting. Compare manual and automatic mounts with the same INI style.\n"
                "Handoff diagnostics: offset/root/preRender checkpoints, local-player top entry only, 13 engine frames. Raw reads, no extra bone/skeleton updates or viewport queries. Correlate frame/QPC across buffered capture groups.\n"
                "Styles 1 and 2: top-mount bounds refresh after native local-player root rebase; look-behind suppressed during mount/climb/dismount, not approach.\n"
                "boundsRefreshAfter refresh: 0=disabled 1=applied 2=bindingsUnavailable 3=noRebase 4=instanceMismatch 5=unsupportedState 6=missingGeometry 7=invalidLevelEntry. Other stages use -1.\n"
                "mountFrame/mountVisibility: every follow-camera update for 6 seconds after complex ladder entry, buffered until window end (4096 records maximum). Wait 7 seconds after entry before quitting.\n"
                "visibleBit is PC IS_CHAR_VISIBLE (entity+24 bit 20); onScreen is the native bounding-sphere viewport test, NOT occlusion or draw submission. -1=unavailable. Sphere getters verified at vtable+54/+58.\n"
                "Snapshots are camera-entry and after follow-camera collision, NOT final rendering. post=0 means post-collision was not observed. Buffered rows are grouped by capture, not globally time-ordered.\n"
                "0=Native (default), 1=Pitch Follow, 2=Height Follow. Invalid values use Native.\n"
                "Styles 1 and 2: yaw=-90/+80 deg blended on entry. Style 1 retains native pitch/height and yaw follow with traversal cutoff bypass; no forced mount turn or side pull.\n"
                "Style 2: pitch=+/-75 deg blended on entry. Rear target for mounts/dismounts; nearest +/-75 deg side while traversing. No idle turn.\n"
                "Style 2 collision below 0.85 orbit ratio selects rear; once clear, traversal pull-around holds until look input. Native collision is retained in all styles.\n"
                "Style 2 suppresses native ladder-added pitch input from complex ladder task entry, including approach/alignment, or while leveling after exit. Mount retains native input-adjusted pitch target across frames.\n"
                "Pitch trace: 50ms samples, 64-sample pre-entry buffer and 6 seconds after complex ladder entry; capped at 4000 rows. Pre-entry rows may repeat previous samples.\n"
                "Trace stages: 01=before vertical carry, 02=before mount target override, 04=native ladder input, 08=before dismount leveling, 10=after collision. Missing stage fields are unavailable, not measured zero.\n"
                "Style 2 vertical height following and dismount pitch leveling unchanged. First frame/cuts reseed without translation.\n"
                "Style 2 dismount pitch leveling: 0.1 residual-error blend per 30 Hz reference step, time-normalized; target=-native GetBaseAlpha, tolerance=1 deg. Once requested, continues through idle and exit; look input/camera reset cancels.\n"
                "Style 2 heading spring k=4 damping=0.95; full response at 1 m/s. Shared yaw-limit attack=750ms. Style 2 phase 3 includes IV native final-step region.\n"
                "No ladder yaw steering/limits after exit. Native collision retained. Look envelope 500ms/1000ms/6000ms.\n"
                "Vertical telemetry: target delta, native translation, added carry, pre/post collision heights, native pitch input delta before suppression, suppression flag, mount target replacement and native override. Input delta is not a rendered pitch angle.\n"
                "Samples at 250ms while on a ladder plus transitions; maximum 2400 rows. Angles in radians.\n", std::to_underlying(cameraStyle));
            std::fflush(log);
        }
    }

    static void HeadingLimit(SafetyHookContext& regs)
    {
        if (headingLimit.bypass && regs.ecx == headingLimit.camera)
        {
            // JA follows the native angle comparison. Leave the subsequent force test intact.
            headingLimit.applied = (regs.eflags & 0x41) != 0;
            regs.eflags &= ~uintptr_t(0x41); // Clear CF and ZF to take JA.
        }
    }

    static void __fastcall TargetBetaCalc(uintptr_t camera, void* edx, float* beta, float target,
        float force, float rate, float cap, uintptr_t ped, uintptr_t support, float motion,
        uint32_t mode, float timestep)
    {
        cameraPed = ped;
        const bool enabled = IsEnabled();
        const auto intelligence = ped ? *reinterpret_cast<const uintptr_t*>(ped + 0x224) : 0;
        const auto task = intelligence
            ? FindActiveTaskByType(intelligence + 68, std::to_underlying(TaskID::TaskSimpleClimbLadder)) : 0;
        const int action = task ? *reinterpret_cast<const int*>(task + CTaskSimpleClimbLadder::nActionOffset) : -1;
        const bool moving = action == 1 || action == 2 || action == 7;
        const bool rearApproach = enabled && !task && !update.onLadder
            && CTaskComplexClimbLadder::RearApproach::IsActiveForPed(ped);
        const auto flags = *reinterpret_cast<const uint8_t*>(camera + 910);
        const auto forceFlags = *reinterpret_cast<const uint8_t*>(camera + 909);
        const auto control = (task || rearApproach || update.camera == camera) && ped && CPlayerPed::GetControlFromPlayer
            ? CPlayerPed::GetControlFromPlayer(ped) : 0;
        const int lookX = control ? GetAxisValue(GetPedAimWeaponLeftRight(control)) : 0;
        const int lookY = control ? GetAxisValue(GetPedLookUpDown(control)) : 0;
        const float before = *beta;
        const float nativeHeading = *reinterpret_cast<const float*>(camera + 776);
        if (update.camera == camera && enabled)
        {
            update.canAlign = control || update.phase == Phase::Mount || update.phase == Phase::Dismount;
            update.canAlign = update.canAlign && !(flags & 0x80) && !(forceFlags & 4) && force == 0.0f
                && std::isfinite(before) && std::isfinite(timestep) && timestep > 0.0f;
            if (update.canAlign)
            {
                // IV passes camera time in 50 Hz units, not seconds.
                update.seconds = timestep * 0.02f;
                const auto* matrix = *reinterpret_cast<const rage::Matrix44**>(ped + 0x20);
                if ((update.onLadder || update.approaching) && matrix && GetRootBone)
                {
                    // Navigation consumes stick input; measure motion, including climb animation translation.
                    alignas(16) rage::Vector4 root = matrix->pos;
                    if (update.onLadder)
                        GetRootBone(ped, &root);
                    const bool valid = std::isfinite(root.x) && std::isfinite(root.y) && std::isfinite(root.z);
                    update.rootValid = update.onLadder && valid;
                    root.w = 0.0f; // The native routine only defines XYZ.
                    update.rootZ = root.z;
                    if (valid && alignment.hasPreviousPosition)
                    {
                        const float x = root.x - alignment.previousPosition.x;
                        const float y = root.y - alignment.previousPosition.y;
                        const float z = root.z - alignment.previousPosition.z;
                        const float distance = std::sqrt(x * x + y * y + z * z);
                        // IV's camera root includes animation translation that its entity origin can omit.
                        if (std::isfinite(distance) && distance < 2.0f)
                        {
                            update.moveSpeed = distance / update.seconds;
                            update.verticalSpeed = z / update.seconds;
                        }
                        else
                            alignment.headingVelocity = 0.0f;
                    }
                    alignment.previousPosition = root;
                    alignment.hasPreviousPosition = valid;
                }
                else
                    alignment.hasPreviousPosition = false;
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
                alignment.lastHeadingDelta = 0.0f;
                alignment.headingVelocity = 0.0f;
            }
        }
        const bool bypass = cameraStyle == CameraStyle::PitchFollow && enabled && moving && control && !lookX && !lookY
            && !update.lookX && !update.lookY
            && !(flags & 0x80) && !(forceFlags & 4) && force == 0.0f
            && std::isfinite(before) && std::isfinite(target) && std::isfinite(motion)
            && std::isfinite(timestep) && timestep > 0.0f;
        const auto previous = headingLimit;
        headingLimit = { camera, bypass, false };
        shTargetBetaCalc.unsafe_fastcall<void>(camera, edx, beta, target, force, rate, cap, ped, support, motion, mode, timestep);
        const bool holdCollisionHeading = ((update.collisionSampleValid && alignment.inhibitCustomPullAround
            && (update.onLadder || update.approaching)) || ((alignment.rearEntryCut || alignment.rearOrbitActive) && update.approaching))
            && !update.lookX && !update.lookY;
        if (update.camera == camera && update.canAlign && enabled
            && ((IsHeightFollow() && update.onLadder) || holdCollisionHeading))
            *beta = nativeHeading; // Apply ladder pull-around after native look input, without doubling native follow.
        const bool applied = headingLimit.applied;
        headingLimit = previous;

        static int lastAction = -2;
        static Phase lastPhase = Phase::None;
        static bool lastEnabled = false;
        static bool lastRearApproach = false;
        static ULONGLONG lastSample = 0;
        static unsigned rows = 0;
        static unsigned bypassCount = 0;
        bypassCount += applied;
        const auto now = GetTickCount64();
        const bool transition = action != lastAction || enabled != lastEnabled || update.phase != lastPhase
            || rearApproach != lastRearApproach;
        lastAction = action;
        lastPhase = update.phase;
        lastEnabled = enabled;
        lastRearApproach = rearApproach;
        if (!log || rows >= 2400 || (!transition && ((!task && !rearApproach && update.phase == Phase::None) || now - lastSample < 250)))
            return;
        lastSample = now;
        update.logFrame = enabled && update.camera == camera;
        update.sampleTime = now;
        const auto* matrix = ped ? *reinterpret_cast<const rage::Matrix44**>(ped + 0x20) : nullptr;
        const float pedHeading = matrix ? std::atan2(-matrix->up.x, matrix->up.y) : 0.0f;
        const float ladderHeading = task ? *reinterpret_cast<const float*>(task + 80)
            : update.camera == camera ? alignment.heading : 0.0f;
        const float retained = *reinterpret_cast<const float*>(camera + 744);
        const float candidate = *reinterpret_cast<const float*>(camera + 752);
        std::fprintf(log, "t=%llu action=%d enabled=%d eligible=%d applied=%d hits=%u beta=%.6f target=%.6f diff=%.6f output=%.6f delta=%.6f pedHeading=%.6f ladderHeading=%.6f candidate=%.6f retained=%.6f motion=%.6f force=%.3f rate=%.6f cap=%.6f dt=%.6f mode=%u flags=%02X/%02X support=%p look=%d/%d phase=%d topDismount=%d limitHits=%u moveSpeed=%.4f vz=%.4f finalStep=%d previousAutoYaw=%.6f rearApproach=%d approachLook=%d/%d approachLimits=%d orbitBlend=%.5f\n",
            static_cast<unsigned long long>(now), action, enabled, bypass, applied, bypassCount,
            before, target, target - before, *beta, *beta - before, pedHeading, ladderHeading, candidate, retained,
            motion, force, rate, cap, timestep, mode & 0xFF, forceFlags, flags, reinterpret_cast<void*>(support), update.lookX, update.lookY,
            static_cast<int>(update.phase), update.topDismount,
            alignment.limitHits, update.moveSpeed, update.verticalSpeed, update.finalStep, alignment.lastHeadingDelta,
            rearApproach, lookX, lookY, update.approaching, update.orbitBlend);
        if (update.logFrame)
        {
            std::fprintf(log, "tracking t=%llu onLadder=%d rootValid=%d entityZ=%.4f rootZ=%.4f smoothTargetZ=%.4f smoothRootZ=%.4f pitch=%.6f ladderTimer=%d alignment=%.5f inputEnvelope=%.5f pullWeight=%.5f controller=%d finishingPitch=%d\n",
                static_cast<unsigned long long>(now), update.onLadder, update.rootValid, matrix ? matrix->pos.z : 0.0f,
                update.rootZ, *reinterpret_cast<const float*>(camera + 680), *reinterpret_cast<const float*>(camera + 696),
                *reinterpret_cast<const float*>(camera + 772), *reinterpret_cast<const int*>(camera + 828),
                update.blend, alignment.lookEnvelope.level, update.pullWeight, update.controller, !update.onLadder && alignment.pitchLevelActive);
        }
        if (++rows == 2400)
            std::fprintf(log, "Sample limit reached.\n");
        std::fflush(log);
    }

    static void ResetHooks()
    {
        shLadderPitch.reset();
        shVerticalFollow.reset();
        shMountPitch.reset();
        mountPitchState = {};
        pitchTrace = {};
        shPitchLevel.reset();
        verticalFollow = {};
        shAfterCollision.reset();
        shBeforeCollision.reset();
        shOrbitHeading.reset();
        shLookBehindInput.reset();
        shBeginUpdate.reset();
        shTargetBetaCalc.reset();
        shHeadingLimit.reset();
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

    namespace RearApproach
    {
        static constexpr uint32_t nTimeoutMs = 4000;
        static constexpr float fMaxDistance = 5.0f;
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
            bool routeLogged = false;
            uint32_t lastRouteTrace = 0;
            int routeIndex = -1, routeCount = 0;
            float remaining = -1.0f, legacyLength = -1.0f;
            const char* cancelReason = nullptr;
        };
        static Candidate pending, blocked;
        static Approach active;

        static bool IsActiveForPed(uintptr_t ped)
        {
            return ped && active.task && active.candidate.ped == ped && !active.cancelReason && IsAutoClimbEnabled();
        }

        static void Trace(const char* reason, float routeLength = -1.0f)
        {
            if (!LadderCamera::log)
                return;
            std::fprintf(LadderCamera::log, "rearApproach time=%u wall=%llu task=%p ladder=%p index=%d event=%s travelled=%.3f route=%.3f cursor=%d count=%d remaining=%.3f legacyLength=%.3f age=%u\n",
                static_cast<uint32_t>(*CTimer::m_snTimeInMilliseconds), GetTickCount64(), reinterpret_cast<void*>(active.task),
                reinterpret_cast<void*>(active.candidate.ladder), active.candidate.index, reason, active.travelled, routeLength,
                active.routeIndex, active.routeCount, active.remaining, active.legacyLength,
                static_cast<uint32_t>(*CTimer::m_snTimeInMilliseconds) - active.candidate.time);
            std::fflush(LadderCamera::log);
        }

        static void Finish(const char* reason, bool retryBlocked = true)
        {
            Trace(reason);
            if (retryBlocked)
                blocked = active.candidate;
            active = {};
        }

        static bool Matches(uintptr_t task, uintptr_t ped)
        {
            return active.task == task && active.candidate.ped == ped;
        }

        // Capture native cancellation inputs before ControlSubTask can release its movement task.
        struct NativeState
        {
            rage::Vector4 position{}, destination{};
            uint32_t flags = 0;
            int subTask = -1, stuckCount = -1, inputX = 0, inputY = 0;
            float timer = 0.0f, cameraHeading = 0.0f;
        };

        static NativeState ReadNativeState(uintptr_t task, uintptr_t ped)
        {
            NativeState state;
            if (const auto matrix = *reinterpret_cast<const rage::Matrix44**>(ped + 0x20))
                state.position = matrix->pos;
            state.destination = *reinterpret_cast<const rage::Vector4*>(task + 0x40);
            state.flags = *reinterpret_cast<const uint32_t*>(task + 0x0C);
            state.timer = *reinterpret_cast<const float*>(task + 0x98);
            if (auto subTask = *reinterpret_cast<const uintptr_t*>(task + 8))
            {
                const auto getType = reinterpret_cast<int (__thiscall*)(uintptr_t)>((*reinterpret_cast<uintptr_t**>(subTask))[3]);
                state.subTask = getType(subTask);
            }
            if (const auto intelligence = *reinterpret_cast<const uintptr_t*>(ped + 0x224))
                state.stuckCount = *reinterpret_cast<const int*>(intelligence + 0x264);
            if (onFootControl && GetAxisValue && GetPedWalkLeftRight && GetPedWalkUpDown)
            {
                state.inputX = GetAxisValue(GetPedWalkLeftRight(onFootControl));
                state.inputY = GetAxisValue(GetPedWalkUpDown(onFootControl));
            }
            if (controlCameraHeading)
                state.cameraHeading = *controlCameraHeading;
            return state;
        }

        static void TraceNativeState(const char* event, const NativeState& state)
        {
            if (!LadderCamera::log)
                return;
            std::fprintf(LadderCamera::log, "rearNative time=%u wall=%llu task=%p event=%s pos=%.4f/%.4f/%.4f destination=%.4f/%.4f/%.4f flags=%08X subTask=%d timer=%.3f stuck=%d input=%d/%d cameraHeading=%.5f\n",
                static_cast<uint32_t>(*CTimer::m_snTimeInMilliseconds), GetTickCount64(), reinterpret_cast<void*>(active.task), event,
                state.position.x, state.position.y, state.position.z, state.destination.x, state.destination.y, state.destination.z,
                state.flags, state.subTask, state.timer, state.stuckCount, state.inputX, state.inputY, state.cameraHeading);
        }

        static void TraceRetry(const char* event, float directionX, float directionY)
        {
            if (!LadderCamera::log || !blocked.ped)
                return;
            std::fprintf(LadderCamera::log, "rearRetry time=%u wall=%llu event=%s ladder=%p index=%d originalDirection=%.4f/%.4f currentDirection=%.4f/%.4f age=%u\n",
                static_cast<uint32_t>(*CTimer::m_snTimeInMilliseconds), GetTickCount64(), event,
                reinterpret_cast<void*>(blocked.ladder), blocked.index, blocked.directionX, blocked.directionY,
                directionX, directionY, static_cast<uint32_t>(*CTimer::m_snTimeInMilliseconds) - blocked.time);
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
            if (active.cancelReason || !IsAutoClimbEnabled())
            {
                Finish(active.cancelReason ? active.cancelReason : "disabled");
                return nFinishedTask;
            }
            if (type == std::to_underlying(TaskID::TaskComplexMoveGoToPointAndStandStill))
            {
                // A rear approach must not use IV's direct-walk fallback through the obstacle.
                Finish("direct-fallback-rejected");
                return nFinishedTask;
            }
            if (type == std::to_underlying(TaskID::TaskSimpleSlideToCoord))
            {
                const bool ready = IsAutoClimbEnabled() && AtUsableEntry(task, ped);
                Finish(ready ? "handoff" : "entry-check-failed", !ready);
                return ready ? type : nFinishedTask;
            }
            if (type != std::to_underlying(TaskID::TaskComplexMoveFollowNavMeshRoute))
            {
                Finish("unexpected-transition");
                return nFinishedTask;
            }
            return type;
        }

        static bool __cdecl CalculateEntryPositions(uintptr_t ped, uintptr_t ladder, int index,
            rage::Vector4* top, rage::Vector4* bottom, rage::Vector4* position, float* heading,
            int mode, rage::Vector4* normal, bool checkSide)
        {
            // Only this automatic task's initial navigation destination may be calculated from behind.
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
                Trace("begin");
            }
            const auto previous = std::exchange(calculatingTask, Matches(task, ped) ? task : 0);
            const auto result = shCreateFirstSubTask.unsafe_fastcall<uintptr_t>(task, edx, ped);
            calculatingTask = previous;
            if (!result && Matches(task, ped))
                Finish("entry-failed");
            return result;
        }

        static const char* CheckRoute(uintptr_t task, uintptr_t ped, const rage::Vector4& position)
        {
            const auto wrapper = *reinterpret_cast<const uintptr_t*>(task + 8);
            if (!wrapper)
                return nullptr;
            const auto wrapperType = reinterpret_cast<int (__thiscall*)(uintptr_t)>((*reinterpret_cast<uintptr_t**>(wrapper))[3]);
            if (wrapperType(wrapper) != std::to_underlying(TaskID::TaskComplexControlMovement))
                return "unexpected-wrapper";
            const auto movement = GetRunningMovementTask(wrapper, ped);
            if (!movement)
                return nullptr;
            const auto getType = reinterpret_cast<int (__thiscall*)(uintptr_t)>((*reinterpret_cast<uintptr_t**>(movement))[3]);
            if (getType(movement) != std::to_underlying(TaskID::TaskComplexMoveFollowNavMeshRoute))
                return "unexpected-movement";
            // PC navigation task: route result +D8, CPointRoute +70, next point index +98.
            const auto flags = *reinterpret_cast<const uint32_t*>(movement + 0xD8);
            if (((flags >> 11) & 3) == 2)
                return "no-route";
            const auto route = *reinterpret_cast<const uintptr_t*>(movement + 0x70);
            if (!(flags & 0x200) || !route)
                return nullptr;
            const int count = *reinterpret_cast<const int*>(route);
            if (count < 0 || count > 32)
                return "route-too-complex";
            if (!count)
                return nullptr;
            const int first = *reinterpret_cast<const int8_t*>(movement + 0x98);
            if (first < 0 || first > count)
                return "invalid-route-index";
            float length = 0.0f;
            float legacyLength = 0.0f;
            auto previous = position;
            auto legacyPrevious = position;
            for (int i = 0; i < count; ++i)
            {
                const auto& point = *reinterpret_cast<const rage::Vector4*>(route + 16 * (i + 1));
                if (!std::isfinite(point.x) || !std::isfinite(point.y) || !std::isfinite(point.z))
                    return "invalid-route";
                if (LadderCamera::log)
                {
                    const float x = point.x - legacyPrevious.x, y = point.y - legacyPrevious.y;
                    legacyLength += std::sqrt(x * x + y * y);
                    legacyPrevious = point;
                }
                if (i >= first)
                {
                    const float x = point.x - previous.x, y = point.y - previous.y;
                    length += std::sqrt(x * x + y * y);
                    if (!std::isfinite(length) || std::abs(point.z - active.start.z) > 1.5f)
                        return "invalid-route";
                    previous = point;
                }
            }
            const auto now = static_cast<uint32_t>(*CTimer::m_snTimeInMilliseconds);
            const bool trace = !active.routeLogged || first != active.routeIndex || count != active.routeCount
                || now - active.lastRouteTrace >= 250 || length > fMaxDistance;
            active.routeIndex = first;
            active.routeCount = count;
            active.remaining = length;
            active.legacyLength = LadderCamera::log ? legacyLength : -1.0f;
            if (trace && LadderCamera::log)
            {
                Trace(active.routeLogged ? "progress" : "route", length);
                TraceNativeState("progress", ReadNativeState(task, ped));
                for (int i = 0; i < count; ++i)
                {
                    const auto& point = *reinterpret_cast<const rage::Vector4*>(route + 16 * (i + 1));
                    std::fprintf(LadderCamera::log, "rearPoint time=%u task=%p point=%d passed=%d pos=%.4f/%.4f/%.4f\n",
                        now, reinterpret_cast<void*>(task), i, i < first, point.x, point.y, point.z);
                }
                active.lastRouteTrace = now;
                active.routeLogged = true;
            }
            if (length > fMaxDistance)
                return "detour-too-long";
            return nullptr;
        }

        static uintptr_t __fastcall ControlSubTask(uintptr_t task, void* edx, uintptr_t ped)
        {
            if (Matches(task, ped))
            {
                const auto matrix = *reinterpret_cast<const rage::Matrix44**>(ped + 0x20);
                const char* reason = active.cancelReason;
                if (!IsAutoClimbEnabled())
                    reason = "disabled";
                else if (!matrix)
                    reason = "no-transform";
                else if (static_cast<uint32_t>(*CTimer::m_snTimeInMilliseconds) - active.candidate.time > nTimeoutMs)
                    reason = "timeout";
                else if (!reason)
                {
                    const float x = matrix->pos.x - active.previous.x, y = matrix->pos.y - active.previous.y;
                    active.travelled += std::sqrt(x * x + y * y);
                    active.previous = matrix->pos;
                    if (!std::isfinite(active.travelled) || active.travelled > fMaxDistance
                        || !std::isfinite(matrix->pos.z) || std::abs(matrix->pos.z - active.start.z) > 1.0f)
                        reason = "travel-limit";
                    else
                        reason = CheckRoute(task, ped, matrix->pos);
                }
                if (reason)
                {
                    active.cancelReason = reason;
                    if (LadderCamera::log)
                        TraceNativeState(reason, ReadNativeState(task, ped));
                    const auto abort = reinterpret_cast<bool (__thiscall*)(uintptr_t, uintptr_t, int, uintptr_t)>(
                        (*reinterpret_cast<uintptr_t**>(task))[5]);
                    if (abort(task, ped, 1, 0))
                    {
                        *reinterpret_cast<uint32_t*>(task + 12) |= 2;
                        Finish(reason);
                    }
                }
            }
            // Retain native input cancellation, task ownership, and cleanup.
            NativeState before;
            if (Matches(task, ped) && LadderCamera::log)
                before = ReadNativeState(task, ped);
            const auto result = shControlSubTask.unsafe_fastcall<uintptr_t>(task, edx, ped);
            if (!result && Matches(task, ped))
            {
                TraceNativeState("native-cancel-before", before);
                Finish("native-cancel");
            }
            return result;
        }

        static void Install()
        {
            auto constructor = hook::pattern("8B 44 24 10 C7 06 ? ? ? ? C7 46 14 00 00 00 00 C7 46 20");
            auto first = hook::pattern("51 A0 ? ? ? ? 56 8B F1 0F B6 D0 B9 01 00 00 00");
            auto positions = hook::pattern("FF B6 94 00 00 00 FF 74 24 28 57 E8 ? ? ? ? 83 C4 28");
            auto movement = hook::pattern("8B 4E 08 57 E8 ? ? ? ? 89 44 24 30 85 C0 0F 84 ? ? ? ? 8B C8 E8");
            auto routeCursor = hook::pattern("0F BE B2 98 00 00 00 57 8B 7A 70 33 C0");
            if (constructor.size() != 1 || first.size() != 1 || positions.size() != 1 || movement.size() != 1 || routeCursor.size() != 1)
                return;
            const auto table = *constructor.get_first<uintptr_t*>(6);
            const uint8_t controlPrologue[]{ 0x55, 0x8B, 0xEC, 0x83, 0xE4, 0xF0 };
            const uint8_t destructorPrologue[]{ 0x56, 0x8B, 0xF1, 0xE8 };
            if (table[19] != reinterpret_cast<uintptr_t>(first.get_first(0))
                || std::memcmp(reinterpret_cast<void*>(table[20]), controlPrologue, sizeof(controlPrologue)) != 0
                || std::memcmp(reinterpret_cast<void*>(table[0]), destructorPrologue, sizeof(destructorPrologue)) != 0
                || injector::GetBranchDestination(positions.get_first(11)).as_int() != reinterpret_cast<uintptr_t>(CalculatePositionsForThisLadder))
                return;
            GetRunningMovementTask = reinterpret_cast<decltype(GetRunningMovementTask)>(
                injector::GetBranchDestination(movement.get_first(4)).as_int());
            shCreateFirstSubTask = safetyhook::create_inline(first.get_first(0), CreateFirstSubTask);
            shControlSubTask = safetyhook::create_inline(reinterpret_cast<void*>(table[20]), ControlSubTask);
            shDestructor = safetyhook::create_mid(reinterpret_cast<void*>(table[0]), [](SafetyHookContext& regs)
            {
                if (active.task == regs.ecx)
                    Finish("task-destroyed");
            });
            if (!shCreateFirstSubTask || !shControlSubTask || !shDestructor)
            {
                shCreateFirstSubTask.reset();
                shControlSubTask.reset();
                shDestructor.reset();
                return;
            }
            injector::MakeCALL(positions.get_first(11), CalculateEntryPositions);
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

    static ptrdiff_t nMoveBlendOffset = 0, nDesiredMoveBlendRatioOffset = 0;

    static void ResolveMoveBlendLayout()
    {
        auto caller = hook::pattern("E8 ? ? ? ? F3 0F 10 44 24 18 5F F3 0F 11 46 18");
        auto store = hook::pattern("F3 0F 11 41 ? FF 50 4C C2 0C 00");
        if (caller.size() == 1 && store.size() == 1)
        {
            const auto setter = injector::GetBranchDestination(caller.get_first(0)).as_int();
            const auto write = reinterpret_cast<uintptr_t>(store.get_first(0));
            if (write >= setter && write - setter < 64
                && *caller.get_first<uint16_t>(-24) == 0x8F8B)
            {
                nMoveBlendOffset = *caller.get_first<int32_t>(-22);
                nDesiredMoveBlendRatioOffset = *store.get_first<uint8_t>(4);
            }
        }
        if (LadderCamera::log)
            std::fprintf(LadderCamera::log, "Front entry blend layout: pedOffset=%d forwardRatioOffset=%d; missing layout uses 0.8m base distance. Candidate radius=1.5m; AutoClimbLaddersRange is ignored.\n",
                static_cast<int>(nMoveBlendOffset), static_cast<int>(nDesiredMoveBlendRatioOffset));
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

    namespace EntryDiagnostics
    {
        static uintptr_t (__thiscall* GetSimplestActiveMovementTask)(uintptr_t);
        static ptrdiff_t nIntelligenceOffset = 0, nTaskManagerOffset = 0, nCoverMaskOffset = 0;

        static void Init()
        {
            if (!LadderCamera::log)
                return;
            auto query = hook::pattern("56 8B F1 33 C9 8D 46 2C 8B 10 85 D2 74 ? F6 42 0C 01");
            auto caller = hook::pattern("8B 88 ? ? ? ? 8D 49 ? E8 ? ? ? ? 8B 4E 04 89 44 24 0C");
            if (query.size() == 1 && caller.size() == 1
                && injector::GetBranchDestination(caller.get_first(9)).as_int() == reinterpret_cast<uintptr_t>(query.get_first(0)))
            {
                GetSimplestActiveMovementTask = reinterpret_cast<decltype(GetSimplestActiveMovementTask)>(query.get_first(0));
                nIntelligenceOffset = *caller.get_first<int32_t>(2);
                nTaskManagerOffset = *caller.get_first<uint8_t>(8);
            }
            auto cover = hook::pattern("8A 41 07 32 C2 3C 7F 76 ? 80 7D 0C 00 75 ? 80 7D 14 00");
            if (cover.size() == 1 && *cover.get_first<uint16_t>(-21) == 0x908A
                && *cover.get_first<uint16_t>(-15) == 0x888D
                && *cover.get_first<int32_t>(-19) == *cover.get_first<int32_t>(-13) + 4)
                nCoverMaskOffset = *cover.get_first<int32_t>(-19);
            std::fprintf(LadderCamera::log, "Entry control bindings: movementQuery=%p intelligenceOffset=%d taskManagerOffset=%d coverMaskOffset=%d; diagnostic only.\n",
                reinterpret_cast<void*>(GetSimplestActiveMovementTask), static_cast<int>(nIntelligenceOffset),
                static_cast<int>(nTaskManagerOffset), static_cast<int>(nCoverMaskOffset));
        }

        static int GetTaskType(uintptr_t task)
        {
            if (!task)
                return -1;
            const auto getType = reinterpret_cast<int (__thiscall*)(uintptr_t)>((*reinterpret_cast<uintptr_t**>(task))[3]);
            return getType(task);
        }

        static void Trace(uintptr_t ped, uintptr_t ladder, int index, int mode, bool rear)
        {
            if (!LadderCamera::log)
                return;
            uintptr_t movementTask = 0;
            int movementType = -1, playerMove = -1, onFootChild = -1, coverTask = -1;
            if (GetSimplestActiveMovementTask)
            {
                if (const auto intelligence = *reinterpret_cast<const uintptr_t*>(ped + nIntelligenceOffset))
                {
                    const auto manager = intelligence + nTaskManagerOffset;
                    movementTask = GetSimplestActiveMovementTask(manager);
                    movementType = GetTaskType(movementTask);
                    playerMove = movementTask ? movementType == std::to_underlying(TaskID::TaskSimpleMovePlayer) : 0;
                    if (LadderCamera::FindActiveTaskByType)
                    {
                        const auto onFoot = LadderCamera::FindActiveTaskByType(manager, std::to_underlying(TaskID::TaskComplexPlayerOnFoot));
                        if (onFoot)
                            onFootChild = GetTaskType(*reinterpret_cast<const uintptr_t*>(onFoot + 8));
                        coverTask = LadderCamera::FindActiveTaskByType(manager, std::to_underlying(TaskID::TaskComplexPlayerInCover)) != 0;
                    }
                }
            }
            int coverCurrent = -1, coverPrevious = -1, coverPressed = -1;
            if (onFootControl && nCoverMaskOffset)
            {
                const auto* value = reinterpret_cast<const uint8_t*>(onFootControl + nCoverMaskOffset);
                coverCurrent = value[2] ^ value[0];
                coverPrevious = value[3] ^ value[0];
                coverPressed = coverCurrent > 0x7F && coverPrevious <= 0x7F;
            }
            int ragdoll = -1;
            // A null player-info argument selects the local player.
            if (reinterpret_cast<uintptr_t>(FindPlayerPed(0)) == ped)
            {
                Ped handle = 0;
                Natives::GetPlayerChar(Natives::GetPlayerId(), &handle);
                if (handle)
                    ragdoll = Natives::IsPedRagdoll(handle);
            }
            std::fprintf(LadderCamera::log, "entryControl time=%u wall=%llu ped=%p ladder=%p index=%d mode=%d rear=%d movement=%p movementType=%d playerMove=%d onFootChild=%d coverTask=%d coverCurrent=%d coverPrevious=%d coverPressed=%d ragdoll=%d\n",
                static_cast<uint32_t>(*CTimer::m_snTimeInMilliseconds), GetTickCount64(), reinterpret_cast<void*>(ped),
                reinterpret_cast<void*>(ladder), index, mode, rear, reinterpret_cast<void*>(movementTask), movementType,
                playerMove, onFootChild, coverTask, coverCurrent, coverPrevious, coverPressed, ragdoll);
        }
    }

    struct FrontEntryMetrics
    {
        rage::Vector4 position{}, mount{};
        float distance = 0.0f, limit = 0.0f, ratio = 0.0f;
        float headingMiss = -1.0f, movementMiss = -1.0f;
    };

    static void TraceFrontEntry(uintptr_t ladder, int index, const FrontEntryMetrics& entry, const char* rule, bool accepted)
    {
        if (!LadderCamera::log)
            return;
        static uint32_t lastRejected = 0;
        const auto now = static_cast<uint32_t>(*CTimer::m_snTimeInMilliseconds);
        if (!accepted && now - lastRejected < 250)
            return;
        if (!accepted)
            lastRejected = now;
        std::fprintf(LadderCamera::log, "frontEntry time=%u wall=%llu ladder=%p index=%d accepted=%d rule=%s pos=%.4f/%.4f/%.4f mount=%.4f/%.4f/%.4f distance=%.4f limit=%.4f blend=%.4f headingMiss=%.4f movementMiss=%.4f\n",
            now, GetTickCount64(), reinterpret_cast<void*>(ladder), index, accepted, rule,
            entry.position.x, entry.position.y, entry.position.z, entry.mount.x, entry.mount.y, entry.mount.z,
            entry.distance, entry.limit, entry.ratio, entry.headingMiss, entry.movementMiss);
    }

    static bool CanAutoMountLadder(uintptr_t ped, uintptr_t ladder, int index, const Direction* movement, bool* rearEntry = nullptr, int* entryMode = nullptr)
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
        if (entryMode)
            *entryMode = mode;
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
                FrontEntryMetrics entry;
                if (!wrongSide)
                {
                    entry.position = matrix->pos;
                    entry.mount = position;
                    const float x = position.x - matrix->pos.x, y = position.y - matrix->pos.y;
                    entry.distance = std::sqrt(x * x + y * y);
                    entry.ratio = GetDesiredMoveBlendRatio(ped);
                    // Ordinary mount distance uses requested locomotion, not physical speed.
                    entry.limit = 0.8f + entry.ratio * 0.1f;
                    if (!std::isfinite(entry.distance) || entry.distance >= entry.limit)
                    {
                        TraceFrontEntry(ladder, index, entry, "mount-distance", false);
                        return false;
                    }
                }
                if (Dot(desired, facing) <= std::cos(2.0f)
                    || Dot(*movement, toward) <= fMinMovementToLadderDot)
                    return false;

                // Limit the close-side allowance to slow approaches, not running past.
                const float sideDot = Dot(ladderNormal, toward);
                if (wrongSide && (Dot(toLadder, toLadder) > 1.5f * 1.5f || sideDot < 0.25f
                    || std::abs(feetZ - bottom.z) > 1.0f))
                    return false;
                const bool closeSideEntry = Dot(toLadder, toLadder) < fCloseBottomEntryDistance * fCloseBottomEntryDistance
                    && sideDot > -0.25f && sideDot < 0.15f
                    && speedSquared <= fSideEntryMaxSpeed * fSideEntryMaxSpeed;
                if (closeSideEntry)
                {
                    if (!wrongSide)
                        TraceFrontEntry(ladder, index, entry, "close-side", true);
                    return true;
                }

                const float halfWidth = speedSquared > 0.001f ? fLadderEntryHalfWidth : fLadderEntryHalfWidthStill;
                bool headingWithinWidth = false;
                bool movementWithinWidth = false;
                if (!ProjectApproach(desired, toLadder, ladderNormal, halfWidth, headingWithinWidth)
                    || !ProjectApproach(*movement, toLadder, ladderNormal, halfWidth, movementWithinWidth))
                    return false;
                const bool intended = headingWithinWidth || movementWithinWidth;
                if (!wrongSide)
                {
                    const auto missDistance = [&](const Direction& direction)
                    {
                        const float distance = Dot(toLadder, ladderNormal) / Dot(direction, ladderNormal);
                        const Direction miss{ distance * direction.x - toLadder.x, distance * direction.y - toLadder.y };
                        return std::sqrt(Dot(miss, miss));
                    };
                    if (LadderCamera::log)
                    {
                        entry.headingMiss = missDistance(desired);
                        entry.movementMiss = missDistance(*movement);
                    }
                    TraceFrontEntry(ladder, index, entry, headingWithinWidth
                        ? (movementWithinWidth ? "both-rays" : "heading-ray")
                        : (movementWithinWidth ? "movement-ray" : "missed-corridor"), intended);
                }
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

    template <bool InWater>
    static bool bAutomaticEntry = false;

    template <bool InWater>
    static injector::hook_back<int(__cdecl*)(int, DWORD*, char)> hbScanForLadderToClimb;

    template <bool InWater>
    static int __cdecl ScanForLadderToClimb(int ped, DWORD* index, char ignoreHeading)
    {
        // Consume the automatic-entry marker; manual scans keep native behavior.
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
                RearApproach::TraceRetry("clear-no-input", 0.0f, 0.0f);
                RearApproach::blocked = {};
                return 0;
            }
            if (movement.x * RearApproach::blocked.directionX + movement.y * RearApproach::blocked.directionY < 0.5f)
            {
                RearApproach::TraceRetry("clear-direction-change", movement.x, movement.y);
                RearApproach::blocked = {};
            }
        }

        fLadderScanRadius = fLadderCandidateRadius;
        // On-foot orientation is checked below using both facing and movement intent.
        auto ladder = hbScanForLadderToClimb<InWater>.fun(ped, index, !InWater);
        fLadderScanRadius = 4.0f;

        // Reject failed entry before the player controller can create a standstill task.
        bool rearEntry = false;
        int entryMode = -1;
        if (ladder && !CanAutoMountLadder(ped, ladder, static_cast<int>(*index), InWater ? nullptr : &movement, InWater ? nullptr : &rearEntry, &entryMode))
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
        if (ladder && LadderCamera::log)
        {
            if constexpr (!InWater)
                EntryDiagnostics::Trace(ped, ladder, static_cast<int>(*index), entryMode, rearEntry);
            const auto matrix = *reinterpret_cast<const rage::Matrix44**>(ped + 0x20);
            if (matrix)
                std::fprintf(LadderCamera::log, "ladderDecision time=%u wall=%llu event=scan-accepted ladder=%p index=%u mode=%d rear=%d water=%d pos=%.4f/%.4f/%.4f direction=%.4f/%.4f desiredHeading=%.5f\n",
                    static_cast<uint32_t>(*CTimer::m_snTimeInMilliseconds), GetTickCount64(), reinterpret_cast<void*>(ladder),
                    static_cast<unsigned>(*index), entryMode, rearEntry, InWater, matrix->pos.x, matrix->pos.y, matrix->pos.z,
                    movement.x, movement.y, *reinterpret_cast<const float*>(ped + 0xAA4));
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
            LadderCamera::SetStyle(iniReader.ReadInteger("MISC", "LadderCameraStyle", 0));
            LadderCamera::OpenLog();
            if (LadderCamera::IsEnabled())
                LadderCamera::OrbitProbe::Resolve();

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
            CTaskComplexClimbLadder::ResolveMoveBlendLayout();
            CTaskComplexClimbLadder::EntryDiagnostics::Init();

            CTaskComplexClimbLadder::FindTopAndBottomOfAllLadderSections = reinterpret_cast<decltype(CTaskComplexClimbLadder::FindTopAndBottomOfAllLadderSections)>(injector::GetBranchDestination(sections.get_first(7)).as_int());
            CTaskComplexClimbLadder::CalculatePositionsForThisLadder = reinterpret_cast<decltype(CTaskComplexClimbLadder::CalculatePositionsForThisLadder)>(positions.get_first(0));

            auto cameraGate = hook::pattern("0F 2F C1 77 ? F6 81 ? ? ? ? 04");
            auto mount = hook::pattern("55 8B EC 83 E4 F0 83 EC 18 56 57 8B 7D 08 8B F1 81 FF 87 03 00 00");
            auto activeTask = hook::pattern("53 55 56 33 F6 57 8B E9 33 C0 8D 9B 00 00 00 00");
            auto lookX = hook::pattern("57 8D BE 98 27 00 00 57 E8 ? ? ? ?");
            auto lookY = hook::pattern("57 8D BE B8 27 00 00 57 E8 ? ? ? ?");
            auto beginUpdate = hook::pattern("F6 86 8C 03 00 00 80 74 ? 57");
            auto lookBehindInput = hook::pattern("F6 C2 40 74 ? 84 C0 75 ? 80 E2 BF");
            auto orbitHeading = hook::pattern("D9 9E 08 03 00 00 83 C4 04 85 DB 74 ? 80 BB 89 32 00 00 00");
            auto collision = hook::pattern("57 56 50 53 E8 ? ? ? ? 8D 44 24 ? 50 8D 4E 10");
            auto cameraRoot = hook::pattern("E8 ? ? ? ? F3 0F 10 B6 80 02 00 00 F3 0F 10 BE");
            auto ladderPitch = hook::pattern("8B CE 57 E8 ? ? ? ? 8B 84 24 ? ? ? ? 85 C0 74");
            auto verticalFollow = hook::pattern("F3 0F 5C 86 38 02 00 00 F3 0F 10 94 24");
            auto mountPitch = hook::pattern("F3 0F 10 44 24 24 F3 0F 5C 44 24 50");
            auto pitchLevel = hook::pattern("F3 0F 10 44 24 6C 83 EC 08 8B CE");
            if (cameraGate.size() != 1 || mount.size() != 1
                || activeTask.size() != 1 || lookX.size() != 1 || lookY.size() != 1 || beginUpdate.size() != 1
                || (LadderCamera::IsEnabled() && (lookBehindInput.size() != 1 || orbitHeading.size() != 1 || collision.size() != 1))
                || (LadderCamera::IsEnabled() && cameraRoot.size() != 1)
                || (LadderCamera::IsHeightFollow() && (ladderPitch.size() != 1 || verticalFollow.size() != 1
                    || mountPitch.size() != 1 || pitchLevel.size() != 1)))
            {
                if (LadderCamera::log)
                {
                    std::fprintf(LadderCamera::log, "Camera signatures unavailable or ambiguous; auto-ladder hooks not installed.\n");
                    std::fflush(LadderCamera::log);
                }
                return;
            }

            // Validate the function entries before installing detours.
            const auto entry = cameraGate.get_first<uint8_t>(-0x3F);
            const uint8_t prologue[]{ 0x55, 0x8B, 0xEC, 0x83, 0xE4, 0xF0, 0x83, 0xEC, 0x28 };
            const auto branch = cameraGate.get_first<uint8_t>(3);
            const auto calculation = branch + 2 + static_cast<int8_t>(branch[1]);
            const uint8_t forceTest[]{ 0xF6, 0x81, 0x8D, 0x03, 0x00, 0x00, 0x04 };
            const auto xEntry = lookX.get_first<uint8_t>(-0x14);
            const auto yEntry = lookY.get_first<uint8_t>(-0x14);
            const uint8_t getterPrologue[]{ 0x00, 0x56, 0x8B, 0xF1, 0x74, 0x08, 0x8D, 0x86 };
            const auto axis = injector::GetBranchDestination(lookX.get_first(8)).as_int();
            const bool gettersValid = xEntry[0] == 0x83 && xEntry[1] == 0x3D
                && yEntry[0] == 0x83 && yEntry[1] == 0x3D
                && std::memcmp(xEntry + 6, getterPrologue, sizeof(getterPrologue)) == 0
                && std::memcmp(yEntry + 6, getterPrologue, sizeof(getterPrologue)) == 0
                && axis == injector::GetBranchDestination(lookY.get_first(8)).as_int();
            if (std::memcmp(entry, prologue, sizeof(prologue)) != 0 || !gettersValid
                || branch[0] != 0x77 || branch[1] != 0x16
                || std::memcmp(branch + 2, forceTest, sizeof(forceTest)) != 0
                || std::memcmp(calculation, forceTest, sizeof(forceTest)) != 0)
                return;

            LadderCamera::FindActiveTaskByType = reinterpret_cast<decltype(LadderCamera::FindActiveTaskByType)>(activeTask.get_first(0));
            LadderCamera::GetPedAimWeaponLeftRight = reinterpret_cast<decltype(LadderCamera::GetPedAimWeaponLeftRight)>(xEntry);
            LadderCamera::GetPedLookUpDown = reinterpret_cast<decltype(LadderCamera::GetPedLookUpDown)>(yEntry);
            LadderCamera::GetAxisValue = reinterpret_cast<decltype(LadderCamera::GetAxisValue)>(axis);
            LadderCamera::shBeginUpdate = safetyhook::create_mid(beginUpdate.get_first(7), LadderCamera::BeginUpdate);
            if (LadderCamera::IsEnabled())
            {
                LadderCamera::shLookBehindInput = safetyhook::create_mid(lookBehindInput.get_first(0), LadderCamera::LookBehindInput);
                LadderCamera::shOrbitHeading = safetyhook::create_mid(orbitHeading.get_first(6), LadderCamera::OrbitHeading);
                LadderCamera::shBeforeCollision = safetyhook::create_mid(collision.get_first(4), LadderCamera::BeforeCollision);
                LadderCamera::shAfterCollision = safetyhook::create_mid(collision.get_first(9), LadderCamera::AfterCollision);
            }
            if (LadderCamera::IsEnabled())
                LadderCamera::GetRootBone = reinterpret_cast<decltype(LadderCamera::GetRootBone)>(
                    injector::GetBranchDestination(cameraRoot.get_first(0)).as_int());
            if (LadderCamera::IsHeightFollow())
            {
                LadderCamera::shLadderPitch = safetyhook::create_inline(
                    injector::GetBranchDestination(ladderPitch.get_first(3)).as_int(), LadderCamera::LadderPitch);
                LadderCamera::shVerticalFollow = safetyhook::create_mid(verticalFollow.get_first(0), LadderCamera::VerticalFollow);
                LadderCamera::shMountPitch = safetyhook::create_mid(mountPitch.get_first(0), LadderCamera::MountPitch);
                LadderCamera::shPitchLevel = safetyhook::create_mid(pitchLevel.get_first(0), LadderCamera::PitchLevel);
            }
            LadderCamera::shHeadingLimit = safetyhook::create_mid(branch, LadderCamera::HeadingLimit);
            if (LadderCamera::shHeadingLimit)
                LadderCamera::shTargetBetaCalc = safetyhook::create_inline(entry, LadderCamera::TargetBetaCalc);
            if (LadderCamera::shTargetBetaCalc)
                CTaskComplexClimbLadder::shCreateSubTask = safetyhook::create_inline(mount.get_first(0), CTaskComplexClimbLadder::CreateSubTask);
            if (!LadderCamera::shTargetBetaCalc || !CTaskComplexClimbLadder::shCreateSubTask || !LadderCamera::shBeginUpdate
                || (LadderCamera::IsEnabled() && (!LadderCamera::shLookBehindInput || !LadderCamera::shOrbitHeading
                    || !LadderCamera::shBeforeCollision || !LadderCamera::shAfterCollision))
                || (LadderCamera::IsHeightFollow() && (!LadderCamera::shLadderPitch || !LadderCamera::shVerticalFollow
                    || !LadderCamera::shMountPitch || !LadderCamera::shPitchLevel)))
            {
                CTaskComplexClimbLadder::shCreateSubTask.reset();
                LadderCamera::ResetHooks();
                if (LadderCamera::log)
                {
                    std::fprintf(LadderCamera::log, "Camera/mount hook installation failed; hooks removed.\n");
                    std::fflush(LadderCamera::log);
                }
                return;
            }
            ShortLadderTraversal::shProcessPed = safetyhook::create_inline(ladderProcess.get_first(0), ShortLadderTraversal::ProcessPed);
            if (!ShortLadderTraversal::shProcessPed)
            {
                CTaskComplexClimbLadder::shCreateSubTask.reset();
                LadderCamera::ResetHooks();
                if (LadderCamera::log)
                {
                    std::fprintf(LadderCamera::log, "Ladder hook installation failed; hooks removed.\n");
                    std::fflush(LadderCamera::log);
                }
                return;
            }
            if (LadderCamera::log)
            {
                std::fprintf(LadderCamera::log, "Hooks installed: target=%p branch=%p calculation=%p beginUpdate=%p restricted=%d\n",
                    static_cast<void*>(entry), static_cast<void*>(branch), static_cast<void*>(calculation),
                    beginUpdate.get_first(7), LadderCamera::IsEnabled());
                std::fflush(LadderCamera::log);
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
            CTaskComplexClimbLadder::RearApproach::Install();
            if (LadderCamera::log)
            {
                std::fprintf(LadderCamera::log, "Rear approach hooks installed=%d\n", CTaskComplexClimbLadder::RearApproach::installed);
                std::fflush(LadderCamera::log);
            }
            LadderCamera::HandoffTrace::Install();
        };
    }
} AutoLadder;
