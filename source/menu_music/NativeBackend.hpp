#pragma once

#include "Native.hpp"

namespace MenuMusic
{
    struct NativeEnvironment
    {
        virtual ~NativeEnvironment() = default;
        virtual AudioPosition ListenerPosition() const = 0;
        virtual audWaveSlot* AccentWaveSlot() const = 0;
        virtual void SetEffects(const Profile& profile) = 0;
        virtual void RestoreEffects() = 0;
    };

    class NativeBackend final : public Backend
    {
    public:
        NativeBackend(const NativeBindings& audioBindings, const StreamBindings& streamBindings, audEntity* entity, NativeEnvironment& environment)
            : audio(audioBindings, entity), stream(streamBindings, SoundsReleased, &audio), environment(environment) {}

        Stream* AllocateStream() override;
        StreamStatus GetStreamStatus(Stream* slot) override;
        bool CreateParent(Episode episode, Sound*& output) override;
        bool AcquireChild(Sound* parent, std::size_t child, Sound*& output) override;
        bool CreateShadow(Episode episode, Sound* source, std::size_t shadow, Sound*& output) override;
        PrepareStatus Prepare(Sound* parent, Stream* slot) override;
        BankStatus GetAccentBankStatus(const char* bank) override;
        void RequestAccentBank(const char* bank) override;
        void PlayParent(Sound* sound) override;
        bool ReferencesValid(Sound* parent, const std::array<Sound*, VoiceCount>& voices) override;
        int ParentPlayTimeMs(Sound* sound) override;
        void ForgetChild(Sound*& child) override;
        void StopAndForget(Sound*& sound) override;
        void FreeStream(Stream* slot) override;
        bool CleanupComplete() override;
        void RestoreEffects() override;
        void SetEffects(const Profile& profile) override;
        void SetVoice(std::size_t voice, const VoiceSettings& settings) override;
        void TriggerAccent(Accent accent, int predelay, Random& random) override;
        void EngineStopped() { ReleaseLoopingWaveSlot(); audio.EngineStopped(); }

    private:
        static bool SoundsReleased(void* context);
        void ReleaseLoopingWaveSlot();

        NativeAudio audio;
        NativeStream stream;
        NativeEnvironment& environment;
        Episode episode = Episode::IV;
        Sound** parent = nullptr;
        audWaveSlot* loopingWaveSlot = nullptr;
        std::array<Sound**, VoiceCount> voices{};
    };
}
