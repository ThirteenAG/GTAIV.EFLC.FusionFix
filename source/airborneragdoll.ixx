module;

#include <common.hxx>
#include <algorithm>
#include <array>
#include <cmath>
#include <stdexcept>

export module airborneragdoll;

import common;
import comvars;
import natives;
import settings;

namespace AirborneRagdoll
{
    // Menu configuration
    constexpr bool ShowRagdollMenuOption = true;  // Hide the rows without disabling the feature or saved setting
    constexpr auto JumpRagdollPreference = "PREF_PLAYERJUMPRAGDOLL";

    // Restore temporary native state when leaving a hook
    template <typename T>
    class ScopedValue
    {
        T& target;
        T saved;

    public:
        ScopedValue(T& target, const T& value) : target(target), saved(target) { target = value; }
        ~ScopedValue() { target = saved; }
        ScopedValue(const ScopedValue&) = delete;
        ScopedValue& operator=(const ScopedValue&) = delete;
    };

    // Command event ownership and water recovery
    namespace CommandEvents
    {
        struct OwnedEvent
        {
            void* event = nullptr;
            uintptr_t ped = 0;
        };

        // Keep ownership through native event cloning
        std::array<OwnedEvent, 32> owned{};
        thread_local uintptr_t postingPed = 0;
        thread_local void* postingTask = nullptr;
        uintptr_t waterHandoffPed = 0;
        SafetyHookInline shConstructor, shClone, shDestructor, shWaterPriority;

        OwnedEvent* Find(void* event)
        {
            if (event)
            {
                for (auto& entry : owned)
                    if (entry.event == event)
                        return &entry;
            }
            return nullptr;
        }

        void Track(void* event, uintptr_t ped)
        {
            if (!event || !ped)
                return;
            auto* entry = Find(event);
            if (!entry)
            {
                for (auto& candidate : owned)
                {
                    if (!candidate.event)
                    {
                        entry = &candidate;
                        break;
                    }
                }
            }
            if (entry)
                *entry = { event, ped };
        }

        void* __fastcall ConstructorHook(void* event, void* edx, uint32_t time, void* task, bool scriptEvent)
        {
            // Use ordinary NM priority for player commands
            const bool ownedCommand = postingPed && task == postingTask;
            auto* result = shConstructor.unsafe_fastcall<void*>(event, edx, time, task, ownedCommand ? false : scriptEvent);
            if (ownedCommand)
                Track(result, postingPed);
            return result;
        }

        void* __fastcall CloneHook(void* event, void* edx)
        {
            auto* result = shClone.unsafe_fastcall<void*>(event, edx);
            if (auto* entry = Find(event))
                Track(result, entry->ped);
            return result;
        }

        uintptr_t __fastcall DestructorHook(void* event, void* edx)
        {
            if (auto* entry = Find(event))
                *entry = {};
            return shDestructor.unsafe_fastcall<uintptr_t>(event, edx);
        }

        bool __fastcall WaterPriorityHook(void* waterEvent, void* edx, void* otherEvent)
        {
            auto* entry = Find(otherEvent);
            if (entry && entry->ped == CPlayer::getLocalPlayerPed()
                && !Natives::IsNetworkGameRunning()
                && (*reinterpret_cast<const uint8_t*>(entry->ped + 0x29C) & 4) != 0
                && CPed::ComparePedTasks(entry->ped, TaskID::TaskSimpleNMHighFall))
            {
                // Let swimming replace this command's HighFall event
                waterHandoffPed = entry->ped;
                return true;
            }
            return shWaterPriority.unsafe_fastcall<bool>(waterEvent, edx, otherEvent);
        }

        void Reset()
        {
            owned = {};
            waterHandoffPed = 0;
        }

        void ResetHooks()
        {
            shWaterPriority.reset();
            shClone.reset();
            shDestructor.reset();
            shConstructor.reset();
            Reset();
        }
    }

    // Native task bindings and parameter layouts
    namespace CPlayerPed
    {
        uintptr_t (__thiscall* GetControlFromPlayer)(uintptr_t playerPed) = nullptr;
    }

    enum class eRagdollTriggerTypes : uint32_t
    {
        RAGDOLL_TRIGGER_SCRIPT = 0x12,
    };

    namespace CTaskSimpleNM
    {
        bool (__cdecl* CanUseRagdoll)(uintptr_t ped, eRagdollTriggerTypes trigger, uintptr_t entity, float force) = nullptr;
    }

    namespace CTaskSimpleNMHighFall
    {
        struct Parameters
        {
            float pdStrengthPlayer;
            float pdStrengthAI;
            float hfBodyStiffness;
            float hfBodyDamping;
            float hfCutOffVelocity;
            float hfCatchFallTime;
            float hfArmsUp;
            float hfLegRadius;
            float hfLegAngularSpeed;
            float hfArmPeriod;
            float hfArmAmplitude;
            float hfAimAngle;
            float hfLegSideDistance;
            float hfFowardIKOffset;
            float hfFowardVelCompOnAngle;
            bool hfForwardRoll;
            bool hfOrientateBodyToFallDirection;
            bool hfUseZeroPose;
            uint8_t padding;
        };
        static_assert(sizeof(Parameters) == 0x40);

        enum class eHighFallType : int32_t
        {
            STANDARD = 0,
        };

        void* (__thiscall* Constructor)(void* storage, uint32_t minTime, uint32_t maxTime, const void* grabHelper, eHighFallType type) = nullptr;
        Parameters* ms_Parameters = nullptr;
        SafetyHookInline shStartBehaviour{};
    }

    // NaturalMotion message bindings
    namespace ART::MessageParams
    {
        constexpr size_t Size = 0xCC4;
        void (__thiscall* Constructor)(void* params) = nullptr;
        void (__thiscall* addBool)(void* params, const char* name, bool value) = nullptr;
        void (__thiscall* addFloat)(void* params, const char* name, float value) = nullptr;
        void (__thiscall* Destructor)(void* params) = nullptr;
    }

    namespace rage::fragInstNM
    {
        void (__thiscall* PostARTMessage)(void* fragInstNM, const char* messageName, const void* params) = nullptr;
    }

