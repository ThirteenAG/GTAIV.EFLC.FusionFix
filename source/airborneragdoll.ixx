module;

#include <common.hxx>
#include <algorithm>
#include <array>
#include <cmath>

export module airborneragdoll;

import common;
import comvars;
import natives;
import settings;

namespace AirborneRagdoll
{
    // Set false to hide the menu rows without disabling the saved preference or feature.
    constexpr bool ShowRagdollMenuOption = true;
    constexpr auto JumpRagdollPreference = "PREF_PLAYERJUMPRAGDOLL";

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

    namespace CommandEvents
    {
        struct OwnedEvent
        {
            void* event = nullptr;
            uintptr_t ped = 0;
        };

        // Track event clones too: the event history need not retain the original instance.
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
                // Water normally has lower priority than NM; only our events yield here.
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

    namespace NaturalMotion
    {
        constexpr ptrdiff_t PedIntelligenceOffset = 0x224;
        constexpr ptrdiff_t FragInstNMOffset = 0x7B4;

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

            // Native events can clone the task, so retain the profile by ped until startup.
            const auto profile = pending;
            pending = {};
            {
                // The native reader uses a shared table; restore it before posting limb messages.
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
            // PostNMTask takes ownership, including destroying a rejected task.
            const bool posted = PostNMTaskInternal(ped, eventTime, false, task);
            if (!posted && pending.ped == ped)
                pending = {};
            return posted;
        }
    }

    namespace RagdollJumpTuning
    {
        constexpr uint32_t ActivationMinimumMs = 200;
        constexpr uint32_t NMHighFallMinimumTimeMs = 100;
        constexpr uint32_t NMHighFallMaximumTimeMs = 10000;
        constexpr uint32_t NMHighFallEventTimeMs = 10000;

        namespace NMHFall
        {
            constexpr float hfBodyStiffness = 12.0f;
            constexpr float hfLegRadius = 0.4f;
            constexpr float hfLegAngularSpeed = 7.85f;
            constexpr float hfArmPeriod = 0.8004058f;
            constexpr float hfArmAmplitude = 1.5f;
        }

        namespace armsWindmillAdaptive
        {
            constexpr float armStiffness = 12.0f;
            constexpr float bodyStiffness = 12.0f;
            constexpr float period = 0.8004058f;
            constexpr float amplitude = 1.5f;
            constexpr float phase = 3.1f;
            constexpr bool disableOnImpact = false;
        }

        namespace pedalLegs
        {
            constexpr bool backPedal = false;
            constexpr float legStiffness = 12.0f;
            constexpr bool pedalLeftLeg = true;
            constexpr bool pedalRightLeg = true;
            constexpr float radius = 0.4f;
            constexpr float angularSpeed = 7.85f;
            constexpr float pedalOffset = 0.0f;
            constexpr float speedAsymmetry = 0.4f;
            constexpr bool adaptivePedal4Dragging = false;
            constexpr float radiusVariance = 0.4f;
            constexpr float legAngleVariance = 0.5f;
        }

        constexpr uint32_t BallisticCorrectionDurationMs = 300;
        constexpr uint32_t PitchForceDurationMs = 300;
        constexpr uint32_t BehaviourControlDurationMs = 300;
        constexpr uint32_t EffectWaitLimitMs = 1000;
        constexpr float ObservedJumpGravity = 10.24f;
        constexpr uint32_t PitchForceType = 1;
        constexpr float PitchMagnitude = 800.0f;
        constexpr float PitchVelocityMaximum = 4.0f;
        constexpr float PitchScaleAtMinimumVelocity = 0.5f;
        constexpr float PitchScaleAtMaximumVelocity = 1.0f;
    }

    constexpr ptrdiff_t ReloadPreviousOffset = 0x286C;
    constexpr ptrdiff_t ReloadCurrentOffset = 0x286E;
    constexpr ptrdiff_t JumpCurrentOffset = 0x2846;
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
    bool highFallAcceptedThisJump = false;
    bool bPlayerJumpRagdollControl = false;
    bool bPlayerClimbRagdollControl = false;
    uint32_t jumpStartTime = 0;
    uintptr_t currentPed = 0;
    Ped currentPedHandle = 0;

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

    ButtonState ReadButtonState(uintptr_t control, ptrdiff_t previousOffset, ptrdiff_t currentOffset)
    {
        return {
            *reinterpret_cast<const uint8_t*>(control + previousOffset),
            *reinterpret_cast<const uint8_t*>(control + currentOffset)
        };
    }

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

