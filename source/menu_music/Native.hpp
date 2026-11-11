#pragma once

#include "Session.hpp"

#include <atomic>

namespace MenuMusic
{
    struct audEntity;
    struct audController;
    struct audRequestedSettings;
    struct audWaveSlot;

    struct AudioPosition
    {
        float x, y, z, w;
    };

    struct audSoundInitParams
    {
        float volume = 0.0f;
        int pitch = 0;
        std::uint32_t reserved08 = 0;
        void* tracker = nullptr;
        std::uint32_t reserved16 = 0;
        const AudioPosition* position = nullptr;
        audWaveSlot* waveSlot = nullptr;
        void* category = nullptr;
        void* occlusionGroup = nullptr;
        int startOffset = 0;
        int predelay = 0;
        std::uint32_t reserved44 = 0;
        Sound* shadowSource = nullptr;
        std::uint32_t reserved52 = 0;
        int timerId = -1;
        float filterCutoff = 23900.0f;
        int pan = -1;
        std::uint8_t bucket = 0xFF;
        std::uint8_t route = 0;
        std::uint8_t flags = 0x20;
        std::uint8_t reserved71 = 0;
    };

    static_assert(sizeof(void*) == 4);
    static_assert(sizeof(audSoundInitParams) == 72);
    static_assert(offsetof(audSoundInitParams, shadowSource) == 48);
    static_assert(offsetof(audSoundInitParams, bucket) == 68);
    static_assert(offsetof(audSoundInitParams, flags) == 70);

    // Bind only after validating the executable and audio globals.
    struct NativeBindings
    {
        using CreateSound = void(__thiscall*)(audEntity*, const char*, Sound**, const audSoundInitParams*, int, int, int);
        CreateSound CreateSound_PersistentReference_Name = nullptr;
        CreateSound CreateSound_LocalReference_Name = nullptr;
        Sound* (__thiscall* GetChildSoundReference)(audController*, Sound*, const char*, audEntity*, Sound**) = nullptr;
        int (__thiscall* Prepare)(Sound*, audWaveSlot*, bool) = nullptr;
        void (__thiscall* Play)(Sound*) = nullptr;
        void (__thiscall* PrepareAndPlay)(Sound*, audWaveSlot*, bool, int) = nullptr;
        void (__thiscall* StopAndForget)(Sound*, bool) = nullptr;
        void (__thiscall* SetVolume)(audRequestedSettings*, float) = nullptr;
        void (__thiscall* SetPan)(audRequestedSettings*, int) = nullptr;
        void (__thiscall* SetPosition)(audRequestedSettings*, const AudioPosition*) = nullptr;
        void (__thiscall* SetLowPassFilterCutoff)(audRequestedSettings*, unsigned int) = nullptr;
        void (__thiscall* SetShadowSoundId)(audRequestedSettings*, std::uint8_t) = nullptr;
        std::int16_t (__cdecl* FindBankId)(const char*) = nullptr;
        int (__thiscall* GetBankLoadingStatus)(audWaveSlot*, std::int16_t) = nullptr;
        void (__thiscall* LoadBank)(audWaveSlot*, std::int16_t) = nullptr;
        float (__cdecl* GetRandomNumberInRange_Float)(float, float) = nullptr;
        int (__cdecl* GetRandomNumberInRange_Int)(int, int) = nullptr;
        audController* controller = nullptr;
        std::byte* soundBuckets = nullptr;
        std::uint32_t soundStride = 0;
        std::uint32_t requestedSettingsStride = 0;
        std::uint8_t streamingBucket = 0xFF;

        bool Complete() const;
    };

    class NativeRandom final : public Random
    {
    public:
        explicit NativeRandom(const NativeBindings& bindings) : bindings(bindings) {}
        float Range(float first, float last) override;
        int Integer(int first, int exclusiveLast) override;

    private:
        const NativeBindings& bindings;
    };