    // HighFall task creation and limb control
    namespace NaturalMotion
    {
        constexpr ptrdiff_t PedIntelligenceOffset = 0x224;
        ptrdiff_t FragInstNMOffset = 0x7B4;

        struct alignas(16) RagdollComponentMatrix
        {
            float values[16];
        };
        static_assert(sizeof(RagdollComponentMatrix) == 0x40);

        enum class RagdollComponent : int32_t
        {
            Buttocks = 0,
            Spine3 = 10,
            Neck = 11,
        };

        namespace armsWindmillAdaptive
        {
            struct Parameters
            {
                float armStiffness;
                float bodyStiffness;
                float period;
                float amplitude;
                float phase;
                bool disableOnImpact;
            };
        }

        namespace pedalLegs
        {
            struct Parameters
            {
                bool backPedal;
                float legStiffness;
                bool pedalLeftLeg;
                bool pedalRightLeg;
                float radius;
                float angularSpeed;
                float pedalOffset;
                float speedAsymmetry;
                bool adaptivePedal4Dragging;
                float radiusVariance;
                float legAngleVariance;
            };
        }

        void* (__thiscall* AllocateFromPoolInternal)(void* pool) = nullptr;
        bool (__cdecl* PostNMTaskInternal)(uintptr_t ped, uint32_t time, bool setPedFlag, void* task) = nullptr;
        bool (__thiscall* GetRagdollComponentMatrixInternal)(uintptr_t ped, RagdollComponentMatrix* matrix, int32_t component) = nullptr;
        // Match deferred task startup by ped, including cloned tasks
        struct PendingHighFall
        {
            uintptr_t ped = 0;
            uint32_t submittedAt = 0;
            uint32_t eventTime = 0;
            CTaskSimpleNMHighFall::Parameters highFall{};
            armsWindmillAdaptive::Parameters arms{};
            pedalLegs::Parameters legs{};

            bool Expired(uint32_t now) const { return now - submittedAt > eventTime; }
        } pending;

        // Apply arm and leg overrides after the native HighFall messages
        void PostHighFallBehaviours(
            uintptr_t ped,
            const armsWindmillAdaptive::Parameters& armsWindmillAdaptiveParameters,
            const pedalLegs::Parameters& pedalLegsParameters)
        {
            if (!ped
                || !ART::MessageParams::Constructor
                || !ART::MessageParams::addBool
                || !ART::MessageParams::addFloat
                || !rage::fragInstNM::PostARTMessage
                || !ART::MessageParams::Destructor)
            {
                return;
            }

            void* fragInstNM = *reinterpret_cast<void**>(ped + FragInstNMOffset);
            if (!fragInstNM)
                return;

            alignas(16) uint8_t params[ART::MessageParams::Size]{};
            ART::MessageParams::Constructor(params);
            ART::MessageParams::addBool(params, "start", true);
            ART::MessageParams::addFloat(params, "armStiffness", armsWindmillAdaptiveParameters.armStiffness);
            ART::MessageParams::addFloat(params, "bodyStiffness", armsWindmillAdaptiveParameters.bodyStiffness);
            ART::MessageParams::addFloat(params, "period", armsWindmillAdaptiveParameters.period);
            ART::MessageParams::addFloat(params, "amplitude", armsWindmillAdaptiveParameters.amplitude);
            ART::MessageParams::addFloat(params, "phase", armsWindmillAdaptiveParameters.phase);
            ART::MessageParams::addBool(params, "disableOnImpact", armsWindmillAdaptiveParameters.disableOnImpact);
            rage::fragInstNM::PostARTMessage(fragInstNM, "armsWindmillAdaptive", params);
            ART::MessageParams::Destructor(params);

            ART::MessageParams::Constructor(params);
            ART::MessageParams::addBool(params, "start", true);
            ART::MessageParams::addBool(params, "backPedal", pedalLegsParameters.backPedal);
            ART::MessageParams::addFloat(params, "legStiffness", pedalLegsParameters.legStiffness);
            ART::MessageParams::addBool(params, "pedalLeftLeg", pedalLegsParameters.pedalLeftLeg);
            ART::MessageParams::addBool(params, "pedalRightLeg", pedalLegsParameters.pedalRightLeg);
            ART::MessageParams::addFloat(params, "radius", pedalLegsParameters.radius);
            ART::MessageParams::addFloat(params, "angularSpeed", pedalLegsParameters.angularSpeed);
            ART::MessageParams::addFloat(params, "pedalOffset", pedalLegsParameters.pedalOffset);
            ART::MessageParams::addFloat(params, "speedAsymmetry", pedalLegsParameters.speedAsymmetry);
            ART::MessageParams::addBool(params, "adaptivePedal4Dragging", pedalLegsParameters.adaptivePedal4Dragging);
            ART::MessageParams::addFloat(params, "radiusVariance", pedalLegsParameters.radiusVariance);
            ART::MessageParams::addFloat(params, "legAngleVariance", pedalLegsParameters.legAngleVariance);
            rage::fragInstNM::PostARTMessage(fragInstNM, "pedalLegs", params);
            ART::MessageParams::Destructor(params);
        }

        void __fastcall StartBehaviourHook(void* task, void* edx, uintptr_t ped)
        {
            if (pending.ped && pending.Expired(*CTimer::m_snTimeInMilliseconds))
                pending = {};

            if (ped != pending.ped || !pending.ped)
            {
                CTaskSimpleNMHighFall::shStartBehaviour.unsafe_fastcall(task, edx, ped);
                return;
            }

            // Consume the command profile once
            const auto profile = pending;
            pending = {};
            {
                // Replace the shared STANDARD profile only for this native call
                ScopedValue parameters(CTaskSimpleNMHighFall::ms_Parameters[0], profile.highFall);
                CTaskSimpleNMHighFall::shStartBehaviour.unsafe_fastcall(task, edx, ped);
            }
            PostHighFallBehaviours(ped, profile.arms, profile.legs);
        }

