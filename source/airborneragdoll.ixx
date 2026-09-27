module;

#include <common.hxx>
#include <algorithm>
#include <cmath>

export module airborneragdoll;

import common;
import comvars;
import natives;

namespace AirborneRagdoll
{
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
    uint32_t jumpStartTime = 0;

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

    void ProcessPostActivationEffects(uintptr_t ped, Ped pedHandle, uint32_t now)
    {
        if (!effects.active)
            return;

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

    void Process()
    {
        if (Natives::IsNetworkGameRunning())
        {
            ResetPostActivationEffects();
            ResetJumpState();
            return;
        }

        const uintptr_t ped = CPlayer::getLocalPlayerPed ? CPlayer::getLocalPlayerPed() : 0;
        if (!ped || !NaturalMotion::NMHighFallBindingsAvailable())
        {
            ResetPostActivationEffects();
            ResetJumpState();
            return;
        }

        Ped pedHandle = 0;
        Natives::GetPlayerChar(Natives::GetPlayerId(), &pedHandle);
        if (!pedHandle)
        {
            ResetPostActivationEffects();
            ResetJumpState();
            return;
        }

        const uint32_t now = static_cast<uint32_t>(*CTimer::m_snTimeInMilliseconds);
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

        const bool climbing = CPed::ComparePedTasks(ped, TaskID::TaskSimpleClimb);
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

        highFallAcceptedThisJump = NaturalMotion::SwitchToNMHighFall(
            ped,
            RagdollJumpTuning::NMHighFallMinimumTimeMs,
            RagdollJumpTuning::NMHighFallMaximumTimeMs,
            CTaskSimpleNMHighFall::eHighFallType::STANDARD,
            RagdollJumpTuning::NMHighFallEventTimeMs,
            parameters,
            GetArmsWindmillAdaptiveParameters(),
            GetPedalLegsParameters());

        if (highFallAcceptedThisJump)
            StartPostActivationEffects(ped, pedHandle, now, climbing, activationVelocity);
    }
}

class PlayerJumpRagdollControl
{
public:
    PlayerJumpRagdollControl()
    {
        FusionFix::onInitEvent() += []()
        {
            CIniReader iniReader("");
            if (iniReader.ReadInteger("MISC", "PlayerJumpRagdollControl", 1))
                FusionFix::onGameProcessEvent() += AirborneRagdoll::Process;
        };
    }
} PlayerJumpRagdollControl;
