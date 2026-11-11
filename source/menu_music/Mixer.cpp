#include "Mixer.hpp"

#include <algorithm>
#include <cmath>

namespace MenuMusic
{
    static constexpr Profile OriginalProfile{
        {1.4f, 1.0f, 1.0f, 1.5f, 1.8f, 1.8f, 2.0f, 2.0f, 2.2f, 2.2f, 2.1f, 2.4f, 1.1f},
        {0.5f, 0.8f, 0.7f, 0.6f, 0.9f, 0.8f, 1.4f, 1.3f, 1.4f, 1.1f, 1.2f, 1.65f, 0.8f},
        {0.0f, 0.0f, 0.0f, 0.0f, 0.3f, 0.3f, 0.4f, 0.4f, 1.0f, 1.0f, 1.25f, 1.25f, 2.0f},
        1.0f, 0.4f, 250.0f, -6.0f
    };
    static constexpr Profile BalladProfile{
        {1.3f, 1.0f, 1.2f, 1.6f, 1.6f, 1.7f, 1.8f, 1.8f, 1.9f, 1.9f, 2.0f, 2.0f, 2.0f},
        {0.5f, 0.8f, 0.7f, 0.6f, 0.9f, 0.8f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.8f},
        {0.0f, 0.0f, 0.0f, 0.0f, 0.3f, 0.3f, 0.4f, 0.4f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f},
        0.75f, 0.6f, 200.0f, -9.0f
    };

    const Profile& GetProfile(Episode episode)
    {
        return episode == Episode::TBoGT ? BalladProfile : OriginalProfile;
    }

    bool SuppressEnvironmentReverb(Episode episode, std::size_t voice)
    {
        return IsShadow(voice) && (episode != Episode::TBoGT || voice == 1 || voice == 3);
    }

    const char* AccentBank(Episode episode)
    {
        if (episode == Episode::TLAD)
            return nullptr;
        return episode == Episode::TBoGT ? "EP2_SFX\\EP2_MENUONESHOTS" : "GPS\\MENU_MUSIC_ONESHOTS";
    }

    int AccentPredelayMs(Episode episode, int parentPlayTimeMs)
    {
        // EFLC waits a full period at exact boundaries.
        const int period = episode == Episode::TBoGT ? 289 : 822;
        const int remainder = parentPlayTimeMs % period;
        return episode == Episode::IV ? remainder : std::max(0, period - remainder);
    }

    float EnvelopeGain(float interpolation)
    {
        // FEMUSIC_INTERP_TO_VOL, retained in the PC curves.dat12.
        if (!std::isfinite(interpolation) || interpolation <= 0.0f || interpolation >= 2.0f)
            return 0.0f;
        if (interpolation <= 1.0f)
            return interpolation;
        if (interpolation <= 1.5f)
            return 1.0f - (interpolation - 1.0f) * 1.3f;
        return 0.35f - (interpolation - 1.5f) * 0.7f;
    }

    float GainToDb(float gain)
    {
        if (!std::isfinite(gain) || gain < 0.00001f)
            return SilenceDb;
        // Preserve the console's log2 approximation.
        return (std::log2(gain) * 0.301f) * 20.0f;
    }

    void Mixer::Start(Episode newEpisode, Random& random)
    {
        Stop();
        episode = newEpisode;
        running = true;
        pendingInput = true;
        percussionWait = random.Range(25.0f, 45.0f);
        dulcimerWait = random.Range(75.0f, 35.0f);
    }

    void Mixer::Stop()
    {
        running = false;
        pendingInput = false;
        channels = {};
        intensity = 0.0f;
        dulcimerElapsed = dulcimerWait = percussionElapsed = percussionWait = 0.0f;
        dulcimerCounter = 0;
        // Keep the intensity target and final-child cutoff across menu openings.
    }

    void Mixer::NotifyInput(bool lobby, Random& random)
    {
        if (!running || !pendingInput)
            return;
        static constexpr std::array<std::array<float, 2>, 8> factors{{
            {0.8f, 1.0f}, {0.75f, 0.97f}, {0.6f, 0.8f}, {0.7f, 0.8f},
            {0.5f, 0.75f}, {0.4f, 0.7f}, {0.3f, 0.6f}, {0.2f, 0.5f}
        }};
        const float scale = lobby ? 1.5f : 1.0f;
        for (std::size_t i = 0; i < factors.size(); ++i)
        {
            auto& channel = channels[i];
            if (channel.phase == ChannelPhase::Waiting)
                channel.waitSeconds -= random.Range(
                    (factors[i][0] * scale) * channel.waitSeconds,
                    (factors[i][1] * scale) * channel.waitSeconds);
        }
        pendingInput = false;
    }