        bool SwitchToNMHighFall(
            uintptr_t ped,
            uint32_t minTime,
            uint32_t maxTime,
            CTaskSimpleNMHighFall::eHighFallType type,
            uint32_t eventTime,
            const CTaskSimpleNMHighFall::Parameters& parameters,
            const armsWindmillAdaptive::Parameters& armsWindmillAdaptiveParameters,
            const pedalLegs::Parameters& pedalLegsParameters)
        {
            if (!ped)
                return false;

            const uintptr_t intelligence = *reinterpret_cast<uintptr_t*>(ped + PedIntelligenceOffset);
            auto* taskPool = CTask::GetTaskPool();
            if (!intelligence || !taskPool)
                return false;

            void* taskStorage = AllocateFromPoolInternal(taskPool);
            if (!taskStorage)
                return false;

            void* task = CTaskSimpleNMHighFall::Constructor(
                taskStorage,
                minTime,
                maxTime,
                nullptr,
                type);
            if (!task)
                return false;

            pending = { ped, *CTimer::m_snTimeInMilliseconds, eventTime,
                parameters, armsWindmillAdaptiveParameters, pedalLegsParameters };
            CommandEvents::waterHandoffPed = 0;
            ScopedValue postingPed(CommandEvents::postingPed, ped);
            ScopedValue postingTask(CommandEvents::postingTask, task);
            // Native posting owns the task, even on rejection
            const bool posted = PostNMTaskInternal(ped, eventTime, false, task);
            if (!posted && pending.ped == ped)
                pending = {};
            return posted;
        }
    }

    // Configuration
    namespace RagdollJumpTuning
    {
        constexpr uint32_t RagdollJumpTestMinTimeMs = 200;        // Earliest input request during a jump
        constexpr uint32_t RagdollJumpActivationMinTimeMs = 200;  // Earliest activation of an accepted request
        constexpr uint32_t NMHighFallMinimumTimeMs = 100;    // Native task minimum duration
        constexpr uint32_t NMHighFallMaximumTimeMs = 10000;  // Native task time limit
        constexpr uint32_t NMHighFallEventTimeMs = 10000;    // Event lifetime and pending-profile expiry

        // Initial HighFall profile; other values come from the game's STANDARD set
        namespace NMHFall
        {
            constexpr float hfBodyStiffness = 12.0f;    // Whole-body stiffness
            constexpr float hfLegRadius = 0.4f;         // Leg pedalling radius in meters
            constexpr float hfLegAngularSpeed = 7.85f;  // Leg pedalling rate
            constexpr float hfArmPeriod = 0.8004058f;   // Lower values windmill the arms faster
            constexpr float hfArmAmplitude = 1.5f;      // Size of the arm swing
        }

        // Arm overrides reapplied during the initial control window
        namespace armsWindmillAdaptive
        {
            constexpr float armStiffness = 12.0f;    // Requested arm stiffness
            constexpr float bodyStiffness = 12.0f;   // Body stiffness request, sent after armStiffness
            constexpr float period = 0.8004058f;     // Lower values produce faster windmilling
            constexpr float amplitude = 1.5f;        // Larger values widen the arm swing
            constexpr float phase = 3.1f;            // Offset between the left and right arm cycles
            constexpr bool disableOnImpact = false;  // Stop each arm's windmilling when it hits the ground
        }

        // Leg overrides reapplied during the initial control window
        namespace pedalLegs
        {
            constexpr bool backPedal = false;               // Reverse the pedalling direction
            constexpr float legStiffness = 12.0f;           // Strength of the leg motion
            constexpr bool pedalLeftLeg = true;             // Enable left-leg pedalling
            constexpr bool pedalRightLeg = true;            // Enable right-leg pedalling
            constexpr float radius = 0.4f;                  // Base pedalling radius in meters
            constexpr float angularSpeed = 7.85f;           // Base pedalling rate
            constexpr float pedalOffset = 0.0f;             // Adjust leg straightness
            constexpr float speedAsymmetry = 0.4f;          // Random per-leg speed offset; zero synchronizes rates
            constexpr bool adaptivePedal4Dragging = false;  // Use travel-driven pedalling instead of angularSpeed
            constexpr float radiusVariance = 0.4f;          // Pedalling radius variation (0-1)
            constexpr float legAngleVariance = 0.5f;        // Hip-angle variation (0-1)
        }

        // Trajectory and forward pitch, timed from command acceptance
        constexpr uint32_t BallisticCorrectionDurationMs = 300;  // Window for suppressing added upward velocity
        constexpr uint32_t PitchForceDurationMs = 300;           // Window for applying forward pitch
        constexpr uint32_t BehaviourControlDurationMs = 300;     // Window for reapplying limb messages
        constexpr uint32_t EffectWaitLimitMs = 1000;             // Discard effects if ragdoll activation takes too long
        constexpr float ObservedJumpGravity = 10.24f;            // Downward acceleration used by the velocity ceiling
        constexpr uint32_t PitchForceType = 1;                   // Native impulse mode; magnitude is time-scaled below
        constexpr float PitchMagnitude = 800.0f;                 // Spine3 impulse per second before speed/upright scaling
        constexpr float PitchVelocityMaximum = 4.0f;             // Horizontal speed reaching maximum pitch scale
        constexpr float PitchScaleAtMinimumVelocity = 0.5f;      // Pitch multiplier near zero horizontal speed
        constexpr float PitchScaleAtMaximumVelocity = 1.0f;      // Pitch multiplier at or above the speed limit
    }

    // Player input fields and per-jump state
    constexpr ptrdiff_t ReloadInputOffset = 0x2868;
    constexpr ptrdiff_t JumpInputOffset = 0x26B8;
    constexpr uint8_t ButtonDownThreshold = 0x7F;

    struct Velocity
    {
        float x = 0.0f;
        float y = 0.0f;
        float z = 0.0f;
    };

    struct ButtonState
    {
        uint8_t previous = 0;
        uint8_t current = 0;

        bool IsJustPressed() const
        {
            return previous <= ButtonDownThreshold && current > ButtonDownThreshold;
        }
    };

    struct PostActivationEffects
    {
        bool active = false;
        bool pitchForceEligible = false;
        uint32_t startTime = 0;
        uint32_t pitchAppliedDurationMs = 0;
        Velocity activationVelocity;
        float pitchDirectionX = 0.0f;
        float pitchDirectionY = 0.0f;
        float pitchScale = 0.0f;
    } effects;

