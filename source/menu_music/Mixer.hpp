#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace MenuMusic
{
    inline constexpr std::size_t VoiceCount = 13;
    inline constexpr float SilenceDb = -100.0f;

    enum class Episode { IV, TLAD, TBoGT };
    enum class ChannelPhase { Reset, Waiting, Playing };
    enum class Accent { Dulcimer, Percussion };

    struct Profile
    {
        std::array<float, VoiceCount> wait;
        std::array<float, VoiceCount> speed;
        std::array<float, VoiceCount> intensity;
        float reverbSize;
        float reverbWet;
        float largeReverbHighPassHz;
        float largeDelayFeedbackDb;
    };

    const Profile& GetProfile(Episode episode);
    // Child slots are interleaved with shadows: child0, shadow0, ... child6.
    constexpr bool IsShadow(std::size_t voice) { return (voice & 1) != 0; }
    constexpr std::size_t ChildIndex(std::size_t voice) { return voice / 2; }
    bool SuppressEnvironmentReverb(Episode episode, std::size_t voice);
    const char* AccentBank(Episode episode); // TLAD's accent wrappers are null.
    int AccentPredelayMs(Episode episode, int parentPlayTimeMs);
    float EnvelopeGain(float interpolation);
    float GainToDb(float gain);

    // Descending float bounds are intentional; integer upper bounds are exclusive.
    struct Random
    {
        virtual ~Random() = default;
        virtual float Range(float first, float last) = 0;
        virtual int Integer(int first, int exclusiveLast) = 0;
    };

    struct VoiceSettings
    {
        float volumeDb = SilenceDb;
        int panDegrees = 0;
        bool updatePositionAndPan = false;
        float reverbSize = 0.0f;
        float reverbWet = 0.0f;
        // Only the final controlled child has an explicit low-pass override.
        int lowPassHz = -1;
    };

    struct Output
    {
        virtual ~Output() = default;
        virtual void SetEffects(const Profile& profile) = 0;
        virtual void SetVoice(std::size_t voice, const VoiceSettings& settings) = 0;
        virtual void TriggerAccent(Accent accent, int predelayMs, Random& random) = 0;
    };

    struct Channel
    {
        ChannelPhase phase = ChannelPhase::Reset;
        float interpolation = 0.0f;
        float speed = 0.0f;
        float panStart = 0.0f;
        float panEnd = 0.0f;
        float waitSeconds = 0.0f;
    };

    class Mixer
    {
    public:
        void Start(Episode episode, Random& random);
        void Stop();
        // Use the frontend's unscaled clock.
        void Update(float elapsedSeconds, int parentPlayTimeMs, bool lobby, Random& random, Output& output);
        // Consume the pending input response once per start.
        void NotifyInput(bool lobby, Random& random);

        bool IsRunning() const { return running; }
        Episode GetEpisode() const { return episode; }
        float Intensity() const { return intensity; }
        const std::array<Channel, VoiceCount>& Channels() const { return channels; }

    private:
        bool running = false;
        bool pendingInput = false;
        Episode episode = Episode::IV;
        std::array<Channel, VoiceCount> channels{};
        float intensity = 0.0f;
        float intensityTarget = 1.0f;
        float dulcimerElapsed = 0.0f;
        float dulcimerWait = 0.0f;
        int dulcimerCounter = 0;
        float percussionElapsed = 0.0f;
        float percussionWait = 0.0f;
        float lastChildLowPass = 7000.0f;
    };
}