    void Mixer::Update(float elapsedSeconds, int parentPlayTimeMs, bool lobby, Random& random, Output& output)
    {
        if (!running || !std::isfinite(elapsedSeconds))
            return;
        // Console caps one update at 100 ms, including menus and long stalls.
        const float dt = std::clamp(elapsedSeconds, 0.0f, 0.1f);
        const auto& profile = GetProfile(episode);
        output.SetEffects(profile);

        intensity += (intensityTarget <= intensity ? -1.0f : 1.0f) * (dt * 0.01f);
        if (intensity > 1.0f)
        {
            intensity = 1.0f;
            intensityTarget = 0.0f;
        }
        if (intensity < 0.0f)
        {
            intensity = 0.0f;
            intensityTarget = 1.0f;
        }

        // Retail decrements the counter before testing the timer.
        dulcimerElapsed += dt;
        if (dulcimerCounter != 0)
        {
            --dulcimerCounter;
            if (dulcimerElapsed > dulcimerWait + 2.0f)
            {
                output.TriggerAccent(Accent::Dulcimer, AccentPredelayMs(episode, parentPlayTimeMs), random);
                dulcimerWait = dulcimerCounter != 0 ? random.Range(0.0f, 1.0f) : random.Range(10.0f, 35.0f);
                dulcimerElapsed = 0.0f;
            }
        }
        else if (dulcimerElapsed > dulcimerWait + 15.0f)
        {
            dulcimerCounter = random.Integer(1, 3);
            output.TriggerAccent(Accent::Dulcimer, AccentPredelayMs(episode, parentPlayTimeMs), random);
            dulcimerElapsed = 0.0f;
            dulcimerWait = random.Range(0.0f, 5.0f);
        }
        percussionElapsed += dt;
        if (percussionElapsed >= percussionWait + 15.0f && intensity > 0.5f &&
            std::count_if(channels.begin(), channels.end(), [](const Channel& channel) {
                return channel.phase == ChannelPhase::Playing;
            }) > 3)
        {
            output.TriggerAccent(Accent::Percussion, AccentPredelayMs(episode, parentPlayTimeMs), random);
            percussionWait = random.Range(0.0f, 15.0f);
            percussionElapsed = 0.0f;
        }

        for (std::size_t i = 0; i < channels.size(); ++i)
        {
            auto& channel = channels[i];
            VoiceSettings settings;
            settings.reverbSize = profile.reverbSize;
            settings.reverbWet = profile.reverbWet;
            if (i == VoiceCount - 1)
                settings.lowPassHz = episode == Episode::TBoGT ? 23900 : static_cast<int>(lastChildLowPass);

            switch (channel.phase)
            {
            case ChannelPhase::Reset:
                channel.panStart = random.Range(0.0f, 359.0f);
                channel.panEnd = random.Range(0.0f, 359.0f);
                channel.interpolation = 0.0f;
                channel.speed = random.Range(0.5f, 2.0f) * profile.speed[i];
                channel.phase = ChannelPhase::Waiting;
                channel.waitSeconds = random.Range(30.0f, 75.0f) *
                    (profile.intensity[i] * (1.0f - intensity) + profile.wait[i]);
                if (i == VoiceCount - 1 && episode != Episode::TBoGT)
                    lastChildLowPass = random.Range(700.0f, 8000.0f);
                break;
            case ChannelPhase::Waiting:
                channel.waitSeconds -= dt;
                if (channel.waitSeconds <= 0.0f)
                    channel.phase = ChannelPhase::Playing;
                break; // Remain silent until the following update, as on console.
            case ChannelPhase::Playing:
                settings.panDegrees = static_cast<int>(
                    (channel.panEnd - channel.panStart) * (channel.interpolation * 0.5f) + channel.panStart);
                settings.updatePositionAndPan = true;
                settings.volumeDb = GainToDb(EnvelopeGain(channel.interpolation));
                channel.interpolation += (channel.speed * dt) * 0.05f;
                if (channel.interpolation >= 2.0f)
                    channel.phase = ChannelPhase::Reset;
                break;
            }
            output.SetVoice(i, settings);
        }
        if (lobby || episode == Episode::TBoGT)
            NotifyInput(lobby, random);
    }
}