    bool jumpWasActive = false;
    bool waitingForJumpRelease = false;
    bool ragdollJumpRequested = false;
    bool highFallAcceptedThisJump = false;
    uint32_t jumpStartTime = 0;
    bool bPlayerJumpRagdollControl = false;
    bool bPlayerClimbRagdollControl = false;
    uintptr_t currentPed = 0;
    Ped currentPedHandle = 0;

    // Movement and input helpers
    Velocity ReadVelocity(Ped ped)
    {
        Velocity velocity;
        Natives::GetCharVelocity(ped, &velocity.x, &velocity.y, &velocity.z);
        return velocity;
    }

    float HorizontalSpeed(const Velocity& velocity)
    {
        return std::sqrt(velocity.x * velocity.x + velocity.y * velocity.y);
    }

    // Reduce pitch as the torso approaches horizontal
    float ReadUprightScale(uintptr_t ped)
    {
        NaturalMotion::RagdollComponentMatrix buttocks{};
        NaturalMotion::RagdollComponentMatrix neck{};
        if (!NaturalMotion::GetRagdollComponentMatrixInternal(
                ped,
                &buttocks,
                static_cast<int32_t>(NaturalMotion::RagdollComponent::Buttocks))
            || !NaturalMotion::GetRagdollComponentMatrixInternal(
                ped,
                &neck,
                static_cast<int32_t>(NaturalMotion::RagdollComponent::Neck)))
        {
            return 1.0f;
        }

        const float x = neck.values[12] - buttocks.values[12];
        const float y = neck.values[13] - buttocks.values[13];
        const float z = neck.values[14] - buttocks.values[14];
        const float lengthSquared = x * x + y * y + z * z;
        if (lengthSquared <= 0.000001f)
            return 1.0f;

        return std::clamp(z / std::sqrt(lengthSquared), 0.0f, 1.0f);
    }

    ButtonState ReadButtonState(uintptr_t control, ptrdiff_t inputOffset)
    {
        const auto* input = reinterpret_cast<const uint8_t*>(control + inputOffset);
        const uint8_t mask = input[4];
        // Current and previous samples share the input record's XOR mask.
        return {
            static_cast<uint8_t>(input[7] ^ mask),
            static_cast<uint8_t>(input[6] ^ mask)
        };
    }

    // Build the limb messages from the authoring values
    NaturalMotion::armsWindmillAdaptive::Parameters GetArmsWindmillAdaptiveParameters()
    {
        NaturalMotion::armsWindmillAdaptive::Parameters parameters{};
        parameters.armStiffness = RagdollJumpTuning::armsWindmillAdaptive::armStiffness;
        parameters.bodyStiffness = RagdollJumpTuning::armsWindmillAdaptive::bodyStiffness;
        parameters.period = RagdollJumpTuning::armsWindmillAdaptive::period;
        parameters.amplitude = RagdollJumpTuning::armsWindmillAdaptive::amplitude;
        parameters.phase = RagdollJumpTuning::armsWindmillAdaptive::phase;
        parameters.disableOnImpact = RagdollJumpTuning::armsWindmillAdaptive::disableOnImpact;
        return parameters;
    }

    NaturalMotion::pedalLegs::Parameters GetPedalLegsParameters()
    {
        NaturalMotion::pedalLegs::Parameters parameters{};
        parameters.backPedal = RagdollJumpTuning::pedalLegs::backPedal;
        parameters.legStiffness = RagdollJumpTuning::pedalLegs::legStiffness;
        parameters.pedalLeftLeg = RagdollJumpTuning::pedalLegs::pedalLeftLeg;
        parameters.pedalRightLeg = RagdollJumpTuning::pedalLegs::pedalRightLeg;
        parameters.radius = RagdollJumpTuning::pedalLegs::radius;
        parameters.angularSpeed = RagdollJumpTuning::pedalLegs::angularSpeed;
        parameters.pedalOffset = RagdollJumpTuning::pedalLegs::pedalOffset;
        parameters.speedAsymmetry = RagdollJumpTuning::pedalLegs::speedAsymmetry;
        parameters.adaptivePedal4Dragging = RagdollJumpTuning::pedalLegs::adaptivePedal4Dragging;
        parameters.radiusVariance = RagdollJumpTuning::pedalLegs::radiusVariance;
        parameters.legAngleVariance = RagdollJumpTuning::pedalLegs::legAngleVariance;
        return parameters;
    }

    // Clear state between jumps and player changes
    void ResetJumpState()
    {
        jumpWasActive = false;
        waitingForJumpRelease = false;
        ragdollJumpRequested = false;
        jumpStartTime = 0;
        highFallAcceptedThisJump = false;
    }

    void ResetPostActivationEffects()
    {
        effects = {};
    }

    void ResetPlayerState()
    {
        CommandEvents::Reset();
        ResetJumpState();
        ResetPostActivationEffects();
        NaturalMotion::pending = {};
        currentPed = 0;
        currentPedHandle = 0;
    }

