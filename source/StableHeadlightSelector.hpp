#pragma once

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>

// Engine-independent policy. BeginFrame MUST use a render-frame serial, not a
// millisecond clock: multiple render frames may have the same timestamp.
namespace fusionfix::shadows
{
    struct Vec3 { float x{}, y{}, z{}; };

    struct Geometry
    {
        float distanceSquared{};
        bool directionKnown{};
        bool aimedAtPlayer{};
    };

    // Only supply forward when the engine adapter has verified that the vector
    // is the light's world-space forward direction. No ABI assumptions here.
    inline Geometry EvaluateGeometry(Vec3 player, Vec3 light, const Vec3* forward = nullptr) noexcept
    {
        const Vec3 delta{ player.x - light.x, player.y - light.y, player.z - light.z };
        Geometry result{ delta.x * delta.x + delta.y * delta.y + delta.z * delta.z };
        if (!forward || !std::isfinite(result.distanceSquared))
            return result;
        const double lengthSquared = static_cast<double>(forward->x) * forward->x +
            static_cast<double>(forward->y) * forward->y + static_cast<double>(forward->z) * forward->z;
        if (!std::isfinite(lengthSquared) || lengthSquared < 0.0001f)
            return result;
        const double dot = static_cast<double>(delta.x) * forward->x +
            static_cast<double>(delta.y) * forward->y + static_cast<double>(delta.z) * forward->z;
        result.directionKnown = true;
        // 45 degree cone; the adapter may use a narrower verified light cone.
        result.aimedAtPlayer = result.distanceSquared < 0.0001f ||
            (dot > 0.0f && dot * dot >= 0.5f * result.distanceSquared * lengthSquared);
        return result;
    }

    struct HeadlightCandidate
    {
        std::uintptr_t identity{};
        Geometry geometry{};
        bool playerHeadlight{};
    };

    struct FrameContext
    {
        std::uint64_t frame{};
        std::uint32_t timeMs{};
        std::uintptr_t session{}; // Player/ped identity; change on game load.
        bool driving{};
        std::uintptr_t occupiedVehicle{};
    };

    class StableHeadlightSelector
    {
    public:
        static constexpr std::size_t SlotCount = 2;
        static constexpr std::size_t CandidateCapacity = 16;

        struct Policy
        {
            float maximumDistanceSquared = 35.0f * 35.0f;
            float replacementRatio = 0.70f;
            float replacementMarginSquared = 4.0f;
            std::uint32_t minimumHoldMs = 500;
            std::uint32_t staleGraceMs = 100;
            std::uint32_t staleGraceFrames = 2;
            std::uint32_t resetGapMs = 2000;
        };

        StableHeadlightSelector() = default;
        explicit StableHeadlightSelector(Policy policy) noexcept : policy_(policy) {}

        void Reset() noexcept
        {
            initialized_ = false;
            suppressed_ = false;
            pendingReset_ = false;
            candidateCount_ = 0;
            active_ = {};
        }

        void BeginFrame(FrameContext next) noexcept
        {
            if (!initialized_)
            {
                StartEmpty(next);
                return;
            }
            if (next.frame == frame_.frame)
            {
                // A caller changing gameplay/session state halfway through a
                // render frame cannot allocate a second pair in that frame.
                if (next.session != frame_.session || next.driving != frame_.driving ||
                    next.occupiedVehicle != frame_.occupiedVehicle)
                {
                    suppressed_ = true;
                    pendingReset_ = true;
                }
                return;
            }

            const std::uint32_t elapsed = next.timeMs - frame_.timeMs;
            const bool newDrivingVehicle = next.driving &&
                (!frame_.driving || next.occupiedVehicle != frame_.occupiedVehicle);
            if (pendingReset_ || next.session != frame_.session || next.frame < frame_.frame ||
                elapsed > policy_.resetGapMs || newDrivingVehicle)
            {
                // Do not reuse last frame's player flag after entering another
                // vehicle. Collect its beam requests for selection next frame.
                StartEmpty(next);
                return;
            }

            for (auto& slot : active_)
            {
                if (!slot.candidate.identity)
                    continue;
                if (slot.observedAny)
                {
                    if (!slot.observedEligible || !Eligible(slot.observation, next))
                    {
                        slot = {};
                        continue;
                    }
                    slot.candidate = slot.observation;
                    slot.lastSeenMs = frame_.timeMs;
                    slot.missingFrames = 0;
                }
                else
                {
                    ++slot.missingFrames;
                    if (slot.missingFrames > policy_.staleGraceFrames ||
                        next.timeMs - slot.lastSeenMs > policy_.staleGraceMs ||
                        !Eligible(slot.candidate, next))
                        slot = {};
                }
            }

            // Re-sort for changed context (particularly driving -> on foot).
            for (std::size_t i = 1; i < candidateCount_; ++i)
                for (std::size_t j = i; j > 0 && Better(candidates_[j], candidates_[j - 1], next); --j)
                {
                    const auto temporary = candidates_[j];
                    candidates_[j] = candidates_[j - 1];
                    candidates_[j - 1] = temporary;
                }

            for (std::size_t c = 0; c < candidateCount_; ++c)
            {
                const auto& candidate = candidates_[c];
                if (!Eligible(candidate, next) || Contains(candidate.identity))
                    continue;
                std::size_t replace = SlotCount;
                for (std::size_t i = 0; i < SlotCount; ++i)
                {
                    if (!active_[i].candidate.identity)
                    {
                        replace = i;
                        break;
                    }
                    if (CanReplace(candidate, active_[i], next) &&
                        (replace == SlotCount || Better(active_[replace].candidate, active_[i].candidate, next)))
                        replace = i;
                }
                if (replace != SlotCount)
                {
                    active_[replace] = {};
                    active_[replace].candidate = candidate;
                    active_[replace].sinceMs = next.timeMs;
                    active_[replace].lastSeenMs = frame_.timeMs;
                }
            }

            frame_ = next;
            ClearObservations();
        }