    class NativeAudio
    {
    public:
        NativeAudio(const NativeBindings& bindings, audEntity* entity) : bindings(bindings), entity(entity) {}
        NativeAudio(const NativeAudio&) = delete;
        NativeAudio& operator=(const NativeAudio&) = delete;

        bool CreateParent(Sound*& output);
        bool AcquireChild(Sound* parent, std::size_t child, Sound*& output, const AudioPosition& position);
        bool CreateShadow(Episode episode, Sound* source, std::size_t shadow, Sound*& output, const AudioPosition& position);
        PrepareStatus Prepare(Sound* parent, audWaveSlot* waveSlot);
        void PlayParent(Sound* parent);
        BankStatus GetBankLoadingStatus(audWaveSlot* waveSlot, const char* bank) const;
        void LoadBank(audWaveSlot* waveSlot, const char* bank) const;
        bool ReferencesValid() const;
        int ParentPlayTimeMs(Sound* parent) const;
        void SetParentVolume(Sound* parent, float volume);
        void SetVoice(Sound* sound, const VoiceSettings& settings, const AudioPosition& position);
        void TriggerMusicOneshot(Accent accent, int predelay, audWaveSlot* waveSlot, const AudioPosition& position, Random& random);
        void InvalidateEntitySoundReference(Sound*& sound);
        void StopAndForget(Sound*& sound);
        bool CleanupComplete() const;
        bool ResetReferences();
        void EngineStopped() { engineStopped = true; }

    private:
        bool Register(Sound*& output);
        audRequestedSettings* GetRequestedSettings(Sound* sound) const;
        bool HasRegisteredSounds(std::uint16_t entityId) const;
        std::uint8_t GetSoundId(Sound* sound) const;

        const NativeBindings& bindings;
        audEntity* entity;
        std::array<Sound**, VoiceCount + 1> references{};
        std::size_t referenceCount = 0;
        std::uint16_t entityId = 0xFFFF;
        bool engineStopped = false;
    };
    struct audWaveSlot;

    struct audStreamClientSettings
    {
        bool (__cdecl* OnMusicStopCallback)(void*) = nullptr;
        bool (__cdecl* HasMusicStoppedCallback)(void*) = nullptr;
        void* userData = nullptr;
        std::uint8_t priority = 0;
        std::uint8_t padding[3]{};
    };

    struct audStreamSlot
    {
        audStreamClientSettings requested;
        audStreamClientSettings active;
        audWaveSlot* waveSlot = nullptr;
        std::uint32_t reserved36 = 0;
        std::uint8_t state = 0;
        std::uint8_t padding[3]{};
    };

    static_assert(sizeof(audStreamClientSettings) == 16);
    static_assert(sizeof(audStreamSlot) == 44);
    static_assert(offsetof(audStreamSlot, state) == 40);

    struct StreamBindings
    {
        Stream* (__cdecl* AllocateSlot)(const audStreamClientSettings*) = nullptr;
        void (__thiscall* Free)(Stream*) = nullptr;
    };

    class NativeStream
    {
    public:
        using CleanupPredicate = bool(*)(void*);
        NativeStream(const StreamBindings& bindings, CleanupPredicate cleanupComplete, void* owner)
            : bindings(bindings), cleanupComplete(cleanupComplete), owner(owner) {}
        NativeStream(const NativeStream&) = delete;
        NativeStream& operator=(const NativeStream&) = delete;

        Stream* Allocate();
        StreamStatus Status(Stream* stream) const;
        void Free(Stream* stream);
        bool CleanupComplete();

    private:
        static bool __cdecl OnMusicStopCallback(void* context);
        static bool __cdecl HasMusicStoppedCallback(void* context);
        bool Owns(const audStreamClientSettings& settings) const;

        const StreamBindings& bindings;
        CleanupPredicate cleanupComplete;
        void* owner;
        Stream* allocated = nullptr;
        std::atomic<bool> stopRequested = false;
    };
}