    // Initial limb control, launch suppression and forward pitch
    void ProcessPostActivationEffects(uintptr_t ped, Ped pedHandle, uint32_t now)
    {
        if (!effects.active)
            return;

        // Stop intervening once the water task takes priority
        if (CommandEvents::waterHandoffPed == ped)
        {
            ResetPostActivationEffects();
            return;
        }

        const uint32_t elapsed = now - effects.startTime;
        if (elapsed > RagdollJumpTuning::EffectWaitLimitMs)
        {
            ResetPostActivationEffects();
            return;
        }

        if (!Natives::IsPedRagdoll(pedHandle))
            return;

        const uint32_t effectDurationMs = std::max(
            std::max(
                RagdollJumpTuning::BallisticCorrectionDurationMs,
                RagdollJumpTuning::PitchForceDurationMs),
            RagdollJumpTuning::BehaviourControlDurationMs);
        if (elapsed <= effectDurationMs)
        {
            const Velocity velocity = ReadVelocity(pedHandle);
            if (elapsed <= RagdollJumpTuning::BehaviourControlDurationMs)
            {
                NaturalMotion::PostHighFallBehaviours(
                    ped,
                    GetArmsWindmillAdaptiveParameters(),
                    GetPedalLegsParameters());
            }

            // Cap upward velocity without changing horizontal momentum
            if (elapsed <= RagdollJumpTuning::BallisticCorrectionDurationMs)
            {
                const float expectedVelocityZ = effects.activationVelocity.z
                    - RagdollJumpTuning::ObservedJumpGravity * (static_cast<float>(elapsed) / 1000.0f);
                if (velocity.z > expectedVelocityZ)
                    Natives::SetCharVelocity(pedHandle, velocity.x, velocity.y, expectedVelocityZ);
            }

            // Integrate impulse over elapsed time, not frame count
            const uint32_t targetPitchDurationMs = std::min(
                elapsed,
                RagdollJumpTuning::PitchForceDurationMs);
            if (targetPitchDurationMs > effects.pitchAppliedDurationMs)
            {
                const uint32_t deltaMs = targetPitchDurationMs - effects.pitchAppliedDurationMs;
                if (effects.pitchForceEligible)
                {
                    const float impulseMagnitude = RagdollJumpTuning::PitchMagnitude
                        * effects.pitchScale
                        * ReadUprightScale(ped)
                        * (static_cast<float>(deltaMs) / 1000.0f);

                    Natives::ApplyForceToPed(
                        pedHandle,
                        RagdollJumpTuning::PitchForceType,
                        effects.pitchDirectionX * impulseMagnitude,
                        effects.pitchDirectionY * impulseMagnitude,
                        0.0f,
                        0.0f,
                        0.0f,
                        0.0f,
                        static_cast<uint32_t>(NaturalMotion::RagdollComponent::Spine3),
                        false,
                        false,
                        false);
                }

                effects.pitchAppliedDurationMs = targetPitchDurationMs;
            }
        }

        if (elapsed >= effectDurationMs)
            ResetPostActivationEffects();
    }

    void StartPostActivationEffects(
        uintptr_t ped,
        Ped pedHandle,
        uint32_t now,
        bool climbing,
        const Velocity& activationVelocity)
    {
        effects = {};
        effects.active = true;
        effects.startTime = now;
        effects.activationVelocity = activationVelocity;

        const float activationHorizontalSpeed = HorizontalSpeed(activationVelocity);
        // Climbing and stationary activation receive no forward push
        effects.pitchForceEligible = !climbing && activationHorizontalSpeed > 0.001f;
        if (effects.pitchForceEligible)
        {
            const float velocityAlpha = std::clamp(
                activationHorizontalSpeed / RagdollJumpTuning::PitchVelocityMaximum,
                0.0f,
                1.0f);
            effects.pitchScale = RagdollJumpTuning::PitchScaleAtMinimumVelocity
                + (RagdollJumpTuning::PitchScaleAtMaximumVelocity
                    - RagdollJumpTuning::PitchScaleAtMinimumVelocity) * velocityAlpha;
            effects.pitchDirectionX = activationVelocity.x / activationHorizontalSpeed;
            effects.pitchDirectionY = activationVelocity.y / activationHorizontalSpeed;
        }

        ProcessPostActivationEffects(ped, pedHandle, now);
    }

    // Keep the weapon in the right hand without attaching the supporting hand
    namespace WeaponAttachment
    {
        thread_local uintptr_t commandPed = 0;
        uintptr_t skipSupportingHand = 0;
        SafetyHookMid supportingHandHook;

        void Resolve(uintptr_t& hookAddress, uintptr_t& exitAddress)
        {
            auto distance = hook::pattern("0F 2F C2 0F 86 ? ? ? ? 81 8F 70 02 00 00 00 00 04 00");
            auto epilogue = hook::pattern("F3 0F 11 86 38 01 00 00 5F 5E 8B E5 5D C2 04 00");
            ptrdiff_t branchEnd = 9;
            ptrdiff_t displacement = 5;
            if (distance.empty())
            {
                distance = hook::pattern("0F 86 ? ? ? ? 8B 8F C4 07 00 00 D9 EE 81 8F 70 02 00 00 00 00 04 00");
                epilogue = hook::pattern("F3 0F 11 86 38 01 00 00 5F 5E 5B 8B E5 5D C2 04 00");
                branchEnd = 6;
                displacement = 2;
            }

            hookAddress = (uintptr_t)distance.get_first(0);
            exitAddress = (uintptr_t)epilogue.get_first(8);
            // Use the native rejection branch and its stack-cleanup path.
            const auto branchTarget = hookAddress + branchEnd
                + *reinterpret_cast<const int32_t*>(hookAddress + displacement);
            if (branchTarget != exitAddress)
                throw std::runtime_error("Unexpected ragdoll supporting-hand cleanup branch");
        }
    }

    // Native entry points used by the hooks
    struct HookTargets
    {
        uintptr_t startBehaviour = 0;
        uintptr_t eventConstructor = 0;
        uintptr_t eventClone = 0;
        uintptr_t eventDestructor = 0;
        uintptr_t waterPriority = 0;
        uintptr_t supportingHand = 0;
        uintptr_t supportingHandExit = 0;
    };

