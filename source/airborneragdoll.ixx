module;

#include <common.hxx>
#include <algorithm>
#include <array>
#include <cmath>
#include <format>

export module airborneragdoll;

import common;
import comvars;
import natives;
import settings;

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

    // Input records of the player's controls: the values are stored XORed with the byte at +4, the current one at +6,
    // the previous one at +7
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

    // The HighFall profile of the ragdoll we posted, applied when its task starts
    struct PendingHighFall
    {
        uintptr_t ped = 0;
        uint32_t postTime = 0;
        CTaskSimpleNMHighFall::Parameters highFall{};
        NaturalMotion::armsWindmillAdaptive::Parameters arms{};
        NaturalMotion::pedalLegs::Parameters legs{};
    } pending;

    bool jumpWasActive = false;
    bool waitingForJumpRelease = false;
    bool ragdollRequested = false;
    bool highFallAcceptedThisJump = false;
    uint32_t jumpStartTime = 0;
    uintptr_t currentPed = 0;

    // The ped and task being posted: their event gets normal priority and is tracked, so swimming can replace it
    uintptr_t postingPed = 0;
    void* postingTask = nullptr;
    std::array<std::pair<void*, uintptr_t>, 32> ownedEvents{};
    uintptr_t waterHandoffPed = 0;

    SafetyHookInline shStartBehaviour, shEventConstructor, shEventClone, shEventDestructor, shWaterPriority;
    SafetyHookMid shSupportingHand;
    uintptr_t supportingHandExit = 0;

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

    ButtonState ReadButtonState(uintptr_t control, ptrdiff_t inputOffset)
    {
        const auto* input = reinterpret_cast<const uint8_t*>(control + inputOffset);
        return { static_cast<uint8_t>(input[7] ^ input[4]), static_cast<uint8_t>(input[6] ^ input[4]) };
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

    void PostHighFallBehaviours(
        uintptr_t ped,
        const NaturalMotion::armsWindmillAdaptive::Parameters& arms,
        const NaturalMotion::pedalLegs::Parameters& legs)
    {
        void* fragInstNM = *reinterpret_cast<void**>(ped + NaturalMotion::FragInstNMOffset);
        if (!fragInstNM)
            return;

        alignas(16) uint8_t params[ART::MessageParams::Size]{};
        ART::MessageParams::Constructor(params);
        ART::MessageParams::addBool(params, "start", true);
        ART::MessageParams::addFloat(params, "armStiffness", arms.armStiffness);
        ART::MessageParams::addFloat(params, "bodyStiffness", arms.bodyStiffness);
        ART::MessageParams::addFloat(params, "period", arms.period);
        ART::MessageParams::addFloat(params, "amplitude", arms.amplitude);
        ART::MessageParams::addFloat(params, "phase", arms.phase);
        ART::MessageParams::addBool(params, "disableOnImpact", arms.disableOnImpact);
        rage::fragInstNM::PostARTMessage(fragInstNM, "armsWindmillAdaptive", params);
        ART::MessageParams::Destructor(params);

        ART::MessageParams::Constructor(params);
        ART::MessageParams::addBool(params, "start", true);
        ART::MessageParams::addBool(params, "backPedal", legs.backPedal);
        ART::MessageParams::addFloat(params, "legStiffness", legs.legStiffness);
        ART::MessageParams::addBool(params, "pedalLeftLeg", legs.pedalLeftLeg);
        ART::MessageParams::addBool(params, "pedalRightLeg", legs.pedalRightLeg);
        ART::MessageParams::addFloat(params, "radius", legs.radius);
        ART::MessageParams::addFloat(params, "angularSpeed", legs.angularSpeed);
        ART::MessageParams::addFloat(params, "pedalOffset", legs.pedalOffset);
        ART::MessageParams::addFloat(params, "speedAsymmetry", legs.speedAsymmetry);
        ART::MessageParams::addBool(params, "adaptivePedal4Dragging", legs.adaptivePedal4Dragging);
        ART::MessageParams::addFloat(params, "radiusVariance", legs.radiusVariance);
        ART::MessageParams::addFloat(params, "legAngleVariance", legs.legAngleVariance);
        rage::fragInstNM::PostARTMessage(fragInstNM, "pedalLegs", params);
        ART::MessageParams::Destructor(params);
    }

    // The game posts the task as an event and starts it later: our profile replaces the shared STANDARD one for
    // that one call
    void __fastcall StartBehaviour(void* task, void* edx, uintptr_t ped)
    {
        if (pending.ped && *CTimer::m_snTimeInMilliseconds - pending.postTime > RagdollJumpTuning::NMHighFallEventTimeMs)
            pending = {};

        if (!pending.ped || ped != pending.ped)
        {
            shStartBehaviour.unsafe_fastcall(task, edx, ped);
            return;
        }

        const auto profile = pending;
        pending = {};
        const auto standard = CTaskSimpleNMHighFall::ms_Parameters[0];
        CTaskSimpleNMHighFall::ms_Parameters[0] = profile.highFall;
        shStartBehaviour.unsafe_fastcall(task, edx, ped);
        CTaskSimpleNMHighFall::ms_Parameters[0] = standard;
        PostHighFallBehaviours(ped, profile.arms, profile.legs);
    }

    std::pair<void*, uintptr_t>* FindEvent(void* event)
    {
        auto it = std::find_if(ownedEvents.begin(), ownedEvents.end(), [event](auto& entry) { return entry.first == event; });
        return it != ownedEvents.end() ? &*it : nullptr;
    }

    void TrackEvent(void* event, uintptr_t ped)
    {
        if (!event)
            return;
        auto entry = FindEvent(event);
        if (!entry)
            entry = FindEvent(nullptr);
        if (entry)
            *entry = { event, ped };
    }

    // A script event would outrank swimming and keep the player ragdolled in water
    void* __fastcall EventConstructor(void* event, void* edx, uint32_t time, void* task, bool scriptEvent)
    {
        const bool ours = postingPed && task == postingTask;
        auto result = shEventConstructor.unsafe_fastcall<void*>(event, edx, time, task, ours ? false : scriptEvent);
        if (ours)
            TrackEvent(result, postingPed);
        return result;
    }

    void* __fastcall EventClone(void* event, void* edx)
    {
        auto result = shEventClone.unsafe_fastcall<void*>(event, edx);
        if (auto entry = FindEvent(event))
            TrackEvent(result, entry->second);
        return result;
    }

    uintptr_t __fastcall EventDestructor(void* event, void* edx)
    {
        if (auto entry = FindEvent(event))
            *entry = {};
        return shEventDestructor.unsafe_fastcall<uintptr_t>(event, edx);
    }

    // Swimming takes over from our HighFall once the player is in water
    bool __fastcall WaterPriority(void* waterEvent, void* edx, void* otherEvent)
    {
        auto entry = otherEvent ? FindEvent(otherEvent) : nullptr;
        if (entry && entry->second == CPlayer::getLocalPlayerPed() && !Natives::IsNetworkGameRunning()
            && (*reinterpret_cast<const uint8_t*>(entry->second + 0x29C) & 4) != 0
            && CPed::ComparePedTasks(entry->second, TaskID::TaskSimpleNMHighFall))
        {
            waterHandoffPed = entry->second;
            return true;
        }
        return shWaterPriority.unsafe_fastcall<bool>(waterEvent, edx, otherEvent);
    }

    bool SwitchToNMHighFall(uintptr_t ped, const CTaskSimpleNMHighFall::Parameters& parameters)
    {
        auto taskPool = CTask::GetTaskPool();
        if (!taskPool)
            return false;

        void* taskStorage = NaturalMotion::AllocateFromPoolInternal(taskPool);
        if (!taskStorage)
            return false;

        void* task = CTaskSimpleNMHighFall::Constructor(taskStorage, RagdollJumpTuning::NMHighFallMinimumTimeMs,
            RagdollJumpTuning::NMHighFallMaximumTimeMs, nullptr, CTaskSimpleNMHighFall::eHighFallType::STANDARD);
        if (!task)
            return false;

        pending = { ped, *CTimer::m_snTimeInMilliseconds, parameters, GetArmsWindmillAdaptiveParameters(), GetPedalLegsParameters() };
        waterHandoffPed = 0;
        postingPed = ped;
        postingTask = task;
        // The game owns the task from here, also when it rejects it
        const bool posted = NaturalMotion::PostNMTaskInternal(ped, RagdollJumpTuning::NMHighFallEventTimeMs, false, task);
        postingPed = 0;
        postingTask = nullptr;
        if (!posted)
            pending = {};
        return posted;
    }

    void ResetJumpState()
    {
        jumpWasActive = false;
        waitingForJumpRelease = false;
        ragdollRequested = false;
        highFallAcceptedThisJump = false;
    }

    void ResetPostActivationEffects()
    {
        effects = {};
    }

    void ResetPlayerState()
    {
        ResetJumpState();
        ResetPostActivationEffects();
        ownedEvents = {};
        waterHandoffPed = 0;
        pending = {};
        currentPed = 0;
    }

    void ProcessPostActivationEffects(uintptr_t ped, Ped pedHandle, uint32_t now)
    {
        if (!effects.active)
            return;

        const uint32_t elapsed = now - effects.startTime;
        if (waterHandoffPed == ped || elapsed > RagdollJumpTuning::EffectWaitLimitMs)
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
                PostHighFallBehaviours(ped, GetArmsWindmillAdaptiveParameters(), GetPedalLegsParameters());

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
        // 0 off, 1 while jumping, 2 also while climbing
        static auto mode = FusionFixSettings.GetRef("PREF_JUMPRAGDOLL");
        if (!mode->get() || Natives::IsNetworkGameRunning())
        {
            ResetPlayerState();
            return;
        }

        const uintptr_t ped = CWorld::FindLocalPlayer();
        Ped pedHandle = 0;
        if (ped)
            Natives::GetPlayerChar(Natives::GetPlayerId(), &pedHandle);
        if (!ped || !pedHandle)
        {
            ResetPlayerState();
            return;
        }
        if (ped != currentPed)
        {
            ResetPlayerState();
            currentPed = ped;
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

        const bool jumpButtonDown = ReadButtonState(control, JumpInputOffset).current > ButtonDownThreshold;
        const ButtonState reloadButton = ReadButtonState(control, ReloadInputOffset);

        if (jumpStarted)
        {
            jumpStartTime = now;
            waitingForJumpRelease = jumpButtonDown;
            ragdollRequested = false;
            highFallAcceptedThisJump = false;
        }
        jumpWasActive = true;

        const bool climbing = CPed::ComparePedTasks(ped, TaskID::TaskSimpleClimb);
        if (climbing && mode->get() != 2)
        {
            ragdollRequested = false;
            return;
        }

        if (highFallAcceptedThisJump)
            return;

        if (waitingForJumpRelease)
        {
            if (jumpButtonDown)
                return;

            waitingForJumpRelease = false;
        }

        // A press is kept until the ragdoll is possible
        const uint32_t jumpElapsed = now - jumpStartTime;
        if (jumpElapsed >= RagdollJumpTuning::ActivationMinimumMs && reloadButton.IsJustPressed())
            ragdollRequested = true;

        if (!ragdollRequested || !Natives::IsCharInAir(pedHandle))
            return;

        if (Natives::IsPedRagdoll(pedHandle))
        {
            ragdollRequested = false;
            highFallAcceptedThisJump = true;
            return;
        }

        if (!CTaskSimpleNM::CanUseRagdoll(ped, eRagdollTriggerTypes::RAGDOLL_TRIGGER_SCRIPT, 0, 0.0f))
            return;

        const Velocity activationVelocity = ReadVelocity(pedHandle);

        auto parameters = CTaskSimpleNMHighFall::ms_Parameters[0];
        parameters.hfBodyStiffness = RagdollJumpTuning::NMHFall::hfBodyStiffness;
        parameters.hfLegRadius = RagdollJumpTuning::NMHFall::hfLegRadius;
        parameters.hfLegAngularSpeed = RagdollJumpTuning::NMHFall::hfLegAngularSpeed;
        parameters.hfArmPeriod = RagdollJumpTuning::NMHFall::hfArmPeriod;
        parameters.hfArmAmplitude = RagdollJumpTuning::NMHFall::hfArmAmplitude;

        highFallAcceptedThisJump = SwitchToNMHighFall(ped, parameters);
        if (highFallAcceptedThisJump)
        {
            ragdollRequested = false;
            StartPostActivationEffects(ped, pedHandle, now, climbing, activationVelocity);
        }
    }

    std::string PatternBytes(uint32_t value)
    {
        return std::format("{:02X} {:02X} {:02X} {:02X}", value & 0xFF, (value >> 8) & 0xFF, (value >> 16) & 0xFF, value >> 24);
    }

    void InstallHooks()
    {
        auto pattern = find_pattern("81 EC C8 0C 00 00 A1 ? ? ? ? 33 C4 89 84 24 C4 0C 00 00 56 8B F1 57 8B 86 E0 00 00 00",
            "81 EC C8 0C 00 00 A1 ? ? ? ? 33 C4 89 84 24 C4 0C 00 00 56 8B F1 8B 86 E0 00 00 00");
        shStartBehaviour = safetyhook::create_inline(pattern.get_first(0), StartBehaviour);

        // CEventSwitch2NM: its clone calls the constructor, whose vtable leads to the destructor
        uintptr_t eventConstructor = 0;
        pattern = hook::pattern("56 8B F1 8B 0D ? ? ? ? 57 E8 ? ? ? ? 8B F8 85 FF 74 25 8B 4E 10 85 C9 74 09 8B 11 FF 52 04 8B C8 EB 02 33 C9 0F B6 46 14 50 51 FF 76 0C 8B CF E8 ? ? ? ? 5F 5E C3");
        if (!pattern.empty()) // 1.2.0.32, 1.2.0.43, 1.2.0.59
        {
            eventConstructor = injector::GetBranchDestination(pattern.get_first(50)).as_int();
            auto vtable = *hook::range_pattern(eventConstructor, eventConstructor + 0x30, "C7 06 ? ? ? ? 8B C6").get_first<uint32_t>(2);
            auto destructor = hook::pattern("56 8B F1 8B 4E 10 C7 06 " + PatternBytes(vtable) + " 85 C9");
            shEventDestructor = safetyhook::create_inline(destructor.get_first(0), EventDestructor);
        }
        else // 1.0.7.0, 1.0.8.0, 1.1.2.0, 1.1.3.0
        {
            pattern = hook::pattern("56 8B F1 8B 0D ? ? ? ? 57 E8 ? ? ? ? 8B F8 85 FF 74 28 83 7E 10 00 74 0C 8B 4E 10 8B 01 8B 50 04 FF D2 EB 02 33 C0 0F B6 4E 14 8B 56 0C 51 50 52 8B CF E8 ? ? ? ? 5F 5E C3");
            eventConstructor = injector::GetBranchDestination(pattern.get_first(53)).as_int();
            auto vtable = *hook::range_pattern(eventConstructor, eventConstructor + 0x30, "C7 02 ? ? ? ? 89 4A 10").get_first<uint32_t>(2);
            auto destructor = hook::pattern("56 8B F1 8B 4E 10 85 C9 C7 06 " + PatternBytes(vtable));
            shEventDestructor = safetyhook::create_inline(destructor.get_first(0), EventDestructor);
        }
        shEventClone = safetyhook::create_inline(pattern.get_first(0), EventClone);
        shEventConstructor = safetyhook::create_inline(eventConstructor, EventConstructor);

        pattern = find_pattern("56 8B 74 24 08 57 8B 06 8B F9 8B CE FF 50 04 83 F8 09", "56 8B 74 24 08 8B 06 8B 50 04 57 8B F9 8B CE FF D2 83 F8 09");
        shWaterPriority = safetyhook::create_inline(pattern.get_first(0), WaterPriority);

        // Our ragdoll keeps a two-handed weapon in the right hand only: attaching the other hand makes the
        // weapon's collision fight the ragdoll. The skip goes to the function's own exit for this case.
        pattern = hook::pattern("0F 2F C2 0F 86 ? ? ? ? 81 8F 70 02 00 00 00 00 04 00");
        if (!pattern.empty()) // 1.2.0.32, 1.2.0.43, 1.2.0.59
            supportingHandExit = resolve_displacement(pattern.get_first(3)).value();
        else // 1.0.7.0, 1.0.8.0, 1.1.2.0, 1.1.3.0
        {
            pattern = hook::pattern("0F 86 ? ? ? ? 8B 8F C4 07 00 00 D9 EE 81 8F 70 02 00 00 00 00 04 00");
            supportingHandExit = resolve_displacement(pattern.get_first(0)).value();
        }
        shSupportingHand = safetyhook::create_mid(pattern.get_first(0), [](SafetyHookContext& regs)
        {
            if (postingPed && regs.edi == postingPed)
                regs.eip = supportingHandExit;
        });
    }
}

class PlayerJumpRagdollControl
{
public:
    PlayerJumpRagdollControl()
    {
        FusionFix::onInitEvent() += []()
        {
            AirborneRagdoll::InstallHooks();
            FusionFix::onGameProcessEvent() += AirborneRagdoll::Process;
        };
    }
} PlayerJumpRagdollControl;
