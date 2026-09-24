#pragma once

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>

// Policy prototype only. This does not hook the game or change its static-light
// data. Its adapter must supply the CURRENT pass's engine-eligible candidates.
namespace fusionfix::shadows::budget
{
    enum class Kind : std::uint8_t { Lamp, PlayerBeam, OtherBeam };

    struct Candidate
    {
        std::uint64_t key{}; // Opaque persistent identity; zero is rejected.
        std::uint32_t index{}; // Index into this pass's engine light list.
        Kind kind{};
        float distanceSquared{};
        bool influencesPlayer{}; // Verified light volume/beam contains player.
        bool engineEligible{};
        // Optional engine generation or stable geometry signature detects reuse
        // of a pointer/key. A changing list index is NOT a generation.
        std::uint64_t generation{};
    };

    struct Frame
    {
        std::uint64_t serial{};
        std::uint32_t timeMs{};
        std::uintptr_t session{};
        bool driving{};
    };

    class PlayerShadowBudget
    {
    public:
        static constexpr std::size_t SlotCount = 7;
        static constexpr std::size_t InputCapacity = 512;
        static constexpr std::uint32_t InvalidIndex = (std::numeric_limits<std::uint32_t>::max)();

        struct Policy
        {
            float maximumLampDistanceSquared = 60.0f * 60.0f;
            float maximumBeamDistanceSquared = 35.0f * 35.0f;
            float replacementRatio = 0.70f;
            float replacementMarginSquared = 4.0f;
            std::uint32_t minimumHoldMs = 500;
            std::uint32_t resetGapMs = 2000;
        };

        struct Slot
        {
            std::uint64_t key{};
            std::uint64_t generation{};
            std::uint32_t index = InvalidIndex;
            Kind kind{};
        };

        struct Selection
        {
            std::array<Slot, SlotCount> slots{};
            std::uint8_t validMask{};
            std::uint8_t count{};
            bool inputOverflow{};
            std::uint16_t ambiguousRecords{};
            bool unsupportedIdentity{};
            bool invalidInput{};
            bool safeToApply{}; // False: adapter must retain original pass output.
        };

        PlayerShadowBudget() = default;
        explicit PlayerShadowBudget(Policy policy) noexcept : policy_(policy) {}

        void Reset() noexcept
        {
            history_ = {};
            initialized_ = false;
            begun_ = false;
            finalized_ = false;
            count_ = 0;
            selectedCount_ = 0;
            overflow_ = false;
            selection_ = {};
        }

        // One instance per engine dynamic-shadow pass. BeginPass may be called
        // again for the same simulation frame, but each result uses fresh input.
        void BeginPass(Frame frame) noexcept
        {
            if (!initialized_ || frame.session != frame_.session || frame.serial < frame_.serial ||
                frame.timeMs - frame_.timeMs > policy_.resetGapMs)
                history_ = {};
            initialized_ = true;
            begun_ = true;
            finalized_ = false;
            frame_ = frame;
            count_ = 0;
            overflow_ = false;
            selection_ = {};
        }

        // No allocations. Exact duplicates merge deterministically. Conflicting
        // key/index ownership rejects every involved record at Finalize.
        bool Add(Candidate candidate) noexcept
        {
            if (!begun_ || finalized_ || !candidate.engineEligible) return false;
            if (!candidate.key)
            {
                selection_.unsupportedIdentity = true;
                return false;
            }
            if (candidate.index == InvalidIndex || !std::isfinite(candidate.distanceSquared) ||
                candidate.distanceSquared < 0.0f ||
                (candidate.kind != Kind::Lamp && candidate.kind != Kind::PlayerBeam && candidate.kind != Kind::OtherBeam))
            {
                selection_.invalidInput = true;
                return false;
            }
            if (!Eligible(candidate)) return false;
            bool ambiguous = false;
            std::size_t same = InputCapacity;
            for (std::size_t i = 0; i < count_; ++i)
            {
                auto& record = input_[i];
                const bool identical = SameIdentity(record.candidate, candidate) &&
                    record.candidate.index == candidate.index;
                if (identical)
                {
                    same = i;
                    continue;
                }
                if (record.candidate.key == candidate.key || record.candidate.index == candidate.index)
                {
                    record.ambiguous = true;
                    ambiguous = true;
                }
            }
            if (same != InputCapacity)
            {
                auto& record = input_[same];
                record.ambiguous |= ambiguous;
                if (candidate.distanceSquared < record.candidate.distanceSquared)
                    record.candidate.distanceSquared = candidate.distanceSquared;
                record.candidate.influencesPlayer |= candidate.influencesPlayer;
                return !record.ambiguous;
            }
            if (count_ == InputCapacity)
            {
                overflow_ = true;
                return false;
            }
            input_[count_++] = {candidate, ambiguous};
            return !ambiguous;
        }