    uintptr_t ResolveEventDestructor(uintptr_t eventConstructor)
    {
        // Several event classes share this layout; the clone call selects our constructor.
        auto constructors = hook::pattern(
            "56 8B F1 E8 ? ? ? ? 8B 44 24 08 89 46 0C 8B 44 24 0C 89 46 10 "
            "8A 44 24 10 88 46 14 C7 06 ? ? ? ? 8B C6 5E C2 0C 00");
        ptrdiff_t constructorVtable = 31;
        if (constructors.empty())
        {
            constructors = hook::pattern("8B D1 E8 ? ? ? ? 8B 44 24 04 8B 4C 24 08 89 42 0C 8A 44 24 0C 88 42 14 C7 02 ? ? ? ? 89 4A 10 8B C2 C2 0C 00");
            constructorVtable = 27;
        }
        uintptr_t vtable = 0;
        for (size_t i = 0; i < constructors.size(); ++i)
        {
            const auto match = constructors.get(i);
            if ((uintptr_t)match.get<void>() == eventConstructor)
                vtable = *match.get<uintptr_t>(constructorVtable);
        }
        if (!vtable)
            throw std::runtime_error("Missing ragdoll event constructor layout");

        // Match the destructor body by the same vtable, without following the deleting wrapper.
        auto destructors = hook::pattern(
            "56 8B F1 8B 4E 10 C7 06 ? ? ? ? 85 C9 74 06 8B 01 6A 01 FF 10 8B CE 5E E9 ? ? ? ?");
        ptrdiff_t destructorVtable = 8;
        if (destructors.empty())
        {
            destructors = hook::pattern("56 8B F1 8B 4E 10 85 C9 C7 06 ? ? ? ? 74 ? 8B 01 8B 10 6A 01 FF D2 8B CE 5E E9 ? ? ? ?");
            destructorVtable = 10;
        }
        uintptr_t target = 0;
        for (size_t i = 0; i < destructors.size(); ++i)
        {
            const auto match = destructors.get(i);
            if (*match.get<uintptr_t>(destructorVtable) == vtable)
            {
                if (target)
                    throw std::runtime_error("Ambiguous ragdoll event destructor");
                target = (uintptr_t)match.get<void>();
            }
        }
        if (!target)
            throw std::runtime_error("Missing ragdoll event destructor");
        return target;
    }

    // Resolve required bindings before installing hooks
    void ResolveBindings(HookTargets& hooks)
    {
        auto pattern = hook::pattern("E8 ? ? ? ? 8A 88 6E 28 00 00 32 88 6C 28 00 00");
        if (!pattern.empty())
            CPlayerPed::GetControlFromPlayer = (decltype(CPlayerPed::GetControlFromPlayer))injector::GetBranchDestination(pattern.get_first(0)).as_int();
        else
        {
            pattern = hook::pattern("80 B9 18 02 00 00 00 75 0E 80 B9 19 02 00 00 00 74 05 E9 ? ? ? ? 33 C0 C3");
            CPlayerPed::GetControlFromPlayer = (decltype(CPlayerPed::GetControlFromPlayer))pattern.get_first(0);
        }

        pattern = find_pattern("55 8B EC 83 E4 F0 83 EC 48 56 57 E8 ? ? ? ? 84 C0 74 31", "55 8B EC 83 E4 F0 83 EC 34 53 56 57 E8 ? ? ? ? 84 C0 74 31");
        CTaskSimpleNM::CanUseRagdoll = (decltype(CTaskSimpleNM::CanUseRagdoll))pattern.get_first(0);

        pattern = find_pattern("E8 ? ? ? ? 8B D0 85 D2 74 ? FF B6 E0 00 00 00 0F B7 46 20 8D 4E 60", "E8 ? ? ? ? 85 C0 74 1E 8B 8E E0 00 00 00 51 0F B7 4E 22 8D 56 60");
        NaturalMotion::AllocateFromPoolInternal = (decltype(NaturalMotion::AllocateFromPoolInternal))injector::GetBranchDestination(pattern.get_first(0)).as_int();

        pattern = find_pattern("56 FF 74 24 0C 8B F1 FF 74 24 0C E8 ? ? ? ? FF 74 24 10 8D 4E 60", "8B 44 24 08 56 8B F1 8B 4C 24 08 50 51 8B CE E8 ? ? ? ? 8B 54 24 10 32 C0 52 8D 4E 60");
        CTaskSimpleNMHighFall::Constructor = (decltype(CTaskSimpleNMHighFall::Constructor))pattern.get_first(0);

        pattern = hook::pattern(
            "8B 86 E0 00 00 00 C1 E0 06 80 BF 19 02 00 00 00 51 8D 4C 24 0C 74 0A "
            "F3 0F 10 80 ? ? ? ? EB 08 F3 0F 10 80 ? ? ? ?");
        ptrdiff_t playerStrength = 27;
        ptrdiff_t aiStrength = 37;
        if (pattern.empty())
        {
            pattern = hook::pattern("8B 86 E0 00 00 00 8B 0D ? ? ? ? C1 E0 06 D9 80 ? ? ? ? D9 1C 24 51 EB 18 8B 96 E0 00 00 00 A1 ? ? ? ? C1 E2 06 D9 82 ? ? ? ?");
            playerStrength = 17;
            aiStrength = 43;
        }
        // The two branches read adjacent player/AI strengths from each 0x40-byte profile.
        if (*pattern.get_first<uintptr_t>(aiStrength) != *pattern.get_first<uintptr_t>(playerStrength) + sizeof(float))
            throw std::runtime_error("Unexpected ragdoll player/AI parameter layout");
        CTaskSimpleNMHighFall::ms_Parameters = (decltype(CTaskSimpleNMHighFall::ms_Parameters))*pattern.get_first<uintptr_t>(playerStrength);

        pattern = hook::pattern("E8 ? ? ? ? 6A 01 FF 35 ? ? ? ? 8D 4C 24 10 E8 ? ? ? ? 8B 86 E0");
        if (!pattern.empty())
            ART::MessageParams::Constructor = (decltype(ART::MessageParams::Constructor))injector::GetBranchDestination(pattern.get_first(0)).as_int();
        else
        {
            pattern = hook::pattern("56 68 C0 0C 00 00 8B F1 6A 00 56 C7 86 C0 0C 00 00 00 00 00 00");
            ART::MessageParams::Constructor = (decltype(ART::MessageParams::Constructor))pattern.get_first(0);
        }

        pattern = hook::pattern("E8 ? ? ? ? 8B 86 E0 00 00 00 C1 E0 06 80 BF");
        if (!pattern.empty())
            ART::MessageParams::addBool = (decltype(ART::MessageParams::addBool))injector::GetBranchDestination(pattern.get_first(0)).as_int();
        else
        {
            pattern = hook::pattern("56 8B F1 8B 96 C0 0C 00 00 83 FA 30 7D 7A");
            ART::MessageParams::addBool = (decltype(ART::MessageParams::addBool))pattern.get_first(0);
        }

        pattern = find_pattern("56 8B F1 8B 8E C0 0C 00 00 83 F9 30 7D 69", "56 8B F1 8B 96 C0 0C 00 00 83 FA 30 7D 7E");
        ART::MessageParams::addFloat = (decltype(ART::MessageParams::addFloat))pattern.get_first(0);

        pattern = find_pattern("8B 15 ? ? ? ? 53 8B 02 56 8B 74 24 0C 57 8B F9", "A1 ? ? ? ? 53 8B 5C 24 0C 55 8B 6C 24 0C 57 8B F9 8B 08 8B 41 0C");
        rage::fragInstNM::PostARTMessage = (decltype(rage::fragInstNM::PostARTMessage))pattern.get_first(0);

        pattern = hook::pattern("C7 81 C0 0C 00 00 00 00 00 00 C3");
        ART::MessageParams::Destructor = (decltype(ART::MessageParams::Destructor))pattern.get_first(0);

        pattern = find_pattern("81 EC E4 0C 00 00 A1 ? ? ? ? 33 C4 89 84 24 E0 0C 00 00 53 8B 9C 24 F0 0C 00 00", "81 EC E0 0C 00 00 A1 ? ? ? ? 33 C4 89 84 24 DC 0C 00 00 53 56 8B B4 24 EC 0C 00 00");
        NaturalMotion::PostNMTaskInternal = (decltype(NaturalMotion::PostNMTaskInternal))pattern.get_first(0);

        pattern = find_pattern("57 8B F9 8B 87 B4 07 00 00 85 C0 74 ? 83 78 64 00", "56 8B F1 8B 86 C4 07 00 00 85 C0 74 ? 83 78 64 00");
        NaturalMotion::GetRagdollComponentMatrixInternal = (decltype(NaturalMotion::GetRagdollComponentMatrixInternal))pattern.get_first(0);
        NaturalMotion::FragInstNMOffset = *pattern.get_first<int32_t>(5);

        auto startBehaviour = find_pattern("81 EC C8 0C 00 00 A1 ? ? ? ? 33 C4 89 84 24 C4 0C 00 00 56 8B F1 57 8B 86 E0 00 00 00", "81 EC C8 0C 00 00 A1 ? ? ? ? 33 C4 89 84 24 C4 0C 00 00 56 8B F1 8B 86 E0 00 00 00");
        auto eventClone = hook::pattern(
            "56 8B F1 8B 0D ? ? ? ? 57 E8 ? ? ? ? 8B F8 85 FF 74 25 "
            "8B 4E 10 85 C9 74 09 8B 11 FF 52 04 8B C8 EB 02 33 C9 "
            "0F B6 46 14 50 51 FF 76 0C 8B CF E8 ? ? ? ? 5F 5E C3");
        ptrdiff_t constructorCall = 50;
        if (eventClone.empty())
        {
            eventClone = hook::pattern("56 8B F1 8B 0D ? ? ? ? 57 E8 ? ? ? ? 8B F8 85 FF 74 28 83 7E 10 00 74 0C 8B 4E 10 8B 01 8B 50 04 FF D2 EB 02 33 C0 0F B6 4E 14 8B 56 0C 51 50 52 8B CF E8 ? ? ? ? 5F 5E C3");
            constructorCall = 53;
        }
        auto waterPriority = find_pattern("56 8B 74 24 08 57 8B 06 8B F9 8B CE FF 50 04 83 F8 09", "56 8B 74 24 08 8B 06 8B 50 04 57 8B F9 8B CE FF D2 83 F8 09");
        hooks.startBehaviour = (uintptr_t)startBehaviour.get_first(0);
        hooks.eventConstructor = injector::GetBranchDestination(eventClone.get_first(constructorCall)).as_int();
        hooks.eventClone = (uintptr_t)eventClone.get_first(0);
        hooks.waterPriority = (uintptr_t)waterPriority.get_first(0);
        hooks.eventDestructor = ResolveEventDestructor(hooks.eventConstructor);
        WeaponAttachment::Resolve(hooks.supportingHand, hooks.supportingHandExit);
    }

