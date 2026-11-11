#include "NativeBackend.hpp"
#include "StreamRefill.hpp"

namespace MenuMusic
{
    bool NativeBackend::SoundsReleased(void* context)
    {
        return static_cast<NativeAudio*>(context)->CleanupComplete();
    }

    Stream* NativeBackend::AllocateStream()
    {
        return audio.ResetReferences() ? stream.Allocate() : nullptr;
    }

    StreamStatus NativeBackend::GetStreamStatus(Stream* slot)
    {
        return stream.Status(slot);
    }

    bool NativeBackend::CreateParent(Episode newEpisode, Sound*& output)
    {
        episode = newEpisode;
        parent = &output;
        return audio.CreateParent(output);
    }

    bool NativeBackend::AcquireChild(Sound* sound, std::size_t child, Sound*& output)
    {
        if (child >= 7)
            return false;
        voices[child * 2] = &output;
        return audio.AcquireChild(sound, child, output, environment.ListenerPosition());
    }

    bool NativeBackend::CreateShadow(Episode currentEpisode, Sound* source, std::size_t shadow, Sound*& output)
    {
        if (shadow >= 6)
            return false;
        voices[shadow * 2 + 1] = &output;
        return audio.CreateShadow(currentEpisode, source, shadow, output, environment.ListenerPosition());
    }

    PrepareStatus NativeBackend::Prepare(Sound* sound, Stream* slot)
    {
        if (stream.Status(slot) != StreamStatus::Ready)
            return PrepareStatus::Failed;
        loopingWaveSlot = slot->waveSlot;
        loopingMenuWaveSlot.store(loopingWaveSlot);
        return audio.Prepare(sound, slot->waveSlot);
    }

    BankStatus NativeBackend::GetAccentBankStatus(const char* bank)
    {
        return audio.GetBankLoadingStatus(environment.AccentWaveSlot(), bank);
    }

    void NativeBackend::RequestAccentBank(const char* bank)
    {
        audio.LoadBank(environment.AccentWaveSlot(), bank);
    }

    void NativeBackend::PlayParent(Sound* sound)
    {
        audio.PlayParent(sound);
    }

    bool NativeBackend::ReferencesValid(Sound*, const std::array<Sound*, VoiceCount>&)
    {
        return audio.ReferencesValid();
    }

    int NativeBackend::ParentPlayTimeMs(Sound* sound)
    {
        return audio.ParentPlayTimeMs(sound);
    }

    void NativeBackend::ForgetChild(Sound*& child)
    {
        audio.InvalidateEntitySoundReference(child);
    }

    void NativeBackend::StopAndForget(Sound*& sound)
    {
        audio.StopAndForget(sound);
    }

    void NativeBackend::FreeStream(Stream* slot)
    {
        ReleaseLoopingWaveSlot();
        stream.Free(slot);
    }

    void NativeBackend::ReleaseLoopingWaveSlot()
    {
        void* expected = loopingWaveSlot;
        loopingMenuWaveSlot.compare_exchange_strong(expected, nullptr);
        loopingWaveSlot = nullptr;
    }

    bool NativeBackend::CleanupComplete()
    {
        if (!stream.CleanupComplete())
            return false;
        parent = nullptr;
        voices = {};
        return audio.ResetReferences();
    }

    void NativeBackend::RestoreEffects()
    {
        environment.RestoreEffects();
    }

    void NativeBackend::SetEffects(const Profile& profile)
    {
        environment.SetEffects(profile);
        if (parent && *parent)
            audio.SetParentVolume(*parent, 0.0f);
    }

    void NativeBackend::SetVoice(std::size_t voice, const VoiceSettings& settings)
    {
        if (voice < voices.size() && voices[voice] && *voices[voice])
            audio.SetVoice(*voices[voice], settings, environment.ListenerPosition());
    }

    void NativeBackend::TriggerAccent(Accent accent, int predelay, Random& random)
    {
        if (AccentBank(episode))
            audio.TriggerMusicOneshot(accent, predelay, environment.AccentWaveSlot(), environment.ListenerPosition(), random);
    }
}