        const Selection& Finalize() noexcept
        {
            if (finalized_) return selection_;
            finalized_ = true;
            if (!begun_) return selection_;
            selection_.inputOverflow = overflow_;
            for (std::size_t i = 0; i < count_; ++i)
                if (input_[i].ambiguous) ++selection_.ambiguousRecords;
            selection_.safeToApply = !overflow_ && !selection_.ambiguousRecords &&
                !selection_.unsupportedIdentity && !selection_.invalidInput;
            if (overflow_)
            {
                // Avoid source-order-dependent truncation. Adapter must detect
                // this result and keep its original engine output for this pass.
                history_ = {};
                return selection_;
            }

            selectedCount_ = 0;
            // Two local lamps protect the occupied car's exterior-light shadow.
            Pick(2, [](const Candidate& c) { return c.kind == Kind::Lamp && c.influencesPlayer; });
            const auto beforePlayerBeams = selectedCount_;
            Pick(2, [this](const Candidate& c) {
                return c.kind == Kind::PlayerBeam && (frame_.driving || c.influencesPlayer);
            });
            const auto playerBeams = selectedCount_ - beforePlayerBeams;
            if (!frame_.driving)
                Pick(2 - playerBeams, [](const Candidate& c) {
                    return c.kind == Kind::OtherBeam && c.influencesPlayer;
                });
            Pick(SlotCount, [](const Candidate& c) { return c.kind == Kind::Lamp; });

            std::array<bool, SlotCount> assigned{};
            std::array<History, SlotCount> nextHistory{};
            // Keep retained identities in their old atlas positions while
            // replacing each stored light index from CURRENT pass input.
            for (std::size_t slot = 0; slot < SlotCount; ++slot)
            {
                if (!history_[slot].slot.key) continue;
                for (std::size_t chosen = 0; chosen < selectedCount_; ++chosen)
                {
                    const auto& c = input_[selected_[chosen]].candidate;
                    if (assigned[chosen] || !SameIdentity(history_[slot].slot, c)) continue;
                    Assign(slot, c, history_[slot].sinceMs, nextHistory);
                    assigned[chosen] = true;
                    break;
                }
            }
            for (std::size_t chosen = 0; chosen < selectedCount_; ++chosen)
            {
                if (assigned[chosen]) continue;
                for (std::size_t slot = 0; slot < SlotCount; ++slot)
                    if (!nextHistory[slot].slot.key)
                    {
                        Assign(slot, input_[selected_[chosen]].candidate, frame_.timeMs, nextHistory);
                        break;
                    }
            }
            // A failed pass was not installed by the bridge, so it must not
            // acquire fictitious persistent slot ownership for the next pass.
            history_ = selection_.safeToApply ? nextHistory : std::array<History, SlotCount>{};
            return selection_;
        }

    private:
        struct Record { Candidate candidate{}; bool ambiguous{}; };
        struct History { Slot slot{}; std::uint32_t sinceMs{}; };

        bool Eligible(const Candidate& c) const noexcept
        {
            if (!c.key || !c.engineEligible || c.index == InvalidIndex ||
                !std::isfinite(c.distanceSquared) || c.distanceSquared < 0.0f)
                return false;
            if (c.kind != Kind::Lamp && c.kind != Kind::PlayerBeam && c.kind != Kind::OtherBeam)
                return false;
            const auto maximum = c.kind == Kind::Lamp ? policy_.maximumLampDistanceSquared : policy_.maximumBeamDistanceSquared;
            return c.distanceSquared <= maximum;
        }

        template <typename A> static bool SameIdentity(const A& a, const Candidate& b) noexcept
        {
            return a.key == b.key && a.generation == b.generation && a.kind == b.kind;
        }

        const History* Retained(const Candidate& candidate) const noexcept
        {
            for (const auto& previous : history_)
                if (previous.slot.key && SameIdentity(previous.slot, candidate)) return &previous;
            return nullptr;
        }

        bool Better(const Candidate& a, const Candidate& b) const noexcept
        {
            const auto* oldA = Retained(a);
            const auto* oldB = Retained(b);
            const bool heldA = oldA && frame_.timeMs - oldA->sinceMs < policy_.minimumHoldMs;
            const bool heldB = oldB && frame_.timeMs - oldB->sinceMs < policy_.minimumHoldMs;
            if (heldA != heldB) return heldA;
            // Comparing adjusted distance gives a total order, avoiding
            // non-transitive pairwise hysteresis under shuffled submission.
            const double distanceA = oldA ? a.distanceSquared :
                (static_cast<double>(a.distanceSquared) + policy_.replacementMarginSquared) / policy_.replacementRatio;
            const double distanceB = oldB ? b.distanceSquared :
                (static_cast<double>(b.distanceSquared) + policy_.replacementMarginSquared) / policy_.replacementRatio;
            if (distanceA != distanceB) return distanceA < distanceB;
            if (a.key != b.key) return a.key < b.key;
            if (a.generation != b.generation) return a.generation < b.generation;
            return a.index < b.index;
        }

        bool AlreadySelected(std::size_t inputIndex) const noexcept
        {
            for (std::size_t i = 0; i < selectedCount_; ++i)
                if (selected_[i] == inputIndex) return true;
            return false;
        }

        template <typename Predicate> void Pick(std::size_t maximum, Predicate predicate) noexcept
        {
            for (std::size_t n = 0; n < maximum && selectedCount_ < SlotCount; ++n)
            {
                std::size_t best = InputCapacity;
                for (std::size_t i = 0; i < count_; ++i)
                {
                    if (input_[i].ambiguous || AlreadySelected(i) || !predicate(input_[i].candidate)) continue;
                    if (best == InputCapacity || Better(input_[i].candidate, input_[best].candidate)) best = i;
                }
                if (best == InputCapacity) break;
                selected_[selectedCount_++] = best;
            }
        }

        void Assign(std::size_t slot, const Candidate& candidate, std::uint32_t since,
                    std::array<History, SlotCount>& nextHistory) noexcept
        {
            const Slot value{candidate.key, candidate.generation, candidate.index, candidate.kind};
            selection_.slots[slot] = value;
            selection_.validMask |= static_cast<std::uint8_t>(1u << slot);
            ++selection_.count;
            nextHistory[slot] = {value, since};
        }

        Policy policy_{};
        Frame frame_{};
        std::array<Record, InputCapacity> input_{};
        std::array<History, SlotCount> history_{};
        std::array<std::size_t, SlotCount> selected_{};
        Selection selection_{};
        std::size_t count_{};
        std::size_t selectedCount_{};
        bool initialized_{};
        bool begun_{};
        bool finalized_{};
        bool overflow_{};
    };
}