    void ResetJumpState()
    {
        jumpWasActive = false;
        waitingForJumpRelease = false;
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

    void ProcessPostActivationEffects(uintptr_t ped, Ped pedHandle, uint32_t now)
    {
        if (!effects.active)
            return;

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

            if (elapsed <= RagdollJumpTuning::BallisticCorrectionDurationMs)
            {
                const float expectedVelocityZ = effects.activationVelocity.z
                    - RagdollJumpTuning::ObservedJumpGravity * (static_cast<float>(elapsed) / 1000.0f);
                if (velocity.z > expectedVelocityZ)
                    Natives::SetCharVelocity(pedHandle, velocity.x, velocity.y, expectedVelocityZ);
            }

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

    namespace WeaponAttachment
    {
        thread_local uintptr_t commandPed = 0;
        uintptr_t skipSupportingHand = 0;
        SafetyHookMid supportingHandHook;

        bool Initialize()
        {
            auto distance = hook::pattern("0F 2F C2 0F 86 ? ? ? ? 81 8F 70 02 00 00");
            auto epilogue = hook::pattern("F3 0F 11 86 38 01 00 00 5F 5E 8B E5");
            if (distance.size() != 1 || epilogue.size() != 1)
                return false;

            const auto distanceAddress = reinterpret_cast<uintptr_t>(distance.get_first());
            const auto exitAddress = reinterpret_cast<uintptr_t>(epilogue.get_first(8));
            const auto base = reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
            if (distanceAddress - base != 0x7669D0 || exitAddress - base != 0x766A6E)
                return false;

            const auto branchTarget = distanceAddress + 9
                + *reinterpret_cast<const int32_t*>(distanceAddress + 5);
            if (branchTarget != exitAddress)
                return false;

            skipSupportingHand = branchTarget;
            supportingHandHook = safetyhook::create_mid(distanceAddress, [](SafetyHookContext& regs)
            {
                // EDI holds the ped; right-hand attachments are already complete at this site.
                if (commandPed && regs.edi == commandPed)
                    regs.eip = skipSupportingHand;
            });
            return static_cast<bool>(supportingHandHook);
        }
    }

    enum class AddressKind { Entry, Call, Pointer };

    uintptr_t ResolveAddress(const char* signature, uintptr_t expectedRva,
        AddressKind kind = AddressKind::Entry, ptrdiff_t offset = 0)
    {
        auto pattern = hook::pattern(signature);
        if (pattern.size() != 1)
            return 0;

        auto address = reinterpret_cast<uintptr_t>(pattern.get_first(offset));
        if (kind == AddressKind::Call)
            address = injector::GetBranchDestination(address).as_int();
        else if (kind == AddressKind::Pointer)
            address = *reinterpret_cast<const uintptr_t*>(address);

        // A signature match alone does not validate the hard-coded field offsets and registers.
        const auto base = reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
        return address - base == expectedRva ? address : 0;
    }

    bool Initialize()
    {
        if (!CPlayer::getLocalPlayerPed || !CTimer::m_snTimeInMilliseconds)
            return false;

        CPlayerPed::GetControlFromPlayer = reinterpret_cast<decltype(CPlayerPed::GetControlFromPlayer)>(ResolveAddress(
            "E8 ? ? ? ? 8A 88 6E 28 00 00 32 88 6C 28 00 00", 0x62B350, AddressKind::Call));
        CTaskSimpleNM::CanUseRagdoll = reinterpret_cast<decltype(CTaskSimpleNM::CanUseRagdoll)>(ResolveAddress(
            "55 8B EC 83 E4 F0 83 EC 48 56 57 E8 ? ? ? ? 84 C0 74 31", 0x8DBE60));
        NaturalMotion::AllocateFromPoolInternal = reinterpret_cast<decltype(NaturalMotion::AllocateFromPoolInternal)>(ResolveAddress(
            "E8 ? ? ? ? 8B D0 85 D2 74 ? FF B6 E0 00 00 00 0F B7 46 20 8D 4E 60", 0x99940, AddressKind::Call));
        CTaskSimpleNMHighFall::Constructor = reinterpret_cast<decltype(CTaskSimpleNMHighFall::Constructor)>(ResolveAddress(
            "56 FF 74 24 0C 8B F1 FF 74 24 0C E8 ? ? ? ? FF 74 24 10 8D 4E 60", 0x8DA010));
        const auto startBehaviour = ResolveAddress(
            "81 EC C8 0C 00 00 A1 ? ? ? ? 33 C4 89 84 24 C4 0C 00 00 56 8B F1 57 8B 86 E0 00 00 00", 0x8E95A0);
        constexpr auto eventCloneSignature = "0F B6 46 14 50 51 FF 76 0C 8B CF E8 ? ? ? ? 5F 5E C3";
        const auto eventConstructor = ResolveAddress(eventCloneSignature, 0x7459A0, AddressKind::Call, 11);
        const auto eventClone = ResolveAddress(
            eventCloneSignature, 0x74A110, AddressKind::Entry, -0x27);
        uintptr_t eventDestructor = 0;
        if (eventConstructor)
        {
            const auto base = reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
            const auto vtable = *reinterpret_cast<const uintptr_t*>(eventConstructor + 0x1F);
            if (vtable - base == 0xAAE674)
            {
                const auto deletingDestructor = *reinterpret_cast<const uintptr_t*>(vtable);
                if (deletingDestructor - base == 0x746A90
                    && *reinterpret_cast<const uint8_t*>(deletingDestructor + 3) == 0xE8)
                {
                    const auto target = injector::GetBranchDestination(deletingDestructor + 3).as_int();
                    if (target - base == 0x746210)
                        eventDestructor = target;
                }
            }
        }
        const auto waterPriority = ResolveAddress(
            "56 8B 74 24 08 57 8B 06 8B F9 8B CE FF 50 04 83 F8 09", 0x74B850);
        CTaskSimpleNMHighFall::ms_Parameters = reinterpret_cast<decltype(CTaskSimpleNMHighFall::ms_Parameters)>(ResolveAddress(
            "8B 86 E0 00 00 00 C1 E0 06 80 BF 19 02 00 00 00 51 8D 4C 24 0C 74 0A", 0x131D028, AddressKind::Pointer, 27));
        ART::MessageParams::Constructor = reinterpret_cast<decltype(ART::MessageParams::Constructor)>(ResolveAddress(
            "E8 ? ? ? ? 6A 01 FF 35 ? ? ? ? 8D 4C 24 10 E8 ? ? ? ? 8B 86 E0", 0x41F860, AddressKind::Call));
        ART::MessageParams::addBool = reinterpret_cast<decltype(ART::MessageParams::addBool)>(ResolveAddress(
            "E8 ? ? ? ? 8B 86 E0 00 00 00 C1 E0 06 80 BF", 0x41FB50, AddressKind::Call));
        ART::MessageParams::addFloat = reinterpret_cast<decltype(ART::MessageParams::addFloat)>(ResolveAddress(
            "56 8B F1 8B 8E C0 0C 00 00 83 F9 30 7D 69", 0x41FAD0));
        rage::fragInstNM::PostARTMessage = reinterpret_cast<decltype(rage::fragInstNM::PostARTMessage)>(ResolveAddress(
            "8B 15 ? ? ? ? 53 8B 02 56 8B 74 24 0C 57 8B F9", 0x41CD70));
        ART::MessageParams::Destructor = reinterpret_cast<decltype(ART::MessageParams::Destructor)>(ResolveAddress(
            "C7 81 C0 0C 00 00 00 00 00 00 C3", 0x41F890));
        NaturalMotion::PostNMTaskInternal = reinterpret_cast<decltype(NaturalMotion::PostNMTaskInternal)>(ResolveAddress(
            "81 EC E4 0C 00 00 A1 ? ? ? ? 33 C4 89 84 24 E0 0C 00 00", 0x7B18A0));
        NaturalMotion::GetRagdollComponentMatrixInternal = reinterpret_cast<decltype(NaturalMotion::GetRagdollComponentMatrixInternal)>(ResolveAddress(
            "57 8B F9 8B 87 B4 07 00 00 85 C0 74 ? 83 78 64 00", 0x5E8810));

        if (!startBehaviour
            || !eventConstructor || !eventClone || !eventDestructor || !waterPriority
            || !CPlayerPed::GetControlFromPlayer
            || !CTaskSimpleNM::CanUseRagdoll
            || !NaturalMotion::AllocateFromPoolInternal
            || !CTaskSimpleNMHighFall::Constructor
            || !CTaskSimpleNMHighFall::ms_Parameters
            || !ART::MessageParams::Constructor
            || !ART::MessageParams::addBool
            || !ART::MessageParams::addFloat
            || !rage::fragInstNM::PostARTMessage
            || !ART::MessageParams::Destructor
            || !NaturalMotion::PostNMTaskInternal
            || !NaturalMotion::GetRagdollComponentMatrixInternal)
        {
            return false;
        }

        CTaskSimpleNMHighFall::shStartBehaviour = safetyhook::create_inline(
            startBehaviour, NaturalMotion::StartBehaviourHook);
        if (!CTaskSimpleNMHighFall::shStartBehaviour)
            return false;

        CommandEvents::shDestructor = safetyhook::create_inline(eventDestructor, CommandEvents::DestructorHook);
        CommandEvents::shClone = safetyhook::create_inline(eventClone, CommandEvents::CloneHook);
        CommandEvents::shWaterPriority = safetyhook::create_inline(waterPriority, CommandEvents::WaterPriorityHook);
        CommandEvents::shConstructor = safetyhook::create_inline(eventConstructor, CommandEvents::ConstructorHook);
        if (!CommandEvents::shConstructor || !CommandEvents::shClone
            || !CommandEvents::shDestructor || !CommandEvents::shWaterPriority)
        {
            CommandEvents::ResetHooks();
            CTaskSimpleNMHighFall::shStartBehaviour.reset();
            return false;
        }

        if (!WeaponAttachment::Initialize())
        {
            CommandEvents::ResetHooks();
            CTaskSimpleNMHighFall::shStartBehaviour.reset();
            return false;
        }
        return true;
    }

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
            *reinterpret_cast<const uint8_t*>(control + JumpCurrentOffset) > ButtonDownThreshold;
        const ButtonState reloadButton = ReadButtonState(
            control,
            ReloadPreviousOffset,
            ReloadCurrentOffset);

        if (jumpStarted)
        {
            jumpStartTime = now;
            waitingForJumpRelease = jumpButtonDown;
            highFallAcceptedThisJump = false;
        }
        jumpWasActive = true;

        const bool climbing = CPed::ComparePedTasks(ped, TaskID::TaskSimpleClimb);
        if ((climbing && !bPlayerClimbRagdollControl) || (!climbing && !bPlayerJumpRagdollControl))
            return;

        if (highFallAcceptedThisJump)
            return;

        if (waitingForJumpRelease)
        {
            if (jumpButtonDown)
                return;

            waitingForJumpRelease = false;
        }

        if (now - jumpStartTime < RagdollJumpTuning::ActivationMinimumMs
            || !reloadButton.IsJustPressed()
            || !Natives::IsCharInAir(pedHandle))
        {
            return;
        }

        const Velocity activationVelocity = ReadVelocity(pedHandle);

        if (Natives::IsPedRagdoll(pedHandle))
        {
            highFallAcceptedThisJump = true;
            return;
        }

        if (!CTaskSimpleNM::CanUseRagdoll(
                ped,
                eRagdollTriggerTypes::RAGDOLL_TRIGGER_SCRIPT,
                0,
                0.0f))
            return;

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
            StartPostActivationEffects(ped, pedHandle, now, climbing, activationVelocity);
    }
}

class PlayerJumpRagdollControl
{
public:
    PlayerJumpRagdollControl()
    {
        FusionFix::onBeforeInitEvent() += []()
        {
            CIniReader iniReader("");

            // [MISC]
            const bool legacyJumpEnabled = iniReader.ReadInteger("MISC", "PlayerJumpRagdollControl", 0) != 0;
            AirborneRagdoll::bPlayerClimbRagdollControl = iniReader.ReadInteger("MISC", "PlayerClimbRagdollControl", 0) != 0;

            const auto preference = FusionFixSettings.RegisterPreference(
                AirborneRagdoll::JumpRagdollPreference, 1, legacyJumpEnabled,
                "MISC", "PlayerJumpRagdollControl", [](int32_t value)
                {
                    AirborneRagdoll::bPlayerJumpRagdollControl = value != 0;
                });
            if (preference < 0)
                return;

            FusionFix::onInitEvent() += [preference]()
            {
                if (!AirborneRagdoll::Initialize())
                    return;

                // Persist the resolved preference and apply it through its callback.
                FusionFixSettings.Set(preference, FusionFixSettings.Get(preference));

                if constexpr (AirborneRagdoll::ShowRagdollMenuOption)
                {
                    FusionFixSettings.AddToggle(CSettings::MenuScreen::Controls,
                        "FF_JUMPRAGDOLL", AirborneRagdoll::JumpRagdollPreference);
                    FusionFixSettings.AddToggle(CSettings::MenuScreen::TitleControls,
                        "FF_JUMPRAGDOLL", AirborneRagdoll::JumpRagdollPreference);
                }

                FusionFix::onGameProcessEvent() += AirborneRagdoll::Process;
            };
        };
    }
} PlayerJumpRagdollControl;
