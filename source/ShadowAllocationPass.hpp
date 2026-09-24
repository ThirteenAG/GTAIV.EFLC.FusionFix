#pragma once

#include "PlayerShadowBudget.hpp"
#include <cstring>

// Offline-testable transaction around the seven-slot policy. This owns copied
// candidate records, never game pointers across passes. A failed check leaves
// every original output byte unchanged and discards provisional policy history.
namespace fusionfix::shadows::budget
{
    class ShadowAllocationPass
    {
    public:
        static constexpr std::size_t LightBytes = 0x80;
        static constexpr std::size_t Capacity = PlayerShadowBudget::InputCapacity;

        void Begin(Frame frame, const void* list, std::uint32_t count) noexcept
        {
            active_ = true;
            const auto address = reinterpret_cast<std::uintptr_t>(list);
            failed_ = !list || count > 4096 ||
                (count <= 4096 && address > UINTPTR_MAX - static_cast<std::uintptr_t>(count) * LightBytes);
            count_ = 0;
            list_ = list;
            listCount_ = count;
            policy_.BeginPass(frame);
        }

        void Cancel() noexcept
        {
            active_ = false;
            failed_ = true;
            policy_.Reset();
        }

        // Native quality/update-divisor gates can return without reaching the
        // output boundary. Retain history, but never reuse this pass's inputs.
        void EndInvocation() noexcept
        {
            if (failed_) policy_.Reset();
            active_ = false;
        }

        // The adapter calls this ONLY at the native engine-eligible boundary.
        // Eligibility/volume policy can reject a light without invalidating the
        // pass, but ownership collisions, overflow, and invalid copies cannot.
        void Observe(Candidate candidate, const void* currentLight) noexcept
        {
            if (!active_ || failed_) return;
            if (!currentLight || candidate.index >= listCount_ || !candidate.key || !candidate.engineEligible ||
                !std::isfinite(candidate.distanceSquared) || candidate.distanceSquared < 0 ||
                count_ == Capacity ||
                currentLight != static_cast<const std::uint8_t*>(list_) + candidate.index * LightBytes)
            {
                failed_ = true;
                return;
            }
            for (std::size_t i = 0; i < count_; ++i)
                if (records_[i].candidate.index == candidate.index ||
                    records_[i].candidate.key == candidate.key)
                {
                    failed_ = true;
                    return;
                }
            auto& record = records_[count_++];
            record.candidate = candidate;
            std::memcpy(record.snapshot.data(), currentLight, LightBytes);
            policy_.Add(candidate);
        }

        // This is the sole engine write: exactly seven int32 indices, after all
        // input records and the native output layout have passed validation.
        bool Commit(const void* list, std::uint32_t count, std::int32_t* sevenIndices) noexcept
        {
            if (!active_ || failed_ || !sevenIndices || list != list_ || count != listCount_)
                return Abort();
            for (std::size_t i = 0; i < count_; ++i)
                if (std::memcmp(records_[i].snapshot.data(),
                        static_cast<const std::uint8_t*>(list) + records_[i].candidate.index * LightBytes,
                        LightBytes) != 0)
                    return Abort();
            for (std::size_t i = 0; i < PlayerShadowBudget::SlotCount; ++i)
            {
                const auto index = sevenIndices[i];
                if (index < -1 || (index >= 0 && !Find(static_cast<std::uint32_t>(index))))
                    return Abort();
                for (std::size_t j = 0; index >= 0 && j < i; ++j)
                    if (sevenIndices[j] == index) return Abort();
            }

            const auto& selected = policy_.Finalize();
            if (!selected.safeToApply || selected.inputOverflow || selected.ambiguousRecords || (selected.validMask & 0x80))
                return Abort();
            std::array<std::int32_t, PlayerShadowBudget::SlotCount> output{};
            output.fill(-1);
            std::size_t outputCount = 0;
            for (std::size_t i = 0; i < output.size(); ++i)
            {
                if (!(selected.validMask & (1u << i))) continue;
                const auto& slot = selected.slots[i];
                const auto* current = Find(slot.index);
                if (!current || slot.index >= count || slot.key != current->candidate.key ||
                    slot.generation != current->candidate.generation || slot.kind != current->candidate.kind)
                    return Abort();
                for (std::size_t j = 0; j < i; ++j)
                    if (output[j] == static_cast<std::int32_t>(slot.index)) return Abort();
                output[i] = static_cast<std::int32_t>(slot.index);
                ++outputCount;
            }
            if (outputCount != selected.count) return Abort();
            std::memcpy(sevenIndices, output.data(), sizeof(output));
            active_ = false;
            return true;
        }

        bool Active() const noexcept { return active_ && !failed_; }

    private:
        struct Record
        {
            Candidate candidate{};
            std::array<std::uint8_t, LightBytes> snapshot{};
        };
        const Record* Find(std::uint32_t index) const noexcept
        {
            for (std::size_t i = 0; i < count_; ++i)
                if (records_[i].candidate.index == index) return &records_[i];
            return nullptr;
        }
        bool Abort() noexcept { Cancel(); return false; }
        PlayerShadowBudget policy_{};
        std::array<Record, Capacity> records_{};
        std::size_t count_{};
        const void* list_{};
        std::uint32_t listCount_{};
        bool active_{};
        bool failed_{};
    };
}
