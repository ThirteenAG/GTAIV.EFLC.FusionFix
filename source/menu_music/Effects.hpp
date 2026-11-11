#pragma once

#include "Mixer.hpp"

#include <atomic>
#include <algorithm>
#include <cmath>

namespace MenuMusic
{
    struct audEffect
    {
        void** vtable;
        const std::uint8_t* metadata;
        audEffect* next;

        bool Is(std::uint8_t type) const { return metadata && *metadata == type; }
        void* GetEffectSettings()
        {
            return reinterpret_cast<void*(__thiscall*)(audEffect*)>(vtable[4])(this);
        }
    };

    struct BiquadSettings
    {
        float cutoff, gain, bandwidth, reserved;
        int type;
        bool bypass;
        std::array<std::uint8_t, 3> padding;
    };

    struct DelaySettings
    {
        std::array<int, 6> time;
        std::array<float, 6> wet;
        std::array<float, 6> feedback;
    };

    static_assert(sizeof(BiquadSettings) == 24);
    static_assert(sizeof(DelaySettings) == 72);

    struct PhysicalDelay
    {
        void** vtable;
        unsigned int position;
        std::array<unsigned int, 6> time;
        std::array<float, 6> wet, feedback;
        float* buffer;
        unsigned int capacity;
    };

    struct audDelayEffect
    {
        audEffect effect;
        std::array<std::byte, 32> reserved12;
        unsigned int readIndex;
        std::array<std::byte, 68> reserved48;
        PhysicalDelay* physical;
        std::array<DelaySettings, 3> settings;
    };

    static_assert(offsetof(audDelayEffect, physical) == 116);
    static_assert(offsetof(audDelayEffect, settings) == 120);
    static_assert(offsetof(PhysicalDelay, capacity) == 84);

    inline bool ApplyDelayTimes(const DelaySettings& settings, PhysicalDelay& physical,
        unsigned int sampleRate, unsigned int frameSamples)
    {
        if (sampleRate < 8000 || sampleRate > 192000 || !frameSamples || frameSamples > 8192)
            return false;
        std::array<unsigned int, 6> samples;
        const auto quantum = static_cast<std::uint64_t>(frameSamples) * 1000;
        for (std::size_t i = 0; i < samples.size(); ++i)
        {
            if (settings.time[i] < 0)
                return false;
            const auto count = (static_cast<std::uint64_t>(settings.time[i]) * sampleRate + quantum - 1) / quantum * frameSamples;
            if (count >= physical.capacity)
                return false;
            samples[i] = static_cast<unsigned int>(count);
        }
        physical.time = samples;
        return true;
    }

    inline bool ApplySourceReverbSettings(const Profile& profile, unsigned int route, audEffect* chain, bool resetDelay = false)
    {
        if (route > 2 || !chain || !chain->Is(3))
            return false;
        auto* highPass = chain->next;
        if (!highPass || !highPass->Is(3))
            return false;
        auto* delay = highPass->next;
        if (route == 2 && (!delay || !delay->Is(7)))
            return false;

        auto* low = static_cast<BiquadSettings*>(chain->GetEffectSettings());
        auto* high = static_cast<BiquadSettings*>(highPass->GetEffectSettings());
        auto* echo = route == 2 ? static_cast<DelaySettings*>(delay->GetEffectSettings()) : nullptr;
        if (!low || !high || (route == 2 && !echo))
            return false;

        low->cutoff = 20000.0f;
        low->bypass = false;
        if (route == 2)
        {
            high->cutoff = profile.largeReverbHighPassHz;
            high->bypass = false;
            echo->feedback.fill(profile.largeDelayFeedbackDb);
            if (resetDelay)
                echo->time.fill(60);
        }
        return true;
    }

    class Effects
    {
    public:
        void Set(const Profile& value)
        {
            profile.store(&value);
            owner.store(this);
        }

        void Release()
        {
            auto* expected = this;
            if (owner.compare_exchange_strong(expected, nullptr))
                delayReset.store(true);
            profile.store(nullptr);
            delay.store(nullptr);
        }

        static void BeforeCommit(unsigned int route, audEffect* chain)
        {
            auto* current = owner.load();
            const auto* value = current ? current->profile.load() : nullptr;
            if (value && ApplySourceReverbSettings(*value, route, chain, delayReset.load()) && route == 2)
                current->delay.store(chain->next->next);
        }

        static void AfterDelayUpdate(audDelayEffect* effect, unsigned int sampleRate, unsigned int frameSamples)
        {
            auto* current = owner.load();
            if (!current || !current->profile.load() || !effect ||
                current->delay.load() != &effect->effect || effect->readIndex >= effect->settings.size() || !effect->physical)
                return;
            // PC divides its block size by 1000 instead of converting milliseconds with the sample rate.
            ApplyDelayTimes(effect->settings[effect->readIndex], *effect->physical, sampleRate, frameSamples);
        }

    private:
        // Native setup rewrites these fields before every commit, including release.
        inline static std::atomic<Effects*> owner = nullptr;
        // Xenon keeps the 60 ms stop reset for subsequent menu sessions.
        inline static std::atomic<bool> delayReset = false;
        std::atomic<const Profile*> profile = nullptr;
        std::atomic<audEffect*> delay = nullptr;
    };
    inline float VoiceAttenuation(int headphones, int voiceVolume, int throughSpeakers, bool talking, bool aboutToTalk)
    {
        return headphones == 0 && voiceVolume > 0 && throughSpeakers != 0 && (talking || aboutToTalk) ? -18.0f : 0.0f;
    }

    class CategoryVolume
    {
    public:
        explicit CategoryVolume(float& requested) : requested(requested) {}

        void Update(float attenuation)
        {
            if (!std::isfinite(requested) || !std::isfinite(attenuation))
                return;
            if (!owned || requested != applied)
                baseline = requested;
            applied = std::max(-100.0f, baseline + std::min(attenuation, 0.0f));
            requested = applied;
            owned = true;
        }

        void Release()
        {
            if (owned && requested == applied)
                requested = baseline;
            owned = false;
        }

    private:
        float& requested;
        float baseline = 0.0f;
        float applied = 0.0f;
        bool owned = false;
    };
}