    // Install in dependency order and remove partial installations on failure
    void InstallHooks(const HookTargets& hooks)
    {
        CTaskSimpleNMHighFall::shStartBehaviour = safetyhook::create_inline(
            hooks.startBehaviour, NaturalMotion::StartBehaviourHook);
        if (!CTaskSimpleNMHighFall::shStartBehaviour)
            throw std::runtime_error("Failed to install ragdoll HighFall hook");

        CommandEvents::shDestructor = safetyhook::create_inline(hooks.eventDestructor, CommandEvents::DestructorHook);
        CommandEvents::shClone = safetyhook::create_inline(hooks.eventClone, CommandEvents::CloneHook);
        CommandEvents::shWaterPriority = safetyhook::create_inline(hooks.waterPriority, CommandEvents::WaterPriorityHook);
        CommandEvents::shConstructor = safetyhook::create_inline(hooks.eventConstructor, CommandEvents::ConstructorHook);
        if (!CommandEvents::shConstructor || !CommandEvents::shClone
            || !CommandEvents::shDestructor || !CommandEvents::shWaterPriority)
        {
            CommandEvents::ResetHooks();
            CTaskSimpleNMHighFall::shStartBehaviour.reset();
            throw std::runtime_error("Failed to install ragdoll event hooks");
        }

        WeaponAttachment::skipSupportingHand = hooks.supportingHandExit;
        WeaponAttachment::supportingHandHook = safetyhook::create_mid(hooks.supportingHand, [](SafetyHookContext& regs)
        {
            // Skip only this command's supporting-hand setup; EDI is the ped
            if (WeaponAttachment::commandPed && regs.edi == WeaponAttachment::commandPed)
                regs.eip = WeaponAttachment::skipSupportingHand;
        });
        if (!WeaponAttachment::supportingHandHook)
        {
            CommandEvents::ResetHooks();
            CTaskSimpleNMHighFall::shStartBehaviour.reset();
            throw std::runtime_error("Failed to install ragdoll supporting-hand hook");
        }
    }

    void Initialize()
    {
        if (!CPlayer::getLocalPlayerPed || !CTimer::m_snTimeInMilliseconds)
            throw std::runtime_error("Missing common ragdoll bindings");

        HookTargets hooks;
        ResolveBindings(hooks);
        InstallHooks(hooks);
    }

