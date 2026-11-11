#pragma once

#include "Mixer.hpp"

namespace MenuMusic
{
    struct audSound;
    struct audStreamSlot;
    using Sound = audSound;
    using Stream = audStreamSlot;
    enum class StreamStatus { Pending, Ready, Lost };
    enum class PrepareStatus { Pending, Ready, Failed };
    enum class BankStatus { Ready, Pending, Failed, Absent };

    // Register these stable output addresses on first acquisition.
    struct Backend : Output
    {
        virtual Stream* AllocateStream() = 0;
        virtual StreamStatus GetStreamStatus(Stream* stream) = 0;
        virtual bool CreateParent(Episode episode, Sound*& output) = 0;
        virtual bool AcquireChild(Sound* parent, std::size_t child, Sound*& output) = 0;
        virtual bool CreateShadow(Episode episode, Sound* source, std::size_t shadow, Sound*& output) = 0;
        virtual PrepareStatus Prepare(Sound* parent, Stream* stream) = 0;
        virtual BankStatus GetAccentBankStatus(const char* bank) = 0;
        virtual void RequestAccentBank(const char* bank) = 0;
        virtual void PlayParent(Sound* parent) = 0;
        virtual bool ReferencesValid(Sound* parent, const std::array<Sound*, VoiceCount>& voices) = 0;
        virtual int ParentPlayTimeMs(Sound* parent) = 0;

        // Children belong to the parent; only shadows are stopped independently.
        virtual void ForgetChild(Sound*& child) = 0;
        virtual void StopAndForget(Sound*& sound) = 0;
        virtual void FreeStream(Stream* stream) = 0;
        // Null output slots alone do not establish native cleanup.
        virtual bool CleanupComplete() = 0;
        virtual void RestoreEffects() = 0;
    };

    enum class SessionState { Idle, WaitingForStream, Preparing, Playing, Stopping, Failed };

    // The engine retains addresses of this object's sound slots until cleanup.
    class Session
    {
    public:
        explicit Session(Backend& backend) : backend(backend) {}
        Session(const Session&) = delete;
        Session& operator=(const Session&) = delete;
        Session(Session&&) = delete;
        Session& operator=(Session&&) = delete;

        void Update(bool shouldPlay, Episode requestedEpisode, float elapsedSeconds, bool lobby, Random& random, bool advanceMix = true);
        void NotifyInput(bool lobby, Random& random) { mixer.NotifyInput(lobby, random); }
        void Stop();
        SessionState State() const { return state; }
        bool CanDestroy() const { return state == SessionState::Idle || state == SessionState::Failed; }

    private:
        void BeginStop(bool failed);
        bool CreateVoices();
        bool HasReferences() const;
        bool HasAllReferences() const;

        Backend& backend;
        Mixer mixer;
        SessionState state = SessionState::Idle;
        Episode episode = Episode::IV;
        bool failed = false;
        bool created = false;
        bool effectsApplied = false;
        Stream* stream = nullptr;
        Sound* parent = nullptr;
        std::array<Sound*, VoiceCount> voices{};
    };

    class Frontend
    {
    public:
        Frontend(Backend& backend, Random& random) : session(backend), random(random) {}

        void Update(bool eligible, Episode episode, std::uint32_t time, bool lobby, bool stopRequested)
        {
            const auto delta = time - lastTime;
            const bool advance = !clockReady || (delta != 0 && delta < 0x80000000u);
            const float elapsed = clockReady && delta < 0x80000000u ? delta * 0.001f : 0.0f;
            clockReady = true;
            lastTime = time;
            if (stopRequested)
                session.Stop();
            session.Update(eligible && !stopRequested, episode, elapsed, lobby, random, advance);
        }

        void NotifyInput(bool lobby) { session.NotifyInput(lobby, random); }
        void Stop() { session.Stop(); }
        SessionState State() const { return session.State(); }
        int NativeState() const
        {
            switch (session.State())
            {
            case SessionState::WaitingForStream: return 1;
            case SessionState::Preparing: return 2;
            case SessionState::Playing: return 3;
            case SessionState::Stopping: return 4;
            default: return 0;
            }
        }

    private:
        Session session;
        Random& random;
        std::uint32_t lastTime = 0;
        bool clockReady = false;
    };
}
