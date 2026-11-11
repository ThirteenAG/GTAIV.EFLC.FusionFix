#include "Session.hpp"

#include <algorithm>

namespace MenuMusic
{
    bool Session::HasReferences() const
    {
        return parent != nullptr || std::any_of(voices.begin(), voices.end(), [](Sound* sound) { return sound != nullptr; });
    }

    bool Session::CreateVoices()
    {
        if (!backend.CreateParent(episode, parent) || parent == nullptr)
            return false;
        for (std::size_t i = 0; i < VoiceCount; ++i)
        {
            const bool success = IsShadow(i)
                ? backend.CreateShadow(episode, voices[i - 1], ChildIndex(i), voices[i])
                : backend.AcquireChild(parent, ChildIndex(i), voices[i]);
            if (!success || voices[i] == nullptr)
                return false;
        }
        return true;
    }

    bool Session::HasAllReferences() const
    {
        return parent != nullptr && std::all_of(voices.begin(), voices.end(), [](Sound* sound) { return sound != nullptr; });
    }

    void Session::BeginStop(bool didFail)
    {
        if (state == SessionState::Stopping)
            return;
        failed = didFail;
        state = SessionState::Stopping;
        mixer.Stop();
        for (std::size_t i = 0; i < VoiceCount; ++i)
        {
            if (voices[i] == nullptr)
                continue;
            if (IsShadow(i))
                backend.StopAndForget(voices[i]);
            else
                backend.ForgetChild(voices[i]);
        }
        if (parent != nullptr)
            backend.StopAndForget(parent);
    }

    void Session::Stop()
    {
        if (state == SessionState::Failed)
            state = SessionState::Idle;
        else if (state != SessionState::Idle)
            BeginStop(false);
    }

    void Session::Update(bool shouldPlay, Episode requestedEpisode, float elapsedSeconds, bool lobby, Random& random, bool advanceMix)
    {
        if (state == SessionState::Stopping)
        {
            if (HasReferences())
                return;
            if (stream != nullptr)
            {
                backend.FreeStream(stream);
                stream = nullptr;
            }
            if (!backend.CleanupComplete())
                return;
            if (effectsApplied)
                backend.RestoreEffects();
            effectsApplied = created = false;
            state = failed && shouldPlay ? SessionState::Failed : SessionState::Idle;
            return;
        }
        if (!shouldPlay || (state != SessionState::Idle && requestedEpisode != episode))
        {
            Stop();
            return;
        }
        if (stream && backend.GetStreamStatus(stream) == StreamStatus::Lost)
        {
            BeginStop(false);
            return;
        }
        switch (state)
        {
        case SessionState::Idle:
            episode = requestedEpisode;
            state = SessionState::WaitingForStream;
            break;
        case SessionState::WaitingForStream:
            if (stream == nullptr)
                stream = backend.AllocateStream();
            if (stream != nullptr && backend.GetStreamStatus(stream) == StreamStatus::Ready)
                state = SessionState::Preparing;
            break;
        case SessionState::Preparing:
        {
            if (!created)
            {
                if (!CreateVoices())
                {
                    BeginStop(true);
                    break;
                }
                created = true;
            }
            if (!HasAllReferences() || !backend.ReferencesValid(parent, voices))
            {
                BeginStop(true);
                break;
            }
            const auto prepared = backend.Prepare(parent, stream);
            if (prepared == PrepareStatus::Failed)
                BeginStop(true);
            if (prepared != PrepareStatus::Ready)
                break;
            if (const char* bank = AccentBank(episode))
            {
                const auto status = backend.GetAccentBankStatus(bank);
                if (status == BankStatus::Absent)
                    backend.RequestAccentBank(bank);
                if (status == BankStatus::Failed)
                    BeginStop(true);
                if (status != BankStatus::Ready)
                    break;
            }
            backend.PlayParent(parent);
            mixer.Start(episode, random);
            state = SessionState::Playing;
            break;
        }
        case SessionState::Playing:
            if (!HasAllReferences() || !backend.ReferencesValid(parent, voices))
            {
                BeginStop(true);
                break;
            }
            if (advanceMix)
            {
                effectsApplied = true;
                mixer.Update(elapsedSeconds, backend.ParentPlayTimeMs(parent), lobby, random, backend);
            }
            break;
        case SessionState::Failed:
            break; // Retry only after eligibility ends or the episode changes.
        default:
            break;
        }
    }
}