        // Collect this frame for NEXT frame. The chosen identities never change
        // here. Current eligibility is checked again, even during stale grace.
        bool Consider(HeadlightCandidate candidate) noexcept
        {
            if (!initialized_ || suppressed_ || !candidate.identity)
                return false;
            const bool eligible = Eligible(candidate, frame_);
            bool selected = false;
            for (auto& slot : active_)
            {
                if (slot.candidate.identity != candidate.identity)
                    continue;
                selected = true;
                slot.observedAny = true;
                if (eligible && (!slot.observedEligible || Better(candidate, slot.observation, frame_)))
                {
                    slot.observation = candidate;
                    slot.observedEligible = true;
                }
            }
            if (eligible)
                Collect(candidate);
            return selected && eligible;
        }

        std::array<std::uintptr_t, SlotCount> ActiveIdentities() const noexcept
        {
            return { active_[0].candidate.identity, active_[1].candidate.identity };
        }

    private:
        struct Slot
        {
            HeadlightCandidate candidate{};
            HeadlightCandidate observation{};
            std::uint32_t sinceMs{};
            std::uint32_t lastSeenMs{};
            std::uint32_t missingFrames{};
            bool observedAny{};
            bool observedEligible{};
        };

        bool Eligible(const HeadlightCandidate& c, const FrameContext& frame) const noexcept
        {
            return c.identity && std::isfinite(c.geometry.distanceSquared) &&
                c.geometry.distanceSquared >= 0.0f && c.geometry.distanceSquared <= policy_.maximumDistanceSquared &&
                (!frame.driving || c.playerHeadlight);
        }

        static unsigned Priority(const HeadlightCandidate& c, const FrameContext& frame) noexcept
        {
            if (frame.driving || (c.geometry.directionKnown && c.geometry.aimedAtPlayer))
                return 0;
            return c.geometry.directionKnown ? 2 : 1;
        }

        static bool Better(const HeadlightCandidate& a, const HeadlightCandidate& b, const FrameContext& frame) noexcept
        {
            const auto aPriority = Priority(a, frame), bPriority = Priority(b, frame);
            if (aPriority != bPriority) return aPriority < bPriority;
            if (a.geometry.distanceSquared != b.geometry.distanceSquared)
                return a.geometry.distanceSquared < b.geometry.distanceSquared;
            if (a.identity != b.identity) return a.identity < b.identity;
            // Deterministic duplicate reduction, including contradictory tags.
            if (a.playerHeadlight != b.playerHeadlight) return a.playerHeadlight;
            if (a.geometry.directionKnown != b.geometry.directionKnown) return a.geometry.directionKnown;
            return a.geometry.aimedAtPlayer && !b.geometry.aimedAtPlayer;
        }

        bool Contains(std::uintptr_t id) const noexcept
        {
            for (const auto& slot : active_) if (slot.candidate.identity == id) return true;
            return false;
        }

        bool CanReplace(const HeadlightCandidate& candidate, const Slot& slot, const FrameContext& next) const noexcept
        {
            if (next.timeMs - slot.sinceMs < policy_.minimumHoldMs)
                return false;
            const auto incomingPriority = Priority(candidate, next), oldPriority = Priority(slot.candidate, next);
            if (incomingPriority != oldPriority) return incomingPriority < oldPriority;
            return candidate.geometry.distanceSquared + policy_.replacementMarginSquared <
                slot.candidate.geometry.distanceSquared * policy_.replacementRatio;
        }

        void Collect(HeadlightCandidate candidate) noexcept
        {
            for (std::size_t i = 0; i < candidateCount_; ++i)
            {
                if (candidates_[i].identity != candidate.identity) continue;
                if (!Better(candidate, candidates_[i], frame_)) return;
                candidates_[i] = candidate;
                SortCandidates();
                return;
            }
            if (candidateCount_ < CandidateCapacity)
                candidates_[candidateCount_++] = candidate;
            else if (Better(candidate, candidates_[candidateCount_ - 1], frame_))
                candidates_[candidateCount_ - 1] = candidate;
            else
                return;
            SortCandidates();
        }

        void SortCandidates() noexcept
        {
            for (std::size_t i = 1; i < candidateCount_; ++i)
                for (std::size_t j = i; j > 0 && Better(candidates_[j], candidates_[j - 1], frame_); --j)
                {
                    const auto temporary = candidates_[j];
                    candidates_[j] = candidates_[j - 1];
                    candidates_[j - 1] = temporary;
                }
        }

        void ClearObservations() noexcept
        {
            candidateCount_ = 0;
            suppressed_ = false;
            for (auto& slot : active_)
            {
                slot.observedAny = false;
                slot.observedEligible = false;
                slot.observation = {};
            }
        }

        void StartEmpty(FrameContext next) noexcept
        {
            Reset();
            initialized_ = true;
            frame_ = next;
            ClearObservations();
        }

        Policy policy_{};
        FrameContext frame_{};
        std::array<Slot, SlotCount> active_{};
        std::array<HeadlightCandidate, CandidateCapacity> candidates_{};
        std::size_t candidateCount_{};
        bool initialized_{};
        bool suppressed_{};
        bool pendingReset_{};
    };
}