    // Per-frame Reload input and ragdoll activation
    void Process()
    {
        if (Natives::IsNetworkGameRunning())
        {
            ResetPlayerState();
            return;
        }

        const uintptr_t ped = CPlayer::getLocalPlayerPed ? CPlayer::getLocalPlayerPed() : 0;
        if (!ped)
        {
            ResetPlayerState();
            return;
        }

        Ped pedHandle = 0;
        Natives::GetPlayerChar(Natives::GetPlayerId(), &pedHandle);
        if (!pedHandle)
        {
            ResetPlayerState();
            return;
        }

        const uint32_t now = static_cast<uint32_t>(*CTimer::m_snTimeInMilliseconds);
        if (ped != currentPed || pedHandle != currentPedHandle)
        {
            ResetPlayerState();
            currentPed = ped;
            currentPedHandle = pedHandle;
        }
        if (NaturalMotion::pending.ped && NaturalMotion::pending.Expired(now))
            NaturalMotion::pending = {};
        ProcessPostActivationEffects(ped, pedHandle, now);

        const bool jumpActive = CPed::ComparePedTasks(ped, TaskID::TaskComplexJump);
        const bool jumpStarted = jumpActive && !jumpWasActive;
        if (!jumpActive)
        {
            ResetJumpState();
            return;
        }

        const uintptr_t control = CPlayerPed::GetControlFromPlayer(ped);
        if (!control)
        {
            ResetJumpState();
            return;
        }

        const bool jumpButtonDown =
            ReadButtonState(control, JumpInputOffset).current > ButtonDownThreshold;
        const ButtonState reloadButton = ReadButtonState(control, ReloadInputOffset);

        if (jumpStarted)
        {
            jumpStartTime = now;
            waitingForJumpRelease = jumpButtonDown;
            ragdollJumpRequested = false;
            highFallAcceptedThisJump = false;
        }
        jumpWasActive = true;

        // Jump and climb controls can be enabled independently
        const bool climbing = CPed::ComparePedTasks(ped, TaskID::TaskSimpleClimb);
        if ((climbing && !bPlayerClimbRagdollControl) || (!climbing && !bPlayerJumpRagdollControl))
        {
            ragdollJumpRequested = false;
            return;
        }

        if (highFallAcceptedThisJump)
            return;

        // Require Jump release before accepting a fresh Reload press
        if (waitingForJumpRelease)
        {
            if (jumpButtonDown)
                return;

            waitingForJumpRelease = false;
        }

        const uint32_t jumpElapsed = now - jumpStartTime;
        // Remember the press while waiting for native ragdoll eligibility.
        if (jumpElapsed >= RagdollJumpTuning::RagdollJumpTestMinTimeMs && reloadButton.IsJustPressed())
            ragdollJumpRequested = true;

        if (!ragdollJumpRequested
            || jumpElapsed < RagdollJumpTuning::RagdollJumpActivationMinTimeMs
            || !Natives::IsCharInAir(pedHandle))
        {
            return;
        }

        const Velocity activationVelocity = ReadVelocity(pedHandle);

        if (Natives::IsPedRagdoll(pedHandle))
        {
            ragdollJumpRequested = false;
            highFallAcceptedThisJump = true;
            return;
        }

        if (!CTaskSimpleNM::CanUseRagdoll(
                ped,
                eRagdollTriggerTypes::RAGDOLL_TRIGGER_SCRIPT,
                0,
                0.0f))
            return;

        // Preserve native defaults for fields not overridden by this feature
        auto parameters = CTaskSimpleNMHighFall::ms_Parameters[0];
        parameters.hfBodyStiffness = RagdollJumpTuning::NMHFall::hfBodyStiffness;
        parameters.hfLegRadius = RagdollJumpTuning::NMHFall::hfLegRadius;
        parameters.hfLegAngularSpeed = RagdollJumpTuning::NMHFall::hfLegAngularSpeed;
        parameters.hfArmPeriod = RagdollJumpTuning::NMHFall::hfArmPeriod;
        parameters.hfArmAmplitude = RagdollJumpTuning::NMHFall::hfArmAmplitude;

        {
            ScopedValue commandPed(WeaponAttachment::commandPed, ped);
            highFallAcceptedThisJump = NaturalMotion::SwitchToNMHighFall(
                ped,
                RagdollJumpTuning::NMHighFallMinimumTimeMs,
                RagdollJumpTuning::NMHighFallMaximumTimeMs,
                CTaskSimpleNMHighFall::eHighFallType::STANDARD,
                RagdollJumpTuning::NMHighFallEventTimeMs,
                parameters,
                GetArmsWindmillAdaptiveParameters(),
                GetPedalLegsParameters());
        }

        if (highFallAcceptedThisJump)
        {
            ragdollJumpRequested = false;
            StartPostActivationEffects(ped, pedHandle, now, climbing, activationVelocity);
        }
    }
}

// Settings, menu rows and startup registration
class PlayerJumpRagdollControl
{
public:
    PlayerJumpRagdollControl()
    {
        FusionFix::onInitEvent() += []()
        {
            CIniReader iniReader("");

            AirborneRagdoll::bPlayerClimbRagdollControl = iniReader.ReadInteger("MISC", "PlayerClimbRagdollControl", 0) != 0;

            FusionFixSettings.SetCallback(AirborneRagdoll::JumpRagdollPreference, [](int32_t value)
            {
                AirborneRagdoll::bPlayerJumpRagdollControl = value != 0;
            });

            AirborneRagdoll::Initialize();

            FusionFixSettings.Set(AirborneRagdoll::JumpRagdollPreference,
                FusionFixSettings.Get(AirborneRagdoll::JumpRagdollPreference));

            if constexpr (AirborneRagdoll::ShowRagdollMenuOption)
            {
                FusionFixSettings.AddToggle(CSettings::MenuScreen::Controls,
                    "Jump Ragdoll", AirborneRagdoll::JumpRagdollPreference);
                FusionFixSettings.AddToggle(CSettings::MenuScreen::TitleControls,
                    "Jump Ragdoll", AirborneRagdoll::JumpRagdollPreference);
            }

            FusionFix::onGameProcessEvent() += AirborneRagdoll::Process;
        };
    }
} PlayerJumpRagdollControl;
