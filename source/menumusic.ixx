module;

#include <common.hxx>
#include <xmmintrin.h>

export module menumusic;

import common;
import comvars;

// Pause menu music. The game plays the menu music stream as it is. Consoles mix it while it plays: the seven stems of
// the stream fade in and out on their own and move around the listener, shadow sounds send them to the reverb,
// one-shots play on top (IV, TBoGT) and the first menu input brings waiting stems in sooner. This mixes it that way
// again with the sounds the PC version has, and keeps the stream playing through its loop point.
namespace MenuMusic
{
    struct audSound;
    struct audWaveSlot;
    struct audRequestedSettings;

    template<typename T>
    T& Field(void* object, size_t offset)
    {
        return *reinterpret_cast<T*>(static_cast<uint8_t*>(object) + offset);
    }

    struct Position
    {
        float x, y, z, w;
    };

    // rage::audSoundInitParams with the values of its constructor
    struct audSoundInitParams
    {
        float volume = 0.0f;
        int pitch = 0;
        uint32_t pad08 = 0;
        void* tracker = nullptr;
        uint32_t pad10 = 0;
        const Position* position = nullptr;
        audWaveSlot* waveSlot = nullptr;
        void* category = nullptr;
        void* occlusionGroup = nullptr;
        int startOffset = 0;
        int predelay = 0;
        uint32_t pad2C = 0;
        audSound* shadowSource = nullptr;
        uint32_t pad34 = 0;
        int timerId = -1;
        float filterCutoff = 23900.0f;
        int pan = -1;
        uint8_t bucket = 0xFF;
        uint8_t route = 0;
        uint8_t flags = 0x20; // the stems and their shadows add 0x40
        uint8_t pad47 = 0;
    };
    static_assert(sizeof(audSoundInitParams) == 0x48);

    struct audStreamClientSettings
    {
        bool(__cdecl* OnStop)(void*) = nullptr;
        bool(__cdecl* HasStopped)(void*) = nullptr;
        void* userData = nullptr;
        uint8_t priority = 0;
    };

    struct audStreamSlot
    {
        audStreamClientSettings requested;
        audStreamClientSettings active;
        audWaveSlot* waveSlot;
        uint32_t pad24;
        uint8_t state; // 1 = the active client has the slot
    };
    static_assert(sizeof(audStreamSlot) == 0x2C);

    // In rage::audRequestedSettings at +0xA0
    struct OcclusionMetric
    {
        float occlusion, filterCutoff, pad08, reverbWet, reverbSize;
        uint8_t pad14[16];
        bool suppressEnvironmentReverb;
    };

    struct audEffect
    {
        void** vtable;
        const uint8_t* metadata; // the first byte is the effect type
        audEffect* next;

        template<typename T>
        T* Settings(uint8_t type)
        {
            if (*metadata != type)
                return nullptr;
            return reinterpret_cast<T*(__thiscall*)(audEffect*)>(vtable[4])(this);
        }
    };

    struct BiquadSettings
    {
        float cutoff, gain, bandwidth, pad0C;
        int type;
        bool bypass;
    };

    struct DelaySettings
    {
        std::array<int, 6> time; // ms
        std::array<float, 6> wet;
        std::array<float, 6> feedback;
    };

    struct PhysicalDelay
    {
        void** vtable;
        uint32_t position;
        std::array<uint32_t, 6> time; // samples
        std::array<float, 6> wet, feedback;
        float* buffer;
        uint32_t capacity;
    };

    struct audDelayEffect
    {
        audEffect effect;
        uint8_t pad0C[32];
        uint32_t readIndex;
        uint8_t pad30[68];
        PhysicalDelay* physical;
        std::array<DelaySettings, 3> settings;
    };
    static_assert(offsetof(audDelayEffect, settings) == 0x78);

    struct ReverbSettings
    {
        float feedback, wet, dry, damping;
        bool mono, bypass;
        uint16_t pad12;
    };

    struct alignas(16) PhysicalReverb
    {
        void** vtable;
        uint8_t pad04[12];
        float coefficients[5][4];
        void* combBuffers;
        void* combHistory;
        void* combPositions;
        float feedback;
        void* allPassBuffers;
        void* allPassPositions;
        ReverbSettings settings[3];
        uint32_t readIndex, writeIndex;
        uint8_t channelCount, activeChannels, mask, padBF;
    };
    static_assert(sizeof(PhysicalReverb) == 0xC0);
    static_assert(offsetof(PhysicalReverb, settings) == 0x78);

    void(__thiscall* rage__audEntity__CreateSound_PersistentReference)(void* entity, const char* name, audSound** sound, const audSoundInitParams* params, int, int, int);
    void(__thiscall* rage__audEntity__CreateSound_LocalReference)(void* entity, const char* name, audSound** sound, const audSoundInitParams* params, int, int, int);
    audSound*(__thiscall* rage__audController__GetChildSoundReference)(void* controller, audSound* parent, const char* path, void* entity, audSound** sound);
    int(__thiscall* rage__audSound__Prepare)(audSound* sound, audWaveSlot* waveSlot, bool);
    void(__thiscall* rage__audSound__Play)(audSound* sound);
    void(__thiscall* rage__audSound__PrepareAndPlay)(audSound* sound, audWaveSlot* waveSlot, bool, int);
    void(__thiscall* rage__audSound__StopAndForget)(audSound* sound, bool);
    void(__thiscall* rage__audRequestedSettings__SetVolume)(audRequestedSettings* settings, float db);
    void(__thiscall* rage__audRequestedSettings__SetPan)(audRequestedSettings* settings, int degrees);
    void(__thiscall* rage__audRequestedSettings__SetPosition)(audRequestedSettings* settings, const Position* position);
    void(__thiscall* rage__audRequestedSettings__SetLowPassFilterCutoff)(audRequestedSettings* settings, uint32_t hz);
    void(__thiscall* rage__audRequestedSettings__SetShadowSoundId)(audRequestedSettings* settings, uint8_t id);
    int16_t(__cdecl* rage__audWaveSlot__FindBankId)(const char* bank);
    int(__thiscall* rage__audWaveSlot__GetBankLoadingStatus)(audWaveSlot* waveSlot, int16_t bank);
    void(__thiscall* rage__audWaveSlot__LoadBank)(audWaveSlot* waveSlot, int16_t bank);
    void*(__thiscall* rage__audCategoryManager__GetCategoryPtr)(void* manager, uint32_t hash);
    int(__thiscall* rage__IMA_ADPCM__Decode)(void* state, void* output, const void* input, int, uint32_t count);
    audStreamSlot*(__cdecl* audStreamSlot__AllocateSlot)(const audStreamClientSettings* settings);
    void(__thiscall* audStreamSlot__Free)(audStreamSlot* slot);
    bool(__thiscall* audFrontendAudioEntity__ShouldFEMusicBePlaying)(void* entity);
    bool(__cdecl* CNetwork__IsOnLobbyScreen)();
    int(__thiscall* CProfileSettings__GetInt)(void* profile, int setting);
    bool(__thiscall* CNetworkPeerMgr__IsAnyRemotePlayerTalking)(void* peerMgr);
    bool(__thiscall* CNetworkPeerMgr__IsAnyRemotePlayerAboutToTalk)(void* peerMgr);

    void** pController;
    void* pCategoryManager;
    uint8_t** pSoundBuckets;
    uint32_t* pSoundStride;
    uint32_t* pRequestedSettingsStride;
    uint8_t* pStreamingBucket;
    float* pListenerPositions;
    uint32_t* pTlsIndex;
    uint32_t listenerIndexOffset;
    bool* pPhoneActive;
    bool* pPhoneMusicAllowed;
    void** pAudioDriver;
    uint32_t* pMixBlockSamples;
    const char** pMinSpecAudio;
    int* pHeadphones;
    void** pProfileSettings;
    void* pNetworkPeerMgr;
    uintptr_t updateMusicEnd;

    SafetyHookInline shNotifyFrontendInput, shShutdownClass, shRefillAdpcmBuffer, shAddAdpcmSampleData, shRefillPcmBuffer, shAddPcmSampleData;
    SafetyHookInline shReverbProcess;

    constexpr size_t VoiceCount = 13; // stem 0, shadow 0, stem 1, ... shadow 5, stem 6
    constexpr float Silence = -100.0f; // dB

    struct MixProfile
    {
        std::array<float, VoiceCount> wait, speed, intensity;
        float reverbSize, reverbWet, largeReverbHighPass, largeDelayFeedback;
    };

    // IV, TLAD
    constexpr MixProfile OriginalMix
    {
        { 1.4f, 1.0f, 1.0f, 1.5f, 1.8f, 1.8f, 2.0f, 2.0f, 2.2f, 2.2f, 2.1f, 2.4f, 1.1f },
        { 0.5f, 0.8f, 0.7f, 0.6f, 0.9f, 0.8f, 1.4f, 1.3f, 1.4f, 1.1f, 1.2f, 1.65f, 0.8f },
        { 0.0f, 0.0f, 0.0f, 0.0f, 0.3f, 0.3f, 0.4f, 0.4f, 1.0f, 1.0f, 1.25f, 1.25f, 2.0f },
        1.0f, 0.4f, 250.0f, -6.0f
    };

    // TBoGT
    constexpr MixProfile BalladMix
    {
        { 1.3f, 1.0f, 1.2f, 1.6f, 1.6f, 1.7f, 1.8f, 1.8f, 1.9f, 1.9f, 2.0f, 2.0f, 2.0f },
        { 0.5f, 0.8f, 0.7f, 0.6f, 0.9f, 0.8f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.8f },
        { 0.0f, 0.0f, 0.0f, 0.0f, 0.3f, 0.3f, 0.4f, 0.4f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f },
        0.75f, 0.6f, 200.0f, -9.0f
    };

    struct Channel
    {
        enum { Reset, Waiting, Playing } phase;
        float interpolation, speed, panStart, panEnd, wait;
    };

    // The first five are the states of the game's own menu music (audFrontendAudioEntity+0x200)
    enum class State { Idle, WaitingForStream, Preparing, Playing, Stopping, Failed };

    void* entity; // the audFrontendAudioEntity
    void* category;
    bool engineStopped = false;
    State state = State::Idle;
    int episode = 0;
    bool failed = false;
    bool created = false;
    bool effectsApplied = false;
    uint32_t lastTime = 0;

    // The engine keeps the addresses of these and clears them when it frees the sounds
    audSound* parent = nullptr;
    std::array<audSound*, VoiceCount> voices{};
    audStreamSlot* stream = nullptr;
    std::atomic<bool> streamStopRequested = false;

    std::array<Channel, VoiceCount> channels{};
    bool pendingInput = false;
    float intensity = 0.0f;
    float intensityTarget = 1.0f;
    float dulcimerElapsed = 0.0f;
    float dulcimerWait = 0.0f;
    int dulcimerCounter = 0;
    float percussionElapsed = 0.0f;
    float percussionWait = 0.0f;
    float lastStemLowPass = 7000.0f;

    // Read by the audio thread
    std::atomic<void*> loopingWaveSlot = nullptr;
    std::atomic<const MixProfile*> reverbProfile = nullptr;
    std::atomic<audEffect*> largeReverbDelay = nullptr;
    std::atomic<bool> delayReset = false;
    std::array<std::atomic<PhysicalReverb*>, 3> sourceReverbs{};
    std::atomic<uint32_t> reverbGeneration = 1;

    float categoryBaseVolume = 0.0f;
    float categoryAppliedVolume = 0.0f;
    bool categoryVolumeOwned = false;

    struct ControllerLock
    {
        ControllerLock() { EnterCriticalSection(&Field<CRITICAL_SECTION>(*pController, 0x3210)); }
        ~ControllerLock() { LeaveCriticalSection(&Field<CRITICAL_SECTION>(*pController, 0x3210)); }
    };

    // 'to' may be below 'from'
    float Random(float from, float to)
    {
        return from + (to - from) * (float(game_rand()) / 32767.0f);
    }

    Position ListenerPosition()
    {
        // Use the TLS-selected listener buffer (four 0x40-byte slots), as on Xenon.
        auto tls = Field<void**>(NtCurrentTeb(), 0x2C)[*pTlsIndex];
        auto index = Field<uint32_t>(tls, listenerIndexOffset);
        return index < 4 ? Field<Position>(pListenerPositions, 0x40 * index) : Position{};
    }

    // A sound pool bucket is 0x6F40 bytes, with its sounds at +0x6F10 and their requested settings at +0x6F14. A sound
    // has its bucket at +0x40 and its settings slot at +4 (0xFF = none).
    audRequestedSettings* GetRequestedSettings(audSound* sound)
    {
        if (!sound || Field<uint8_t>(sound, 4) == 0xFF)
            return nullptr;
        auto bucket = *pSoundBuckets + 0x6F40 * Field<uint8_t>(sound, 0x40);
        return reinterpret_cast<audRequestedSettings*>(Field<uint8_t*>(bucket, 0x6F14) + *pRequestedSettingsStride * Field<uint8_t>(sound, 4));
    }

    uint8_t GetSoundId(audSound* sound)
    {
        auto bucket = *pSoundBuckets + 0x6F40 * Field<uint8_t>(sound, 0x40);
        return uint8_t((reinterpret_cast<uint8_t*>(sound) - Field<uint8_t*>(bucket, 0x6F10)) / *pSoundStride);
    }

    OcclusionMetric& GetOcclusionMetric(audRequestedSettings* settings)
    {
        return Field<OcclusionMetric>(settings, 0xA0);
    }

    bool IsOwnSlot(audSound** slot)
    {
        return slot == &parent || (slot >= voices.data() && slot < voices.data() + voices.size());
    }

    // The controller links the sounds of an entity from +0xFA4 + 8 * entity id: 4-byte nodes with the next node, the
    // bucket and the sound's index in it. A sound has the address of the pointer that refers to it at +0x90.
    bool HasRegisteredSounds(uint16_t entityId)
    {
        for (auto index = Field<uint16_t>(*pController, 0xFA4 + 8 * entityId); index != 0xFFFF;)
        {
            auto node = static_cast<uint8_t*>(*pController) + 4 * index;
            auto bucket = *pSoundBuckets + 0x6F40 * node[2];
            auto sound = Field<uint8_t*>(bucket, 0x6F10) + *pSoundStride * node[3];
            if (IsOwnSlot(Field<audSound**>(sound, 0x90)))
                return true;
            index = Field<uint16_t>(node, 0);
        }
        return false;
    }

    // The engine has no sound of the last session left: neither in the list of the frontend entity (id 0xFFFF once
    // it's shut down) nor in that of entity 0
    bool SoundsReleased()
    {
        if (engineStopped)
            return true;
        ControllerLock lock;
        auto id = Field<uint16_t>(entity, 4);
        return !(id != 0xFFFF && HasRegisteredSounds(id)) && !HasRegisteredSounds(0);
    }

    // MENU_MUSIC_1 and its seven stems, each of the first six with a shadow sound
    bool CreateVoices()
    {
        ControllerLock lock;
        auto position = ListenerPosition();

        audSoundInitParams params;
        params.volume = Silence;
        params.bucket = *pStreamingBucket;
        params.flags = 0x60;
        rage__audEntity__CreateSound_PersistentReference(entity, "MENU_MUSIC_1", &parent, &params, -1, 0, 0);
        if (!GetRequestedSettings(parent))
            return false;

        for (size_t i = 0; i < VoiceCount; i++)
        {
            auto& voice = voices[i];
            if (i % 2 == 0)
            {
                const char path[]{ char('0' + i / 2), '\0' };
                voice = rage__audController__GetChildSoundReference(*pController, parent, path, entity, &voice);
                if (!voice)
                    return false;
                // Not registered when the stem already has a reference
                if (Field<audSound**>(voice, 0x90) != &voice)
                {
                    voice = nullptr;
                    return false;
                }
                auto settings = GetRequestedSettings(voice);
                if (!settings)
                    return false;
                // Flags at +0xE7: 0x08 for the position, 0x20 for the shadow
                Field<uint8_t>(settings, 0xE7) |= 0x08;
                rage__audRequestedSettings__SetPosition(settings, &position);
                if (i + 1 < VoiceCount)
                {
                    Field<uint8_t>(settings, 0xE7) |= 0x20;
                    rage__audRequestedSettings__SetShadowSoundId(settings, GetSoundId(voice));
                }
            }
            else
            {
                auto source = voices[i - 1];
                char name[32];
                sprintf_s(name, "MENU_MUSIC_SHADOW_SOUND_%u", unsigned(i / 2));
                audSoundInitParams shadow;
                shadow.volume = Silence;
                shadow.shadowSource = source;
                shadow.bucket = Field<uint8_t>(source, 0x40);
                shadow.flags = 0x60;
                rage__audEntity__CreateSound_PersistentReference(entity, name, &voice, &shadow, -1, 0, 0);
                auto settings = GetRequestedSettings(voice);
                if (!settings)
                    return false;
                rage__audSound__PrepareAndPlay(voice, nullptr, false, 0);
                rage__audRequestedSettings__SetPosition(settings, &position);
                // TBoGT keeps the environment reverb on all but the first two shadows
                GetOcclusionMetric(settings).suppressEnvironmentReverb = episode != 2 || i == 1 || i == 3;
            }
        }
        return true;
    }

    // Every sound is alive, still refers back to its pointer and isn't stopping (state 3 at +6, flags 0x18 at +0xE8 of
    // its settings)
    bool VoicesValid()
    {
        ControllerLock lock;
        auto valid = [](audSound*& sound)
        {
            auto settings = GetRequestedSettings(sound);
            return settings && Field<audSound**>(sound, 0x90) == &sound && Field<uint16_t>(sound, 6) != 3 && !(Field<uint8_t>(settings, 0xE8) & 0x18);
        };
        return valid(parent) && std::all_of(voices.begin(), voices.end(), valid);
    }

    // The stems belong to the parent: they are only let go, the parent and the shadows are stopped. The engine clears
    // the pointers when it frees the sounds.
    void StopVoices()
    {
        ControllerLock lock;
        for (size_t i = 0; i < VoiceCount; i++)
        {
            auto settings = GetRequestedSettings(voices[i]);
            if (!settings)
                continue;
            if (i % 2 != 0)
                rage__audSound__StopAndForget(voices[i], false);
            else
            {
                Field<uint8_t>(settings, 0xE8) |= 0x10;
                voices[i] = nullptr;
            }
        }
        if (GetRequestedSettings(parent))
            rage__audSound__StopAndForget(parent, false);
    }

    bool __cdecl OnStreamStop(void*)
    {
        streamStopRequested = true;
        return true;
    }

    bool __cdecl HasStreamStopped(void*)
    {
        return SoundsReleased();
    }

    bool OwnsStream(const audStreamClientSettings& client)
    {
        return client.HasStopped == HasStreamStopped;
    }

    bool StreamReady()
    {
        return stream->state == 1 && OwnsStream(stream->active);
    }

    void FreeStream()
    {
        loopingWaveSlot = nullptr;
        streamStopRequested = true;
        if (OwnsStream(stream->requested))
            stream->requested = {};
        if (StreamReady())
            audStreamSlot__Free(stream);
    }

    // Separate histories keep the 48 kHz delay-time correction out of gameplay's reverb buffers and tables.
    struct MenuReverb
    {
        struct alignas(16) History
        {
            float comb[8][1616]{};
            float allPass[4][556]{};
            float filtered[8]{};
            uint32_t combPosition[8]{}, allPassPosition[4]{};
        };
        std::array<History, 6> history{};
        std::array<uint32_t, 8> combLength{};
        std::array<uint32_t, 4> allPassLength{};
        PhysicalReverb* owner = nullptr;
        uint32_t generation = 0;
        uint8_t channelCount = 0, mask = 0;
        uint32_t quietFrames = 0;

        void Reset(PhysicalReverb* effect, uint32_t session)
        {
            constexpr uint32_t comb[]{ 1116, 1188, 1276, 1356, 1424, 1492, 1556, 1616 };
            constexpr uint32_t allPass[]{ 556, 440, 340, 224 };
            // The native SIMD recurrence advances four samples at a time. Round 44.1/48 kHz to whole vectors.
            for (size_t i = 0; i < combLength.size(); i++)
                combLength[i] = (comb[i] * 147 + 320) / 640 * 4;
            for (size_t i = 0; i < allPassLength.size(); i++)
                allPassLength[i] = (allPass[i] * 147 + 320) / 640 * 4;
            std::memset(history.data(), 0, sizeof(history));
            owner = effect;
            generation = session;
            channelCount = effect->channelCount;
            mask = effect->mask;
            quietFrames = 0;
        }

        static __m128 Dot(__m128 samples, const float* coefficients)
        {
            auto product = _mm_mul_ps(samples, _mm_loadu_ps(coefficients));
            auto first = _mm_add_ss(product, _mm_shuffle_ps(product, product, 0x55));
            auto last = _mm_add_ss(_mm_shuffle_ps(product, product, 0xAA), _mm_shuffle_ps(product, product, 0xFF));
            return _mm_add_ss(first, last);
        }

        void Process(const PhysicalReverb* effect, float* buffer, uint32_t frames)
        {
            constexpr uint8_t speakerMask[]{ 128, 64, 32, 16, 8, 4 };
            const auto& settings = effect->settings[effect->readIndex];
            auto wet = _mm_set1_ps(settings.wet);
            auto dry = _mm_set1_ps(settings.dry);
            auto feedback = _mm_set1_ps(effect->feedback);
            auto previous = _mm_loadu_ps(effect->coefficients[0]);
            auto bias = _mm_set1_ps(1.0e-20f);
            for (size_t channel = 0; channel < effect->channelCount; channel++)
            {
                if (!(effect->mask & speakerMask[channel]))
                    continue;
                auto samples = buffer + channel * frames;
                if (settings.mono && channel)
                {
                    std::memcpy(samples, buffer, frames * sizeof(float));
                    continue;
                }
                auto& state = history[channel];
                for (uint32_t sample = 0; sample < frames; sample += 4)
                {
                    auto input = _mm_loadu_ps(samples + sample);
                    auto gain = _mm_mul_ps(input, _mm_set1_ps(0.015f));
                    auto sum = _mm_setzero_ps();
                    for (size_t i = 0; i < combLength.size(); i++)
                    {
                        auto& position = state.combPosition[i];
                        auto delayed = _mm_load_ps(state.comb[i] + position);
                        sum = _mm_add_ps(sum, delayed);
                        auto a = Dot(delayed, effect->coefficients[1]);
                        auto b = Dot(delayed, effect->coefficients[2]);
                        auto c = Dot(delayed, effect->coefficients[3]);
                        auto d = Dot(delayed, effect->coefficients[4]);
                        auto filtered = _mm_movelh_ps(_mm_unpacklo_ps(a, b), _mm_unpacklo_ps(c, d));
                        auto value = _mm_add_ps(_mm_mul_ps(filtered, feedback), _mm_mul_ps(_mm_set1_ps(state.filtered[i]), previous));
                        _mm_store_ps(state.comb[i] + position, _mm_add_ps(_mm_add_ps(value, gain), bias));
                        state.filtered[i] = _mm_cvtss_f32(d);
                        position += 4;
                        if (position == combLength[i])
                            position = 0;
                    }
                    sum = _mm_add_ps(sum, bias);
                    for (size_t i = 0; i < allPassLength.size(); i++)
                    {
                        auto& position = state.allPassPosition[i];
                        auto delayed = _mm_load_ps(state.allPass[i] + position);
                        auto value = _mm_add_ps(_mm_mul_ps(delayed, _mm_set1_ps(0.5f)), sum);
                        _mm_store_ps(state.allPass[i] + position, _mm_add_ps(value, bias));
                        sum = _mm_sub_ps(delayed, sum);
                        position += 4;
                        if (position == allPassLength[i])
                            position = 0;
                    }
                    _mm_storeu_ps(samples + sample, _mm_add_ps(_mm_add_ps(_mm_mul_ps(sum, wet), _mm_mul_ps(input, dry)), bias));
                }
            }
        }
    };

    void __fastcall ProcessMenuReverb(PhysicalReverb* effect, void* edx, float* buffer, uint32_t frames)
    {
        // Replace rage::audReverbEffectPc::Process for menu input; preserve both histories across transitions.
        static std::array<MenuReverb, 3> reverbs;
        if (!*pMinSpecAudio && *pAudioDriver &&
            Field<uint32_t>(*pAudioDriver, 0x80) == 44100 && buffer && frames && frames <= 512 && !(frames % 4) &&
            effect->channelCount && effect->channelCount <= 6 && effect->readIndex < 3)
        {
            for (size_t i = 0; i < reverbs.size(); i++)
            {
                auto& reverb = reverbs[i];
                auto playing = reverbProfile.load() && sourceReverbs[i].load() == effect;
                if (!playing && reverb.owner != effect)
                    continue;
                if (effect->settings[effect->readIndex].bypass)
                {
                    reverb.owner = nullptr;
                    break;
                }
                auto generation = reverbGeneration.load();
                if (!playing)
                {
                    shReverbProcess.unsafe_fastcall<void>(effect, edx, buffer, frames);
                    if (reverb.channelCount != effect->channelCount || reverb.mask != effect->mask ||
                        (reverb.generation != generation && reverb.generation + 1 != generation))
                    {
                        reverb.owner = nullptr;
                        return;
                    }
                    reverb.generation = generation;
                    // Drain only the menu history; gameplay keeps the native processor and input.
                    std::array<float, 6 * 512> tail{};
                    reverb.Process(effect, tail.data(), frames);
                    float peak = 0.0f;
                    for (size_t sample = 0; sample < frames * effect->channelCount; sample++)
                    {
                        peak = std::max(peak, std::abs(tail[sample]));
                        buffer[sample] += tail[sample];
                    }
                    reverb.quietFrames = peak < 1.0e-6f ? reverb.quietFrames + frames : 0;
                    auto historyLength = reverb.combLength.back();
                    for (auto length : reverb.allPassLength)
                        historyLength += length;
                    // Retire after output stays below -120 dB for the longest comb plus all four allpasses.
                    if (reverb.quietFrames >= historyLength)
                        reverb.owner = nullptr;
                    return;
                }
                if (reverb.owner != effect || reverb.generation != generation ||
                    reverb.channelCount != effect->channelCount || reverb.mask != effect->mask)
                    reverb.Reset(effect, generation);
                reverb.quietFrames = 0;
                reverb.Process(effect, buffer, frames);
                // Finish the gameplay tail without feeding menu input into the native history.
                alignas(16) std::array<float, 6 * 512> tail{};
                shReverbProcess.unsafe_fastcall<void>(effect, edx, tail.data(), frames);
                for (size_t sample = 0; sample < frames * effect->channelCount; sample++)
                    buffer[sample] += tail[sample];
                return;
            }
        }
        else
        {
            for (auto& reverb : reverbs)
                reverb.owner = nullptr;
        }
        shReverbProcess.unsafe_fastcall<void>(effect, edx, buffer, frames);
    }

    // The reverb settings of the console mix. The game sets up the filters of the three source reverbs (small, medium,
    // large) before it commits them every frame, also the values changed here.
    void SetSourceReverb(uint32_t reverb, audEffect* chain)
    {
        auto profile = reverbProfile.load();
        if (!profile && (!delayReset || reverb != 2))
            return;

        // Low-pass and high-pass biquads (type 3), then on the large reverb a delay (type 7)
        auto lowPass = chain->Settings<BiquadSettings>(3);
        auto highPass = lowPass && chain->next ? chain->next->Settings<BiquadSettings>(3) : nullptr;
        if (!highPass)
            return;
        auto delayEffect = chain->next->next;
        auto delay = reverb == 2 && delayEffect ? delayEffect->Settings<DelaySettings>(7) : nullptr;
        if (reverb == 2 && !delay)
            return;

        auto reverbEffect = delay ? delayEffect->next : delayEffect;
        if (profile && reverb < sourceReverbs.size())
            sourceReverbs[reverb] = reverbEffect && *reverbEffect->metadata == 2 ? Field<PhysicalReverb*>(reverbEffect, 200) : nullptr;

        if (profile)
        {
            lowPass->cutoff = 20000.0f;
            lowPass->bypass = false;
        }
        if (delay)
        {
            if (profile)
            {
                highPass->cutoff = profile->largeReverbHighPass;
                highPass->bypass = false;
                delay->feedback.fill(profile->largeDelayFeedback);
            }
            // Consoles leave the delay at 60 ms from the first time the menu music stops
            if (delayReset)
                delay->time.fill(60);
            largeReverbDelay = delayEffect;
        }
    }

    // The game turns the delay times into samples with the mix block size in place of the sample rate. For the menu
    // music's delay they are set from the sample rate, rounded up to whole blocks.
    void SetDelayTimes(audDelayEffect* effect)
    {
        if (largeReverbDelay.load() != &effect->effect || !*pAudioDriver)
            return;

        auto& settings = effect->settings[effect->readIndex];
        auto sampleRate = Field<uint32_t>(*pAudioDriver, 0x80);
        auto block = *pMixBlockSamples;
        if (!sampleRate || !block)
            return;
        std::array<uint32_t, 6> samples;
        for (size_t i = 0; i < samples.size(); i++)
        {
            if (settings.time[i] < 0)
                return;
            auto count = (uint64_t(settings.time[i]) * sampleRate + block * 1000ull - 1) / (block * 1000ull) * block;
            if (count >= effect->physical->capacity)
                return;
            samples[i] = uint32_t(count);
        }
        effect->physical->time = samples;
    }

    void ReleaseReverb()
    {
        if (reverbProfile.exchange(nullptr))
        {
            delayReset = true;
            reverbGeneration.fetch_add(1);
        }
        largeReverbDelay = nullptr;
        for (auto& reverb : sourceReverbs)
            reverb = nullptr;
    }

    // The phone silences the menu music category and on 1.0.7.0-1.1.3.0 voice chat through the speakers lowers it,
    // like the game does with its own menu music; changes made by the game in between are kept
    void UpdateCategoryVolume()
    {
        float attenuation = 0.0f;
        if (*pPhoneActive && !*pPhoneMusicAllowed)
            attenuation = Silence;
        else if (CProfileSettings__GetInt && *pHeadphones == 0 && *pProfileSettings)
        {
            // Voice output volume and voice through speakers
            if (CProfileSettings__GetInt(*pProfileSettings, 102) > 0 && CProfileSettings__GetInt(*pProfileSettings, 100) != 0)
            {
                if (CNetworkPeerMgr__IsAnyRemotePlayerTalking(pNetworkPeerMgr) || CNetworkPeerMgr__IsAnyRemotePlayerAboutToTalk(pNetworkPeerMgr))
                    attenuation = -18.0f;
            }
        }

        auto& volume = Field<float>(category, 0x28);
        if (!categoryVolumeOwned || volume != categoryAppliedVolume)
            categoryBaseVolume = volume;
        categoryAppliedVolume = std::max(Silence, categoryBaseVolume + attenuation);
        volume = categoryAppliedVolume;
        categoryVolumeOwned = true;
    }

    void RestoreCategoryVolume()
    {
        if (categoryVolumeOwned && Field<float>(category, 0x28) == categoryAppliedVolume)
            Field<float>(category, 0x28) = categoryBaseVolume;
        categoryVolumeOwned = false;
    }

    const char* OneShotBank()
    {
        static constexpr std::array<const char*, 3> banks{ "GPS\\MENU_MUSIC_ONESHOTS", nullptr, "EP2_SFX\\EP2_MENUONESHOTS" };
        return banks[episode];
    }

    // audFrontendAudioEntity+0x3FC
    audWaveSlot* OneShotWaveSlot()
    {
        return Field<audWaveSlot*>(entity, 0x3FC);
    }

    // The delay of a one-shot comes from the music's position in its beat: 822 ms in IV, 289 ms in TBoGT
    void PlayOneShot(const char* name, int playTime, const Position& position)
    {
        if (!OneShotBank())
            return;

        audSoundInitParams params;
        params.predelay = episode == 2 ? 289 - playTime % 289 : playTime % 822;
        params.pan = game_rand() % 360;
        params.position = &position;
        audSound* sound = nullptr;
        rage__audEntity__CreateSound_LocalReference(entity, name, &sound, &params, -1, 0, 0);
        if (auto settings = GetRequestedSettings(sound))
        {
            auto& metric = GetOcclusionMetric(settings);
            metric.reverbSize = Random(0.5f, 1.0f);
            metric.reverbWet = Random(0.5f, 0.3f);
            metric.suppressEnvironmentReverb = Random(0.0f, 1.0f) < 0.3f;
            rage__audSound__PrepareAndPlay(sound, OneShotWaveSlot(), false, 0);
        }
    }

    void StartMix()
    {
        channels = {};
        intensity = 0.0f;
        dulcimerElapsed = percussionElapsed = 0.0f;
        dulcimerCounter = 0;
        pendingInput = true;
        percussionWait = Random(25.0f, 45.0f);
        dulcimerWait = Random(75.0f, 35.0f);
    }

    // The first input after the music starts shortens the wait of the first eight voices
    void NotifyInput(bool lobby)
    {
        if (state != State::Playing || !pendingInput)
            return;

        static constexpr std::array<std::array<float, 2>, 8> factors
        { {
            { 0.8f, 1.0f }, { 0.75f, 0.97f }, { 0.6f, 0.8f }, { 0.7f, 0.8f },
            { 0.5f, 0.75f }, { 0.4f, 0.7f }, { 0.3f, 0.6f }, { 0.2f, 0.5f }
        } };
        float scale = lobby ? 1.5f : 1.0f;
        for (size_t i = 0; i < factors.size(); i++)
        {
            auto& channel = channels[i];
            if (channel.phase == Channel::Waiting)
                channel.wait -= Random(factors[i][0] * scale * channel.wait, factors[i][1] * scale * channel.wait);
        }
        pendingInput = false;
    }

    void UpdateMix(float elapsed, bool lobby)
    {
        ControllerLock lock;
        auto& profile = episode == 2 ? BalladMix : OriginalMix;
        auto position = ListenerPosition();
        int playTime = parent ? Field<int>(parent, 0xB8) : 0;
        // One update covers at most 100 ms
        float dt = std::clamp(elapsed, 0.0f, 0.1f);

        reverbProfile = &profile;
        // Only TBoGT unmutes the parent; IV and TLAD use the independent shadow sounds.
        if (auto settings = GetRequestedSettings(parent))
            rage__audRequestedSettings__SetVolume(settings, episode == 2 ? 0.0f : Silence);

        // The intensity goes up and down between 0 and 1 over 100 seconds
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

        // Dulcimer one-shots come in groups of up to three
        dulcimerElapsed += dt;
        if (dulcimerCounter != 0)
        {
            --dulcimerCounter;
            if (dulcimerElapsed > dulcimerWait + 2.0f)
            {
                PlayOneShot("MENU_MUSIC_DULCIMER", playTime, position);
                dulcimerWait = dulcimerCounter != 0 ? Random(0.0f, 1.0f) : Random(10.0f, 35.0f);
                dulcimerElapsed = 0.0f;
            }
        }
        else if (dulcimerElapsed > dulcimerWait + 15.0f)
        {
            dulcimerCounter = 1 + game_rand() % 2;
            PlayOneShot("MENU_MUSIC_DULCIMER", playTime, position);
            dulcimerElapsed = 0.0f;
            dulcimerWait = Random(0.0f, 5.0f);
        }

        // Percussion one-shots need a busy mix
        percussionElapsed += dt;
        if (percussionElapsed >= percussionWait + 15.0f && intensity > 0.5f &&
            std::count_if(channels.begin(), channels.end(), [](const Channel& channel) { return channel.phase == Channel::Playing; }) > 3)
        {
            PlayOneShot("MENU_MUSIC_PERCUSSION", playTime, position);
            percussionWait = Random(0.0f, 15.0f);
            percussionElapsed = 0.0f;
        }

        for (size_t i = 0; i < VoiceCount; i++)
        {
            auto& channel = channels[i];
            float volume = Silence;
            bool moving = false;
            int pan = 0;

            switch (channel.phase)
            {
                case Channel::Reset:
                    channel.panStart = Random(0.0f, 359.0f);
                    channel.panEnd = Random(0.0f, 359.0f);
                    channel.interpolation = 0.0f;
                    channel.speed = Random(0.5f, 2.0f) * profile.speed[i];
                    channel.phase = Channel::Waiting;
                    channel.wait = Random(30.0f, 75.0f) * (profile.intensity[i] * (1.0f - intensity) + profile.wait[i]);
                    if (i == VoiceCount - 1 && episode != 2)
                        lastStemLowPass = Random(700.0f, 8000.0f);
                    break;
                case Channel::Waiting:
                    // Silent until the next update
                    channel.wait -= dt;
                    if (channel.wait <= 0.0f)
                        channel.phase = Channel::Playing;
                    break;
                case Channel::Playing:
                {
                    // The game's FEMUSIC_INTERP_TO_VOL curve: up to full volume at 1, down to 0.35 at 1.5, silent at 2
                    float t = channel.interpolation;
                    float gain = t <= 0.0f ? 0.0f : t <= 1.0f ? t : t <= 1.5f ? 1.0f - (t - 1.0f) * 1.3f : 0.35f - (t - 1.5f) * 0.7f;
                    // dB with the consoles' 0.301 for log10(2)
                    if (gain >= 0.00001f)
                        volume = std::log2(gain) * 0.301f * 20.0f;
                    pan = int((channel.panEnd - channel.panStart) * (t * 0.5f) + channel.panStart);
                    moving = true;
                    channel.interpolation += channel.speed * dt * 0.05f;
                    if (channel.interpolation >= 2.0f)
                        channel.phase = Channel::Reset;
                    break;
                }
            }

            auto settings = GetRequestedSettings(voices[i]);
            if (!settings)
                continue;
            rage__audRequestedSettings__SetVolume(settings, volume);
            if (moving)
            {
                rage__audRequestedSettings__SetPan(settings, pan);
                rage__audRequestedSettings__SetPosition(settings, &position);
            }
            GetOcclusionMetric(settings).reverbSize = profile.reverbSize;
            GetOcclusionMetric(settings).reverbWet = profile.reverbWet;
            // The last stem has its own low-pass, a new one for each time it plays in IV and TLAD
            if (i == VoiceCount - 1)
                rage__audRequestedSettings__SetLowPassFilterCutoff(settings, episode == 2 ? 23900 : uint32_t(lastStemLowPass));
        }

        // In the lobby and in TBoGT the waiting voices come in sooner without an input
        if (lobby || episode == 2)
            NotifyInput(lobby);
    }

    void BeginStop(bool failure)
    {
        if (state == State::Stopping)
            return;
        failed = failure;
        state = State::Stopping;
        StopVoices();
    }

    void Stop()
    {
        if (state == State::Failed)
            state = State::Idle;
        else if (state != State::Idle)
            BeginStop(false);
    }

    void UpdateSession(bool shouldPlay, bool lobby, float elapsed, bool advance)
    {
        if (state == State::Stopping)
        {
            // Wait for the engine to free the sounds, then give the stream slot back
            if (parent || std::any_of(voices.begin(), voices.end(), [](audSound* sound) { return sound != nullptr; }))
                return;
            if (stream)
                FreeStream();
            if (!SoundsReleased() || (stream && (OwnsStream(stream->active) || OwnsStream(stream->requested))))
                return;
            stream = nullptr;
            if (effectsApplied)
                ReleaseReverb();
            effectsApplied = created = false;
            // A failed start is only tried again after the menu has closed
            state = failed && shouldPlay ? State::Failed : State::Idle;
            return;
        }

        if (!shouldPlay || (state != State::Idle && episode != *_dwCurrentEpisode))
            return Stop();

        // Another stream took the slot
        if (stream && (streamStopRequested || (!StreamReady() && !OwnsStream(stream->requested))))
            return BeginStop(false);

        switch (state)
        {
            case State::Idle:
                episode = *_dwCurrentEpisode;
                state = State::WaitingForStream;
                break;
            case State::WaitingForStream:
                if (!stream && SoundsReleased())
                {
                    streamStopRequested = false;
                    audStreamClientSettings client;
                    client.OnStop = OnStreamStop;
                    client.HasStopped = HasStreamStopped;
                    client.priority = 5; // the priority of the game's menu music
                    stream = audStreamSlot__AllocateSlot(&client);
                }
                if (stream && StreamReady())
                    state = State::Preparing;
                break;
            case State::Preparing:
            {
                if (!created)
                {
                    if (!CreateVoices())
                        return BeginStop(true);
                    created = true;
                }
                if (!VoicesValid() || !StreamReady())
                    return BeginStop(true);

                loopingWaveSlot = stream->waveSlot;
                int prepared;
                {
                    ControllerLock lock;
                    prepared = rage__audSound__Prepare(parent, stream->waveSlot, true);
                }
                if (prepared == 2) // failed
                    BeginStop(true);
                if (prepared != 1) // not ready yet
                    break;

                if (auto bank = OneShotBank())
                {
                    // 0 = loaded, 1 = loading, 2 = failed, 3 = not requested
                    auto id = rage__audWaveSlot__FindBankId(bank);
                    int status = OneShotWaveSlot() && id != -1 ? rage__audWaveSlot__GetBankLoadingStatus(OneShotWaveSlot(), id) : 2;
                    if (status == 3)
                        rage__audWaveSlot__LoadBank(OneShotWaveSlot(), id);
                    if (status == 2)
                        BeginStop(true);
                    if (status != 0)
                        break;
                }

                {
                    ControllerLock lock;
                    rage__audSound__Play(parent);
                }
                StartMix();
                state = State::Playing;
                break;
            }
            case State::Playing:
                if (!VoicesValid())
                    return BeginStop(true);
                if (advance)
                {
                    effectsApplied = true;
                    UpdateMix(elapsed, lobby);
                }
                break;
            default:
                break;
        }
    }

    // Replaces the menu music states of audFrontendAudioEntity::UpdateMusic; 'time' is the function's millisecond clock
    void UpdateMusic(SafetyHookContext& regs, uint32_t time)
    {
        entity = reinterpret_cast<void*>(regs.esi);
        engineStopped = false;
        if (!category)
            category = rage__audCategoryManager__GetCategoryPtr(pCategoryManager, 0xB65AE241); // FRONTEND_MENU_MUSIC

        bool lobby = CNetwork__IsOnLobbyScreen();
        // +0x43D: the game is ready for menu music; the game asks for a stop with its state 4
        bool shouldPlay = Field<bool>(entity, 0x43D) && audFrontendAudioEntity__ShouldFEMusicBePlaying(entity);
        bool stopRequested = Field<int>(entity, 0x200) == 4 && state != State::Stopping;
        UpdateCategoryVolume();

        // Nothing moves in a repeated update or when the clock steps back
        uint32_t delta = time - lastTime;
        bool advance = delta != 0 && delta < 0x80000000;
        lastTime = time;
        if (stopRequested)
            Stop();
        UpdateSession(shouldPlay && !stopRequested, lobby, advance ? delta * 0.001f : 0.0f, advance);

        Field<int>(entity, 0x200) = state == State::Failed ? 0 : int(state);
        if (state == State::WaitingForStream || state == State::Preparing)
        {
            // Like the game in these states: stop the sounds at +0x34C and +0x228 (with its state at +0x234)
            if (auto sound = Field<audSound*>(entity, 0x34C))
                rage__audSound__StopAndForget(sound, false);
            if (auto sound = Field<audSound*>(entity, 0x228))
            {
                rage__audSound__StopAndForget(sound, false);
                Field<int>(entity, 0x234) = 0;
            }
        }
        regs.eip = updateMusicEnd;
    }

    void __fastcall NotifyFrontendInput(void* _this, void* edx)
    {
        NotifyInput(CNetwork__IsOnLobbyScreen());
    }

    int __cdecl ShutdownClass()
    {
        if (!engineStopped)
        {
            Stop();
            UpdateSession(false, false, 0.0f, false);
            ReleaseReverb();
            RestoreCategoryVolume();
        }
        auto result = shShutdownClass.unsafe_ccall<int>();

        // A menu stop advances by one to drain its tail; advancing by two rejects histories from the old engine.
        reverbGeneration.fetch_add(2);

        // The engine is gone with its sounds, stream slots and categories. It starts again when the episode changes.
        engineStopped = true;
        loopingWaveSlot = nullptr;
        state = State::Idle;
        failed = created = effectsApplied = categoryVolumeOwned = false;
        parent = nullptr;
        voices = {};
        stream = nullptr;
        category = nullptr;
        return result;
    }

    // A looping stream voice that runs out of data inside a block is ended by the game, which leaves a gap at the menu
    // music's loop point. The short block is remembered before the refill and completed from the next packet as soon as
    // that is submitted, which keeps the voice playing. One for ADPCM voices (rage::audVoicePcAdpcm) and one for PCM
    // voices (rage::audVoiceSoft); both have two 64-byte packets at +0x90 (data at +0, read offset at +0x38, bytes left
    // at +0x3C), the packet being read at +0x11C and a two-block buffer.
    template<bool Pcm>
    class LoopingStreamRefill
    {
        static constexpr uint32_t BlockSize = Pcm ? 0x10000 : 0x4000; // input bytes
        static constexpr uint32_t OutputScale = Pcm ? 1 : 4;
        static constexpr size_t OutputOffset = Pcm ? 0x130 : 0x140;
        static constexpr size_t BufferOffset = Pcm ? 0x138 : 0x148;
        static constexpr size_t EndedOffset = Pcm ? 0x13C : 0x14C;

        struct ShortBlock
        {
            void* voice;
            void* waveSlot;
            uint8_t* buffer;
            uint8_t* destination;
            uint32_t remaining, half, refills, time, lifetime;
            void* output;
        };
        std::array<ShortBlock, 256> blocks{};

        // By voice index
        ShortBlock* Find(void* voice)
        {
            auto index = Field<uint32_t>(voice, 0x88);
            return index < blocks.size() ? &blocks[index] : nullptr;
        }

        static bool IsMenuMusic(void* voice)
        {
            auto waveSlot = loopingWaveSlot.load();
            return waveSlot && waveSlot == Field<void*>(voice, 0x10);
        }

    public:
        void BeforeRefill(void* voice, bool initial)
        {
            auto found = Find(voice);
            if (!found)
                return;
            auto& block = *found;
            block = {};
            // A looping voice (flag 0x10 at +0x8C) that hasn't ended, with both packets queued (+0x120)
            if (initial || !IsMenuMusic(voice) || Field<uint8_t>(voice, EndedOffset) || !(Field<uint8_t>(voice, 0x8C) & 0x10) || Field<uint32_t>(voice, 0x120) != 2)
                return;

            auto half = Field<uint32_t>(voice, 0x110); // the block of the buffer to fill
            auto read = Field<uint32_t>(voice, 0x11C);
            auto sampleRate = Field<uint32_t>(voice, 0xC);
            auto buffer = Field<uint8_t*>(voice, BufferOffset);
            auto output = Field<void*>(voice, OutputOffset);
            if (half > 1 || read > 1 || !sampleRate || !buffer || !output)
                return;

            auto left = [&](uint32_t packet) { return Field<uint32_t>(voice, 0xCC + packet * 0x40); };
            auto first = std::min(BlockSize, left(read));
            auto next = first && first == left(read) ? read ^ 1 : read;
            auto count = first + std::min(BlockSize - first, left(next));
            if (count == BlockSize)
                return;

            // Valid for the refill that follows (counter at +0x124) and as long as a block plays
            block = { voice, Field<void*>(voice, 0x10), buffer, buffer + half * 0x10000 + count * OutputScale, BlockSize - count, half,
                Field<uint32_t>(voice, 0x124) + 1, GetTickCount(), 32768000u / sampleRate, output };
        }

        void AfterAddSampleData(void* voice)
        {
            auto found = Find(voice);
            if (!found || !found->remaining)
                return;
            auto& block = *found;

            auto output = Field<void*>(voice, OutputOffset);
            // The same voice and buffer, ended by the short refill, and the output still plays the other block (its
            // position at +0xC)
            bool valid = IsMenuMusic(voice) && block.voice == voice && block.waveSlot == Field<void*>(voice, 0x10) &&
                block.buffer == Field<uint8_t*>(voice, BufferOffset) && Field<uint8_t>(voice, EndedOffset) &&
                Field<uint32_t>(voice, 0x124) == block.refills && GetTickCount() - block.time < block.lifetime &&
                output && output == block.output && (Field<uint32_t>(output, 0xC) >= 0x10000) != block.half;
            // PCM: the voice is waiting to stop (+0x114) and its output still plays (+0x24, not stopped at +0x61)
            if (Pcm && valid)
                valid = Field<uint32_t>(voice, 0x114) == 1 && Field<uint32_t>(output, 0x24) && !Field<uint8_t>(output, 0x61);
            if (!valid)
            {
                block = {};
                return;
            }

            // A short last packet can leave both packets empty before the next one arrives
            for (int i = 0; i < 2 && block.remaining; i++)
            {
                auto& read = Field<uint32_t>(voice, 0x11C);
                if (read > 1)
                    break;
                auto packet = static_cast<uint8_t*>(voice) + 0x90 + read * 0x40;
                auto& offset = Field<uint32_t>(packet, 0x38);
                auto& left = Field<uint32_t>(packet, 0x3C);
                auto count = std::min(block.remaining, left);
                if (!count)
                    break;
                auto input = Field<const uint8_t*>(packet, 0) + offset;
                if constexpr (Pcm)
                    std::memcpy(block.destination, input, count);
                else
                    rage__IMA_ADPCM__Decode(static_cast<uint8_t*>(voice) + 0x14E, block.destination, input, 0, count);
                offset += count;
                left -= count;
                block.destination += count * OutputScale;
                block.remaining -= count;
                if (!left)
                    read ^= 1;
            }

            if (!block.remaining)
            {
                Field<uint8_t>(voice, EndedOffset) = 0;
                Field<uint32_t>(voice, 0x114) = 0;
                block = {};
            }
        }
    };

    LoopingStreamRefill<false> adpcmRefill;
    LoopingStreamRefill<true> pcmRefill;

    uint32_t __fastcall RefillAdpcmBuffer(void* voice, void* edx, bool initial)
    {
        adpcmRefill.BeforeRefill(voice, initial);
        return shRefillAdpcmBuffer.unsafe_fastcall<uint32_t>(voice, edx, initial);
    }

    uint32_t __fastcall AddAdpcmSampleData(void* voice, void* edx, uint32_t offset)
    {
        auto result = shAddAdpcmSampleData.unsafe_fastcall<uint32_t>(voice, edx, offset);
        adpcmRefill.AfterAddSampleData(voice);
        return result;
    }

    uint32_t __fastcall RefillPcmBuffer(void* voice, void* edx, bool initial)
    {
        pcmRefill.BeforeRefill(voice, initial);
        return shRefillPcmBuffer.unsafe_fastcall<uint32_t>(voice, edx, initial);
    }

    uint32_t __fastcall AddPcmSampleData(void* voice, void* edx, uint32_t offset)
    {
        auto result = shAddPcmSampleData.unsafe_fastcall<uint32_t>(voice, edx, offset);
        pcmRefill.AfterAddSampleData(voice);
        return result;
    }
}

class MenuMusicModule
{
public:
    MenuMusicModule()
    {
        FusionFix::onInitEvent() += []()
        {
            using namespace MenuMusic;

            // audFrontendAudioEntity::UpdateMusic: the switch over the menu music states, its first case and its end
            auto pattern = hook::pattern("8B 86 00 02 00 00 83 F8 04 0F 87 ? ? ? ? FF 24 85");
            auto updateMusic = pattern.get_first(0);
            updateMusicEnd = uintptr_t(pattern.get_first(15)) + *pattern.get_first<int32_t>(11);
            audFrontendAudioEntity__ShouldFEMusicBePlaying = (decltype(audFrontendAudioEntity__ShouldFEMusicBePlaying))injector::GetBranchDestination(pattern.get_first(24)).as_int();

            // audFrontendAudioEntity::NotifyFrontendInput, called after CNetwork::IsOnLobbyScreen
            pattern = hook::pattern("C7 86 00 02 00 00 04 00 00 00 E8 ? ? ? ? 84 C0 74 07 8B CE E8");
            CNetwork__IsOnLobbyScreen = (decltype(CNetwork__IsOnLobbyScreen))injector::GetBranchDestination(pattern.get_first(10)).as_int();
            shNotifyFrontendInput = safetyhook::create_inline(injector::GetBranchDestination(pattern.get_first(21)).as_int(), NotifyFrontendInput);

            // In audFrontendAudioEntity::ShouldFEMusicBePlaying
            pattern = hook::pattern("38 05 ? ? ? ? 74 08 38 05 ? ? ? ? 74");
            pPhoneActive = *pattern.get_first<bool*>(2);
            pPhoneMusicAllowed = *pattern.get_first<bool*>(10);

            pattern = find_pattern("E8 ? ? ? ? C7 86 18 02 00 00 00 00 00 00 C7 86 00 02 00 00 00 00 00 00", "E8 ? ? ? ? 89 9E 18 02 00 00 89 9E 00 02 00 00");
            audStreamSlot__Free = (decltype(audStreamSlot__Free))injector::GetBranchDestination(pattern.get_first(0)).as_int();

            pattern = find_pattern("53 8B 5C 24 08 80 7B 0C 06 75", "53 55 8B 6C 24 0C 8A 45 0C B2");
            audStreamSlot__AllocateSlot = (decltype(audStreamSlot__AllocateSlot))pattern.get_first(0);

            pattern = find_pattern("C7 44 24 24 00 00 00 00 E8 ? ? ? ? 8B 4C 24 0C 6A 00 6A 00 85 C9", "C7 44 24 24 00 00 00 00 E8 ? ? ? ? 8B 4C 24 0C 85 C9 6A 00 6A 00");
            rage__audEntity__CreateSound_LocalReference = (decltype(rage__audEntity__CreateSound_LocalReference))injector::GetBranchDestination(pattern.get_first(8)).as_int();

            pattern = find_pattern("83 EC 10 53 55 56 57 8B F9 8D 4C 24 18 8D 9F 10 32 00 00", "83 EC 08 56 57 8B F9 8D 87 10 32 00 00 50 8D 4C 24 0C E8 ? ? ? ? 8B 74");
            rage__audController__GetChildSoundReference = (decltype(rage__audController__GetChildSoundReference))pattern.get_first(0);

            pattern = hook::pattern("56 8B F1 F6 46 39 01 74 09 B8");
            rage__audSound__Prepare = (decltype(rage__audSound__Prepare))pattern.get_first(0);

            pattern = find_pattern("0F B6 41 04 56 3D FF 00 00 00 74 21 0F B6 51 40 8B 35 ? ? ? ? 69 D2 40 6F 00 00 0F AF F0 A1 ? ? ? ? 03 B4 02 14 6F 00 00 EB 02 33 F6 8B 49 74 85 C9 74 0F",
                "0F B6 41 04 3D FF 00 00 00 56 74 21 0F B6 51 40 8B 35 ? ? ? ? 69 D2 40 6F 00 00 0F AF F0 A1 ? ? ? ? 03 B4 02 14 6F 00 00 EB 02 33 F6 83");
            rage__audSound__Play = (decltype(rage__audSound__Play))pattern.get_first(0);
            pRequestedSettingsStride = *pattern.get_first<uint32_t*>(18);
            pSoundBuckets = *pattern.get_first<uint8_t**>(32);

            pattern = find_pattern("56 8B F1 57 F6 46 39 01 75 4D", "57 8B F9 F6 47 39 01 75 05 E8");
            rage__audSound__PrepareAndPlay = (decltype(rage__audSound__PrepareAndPlay))pattern.get_first(0);

            pattern = find_pattern("53 56 8B F1 0F B6 46 04 3D FF", "8B D1 0F B6 42 04 3D FF 00 00");
            rage__audSound__StopAndForget = (decltype(rage__audSound__StopAndForget))pattern.get_first(0);

            pattern = find_pattern("0F B6 81 E7 00 00 00 F3 0F 10 44 24 04 83 E0 07 8B 04 C5 ? ? ? ? 83", "0F B6 81 E7 00 00 00 F3 0F 10 44 24 04 83 E0 07 8B 14 C5 ? ? ? ? 83");
            rage__audRequestedSettings__SetVolume = (decltype(rage__audRequestedSettings__SetVolume))pattern.get_first(0);

            pattern = hook::pattern("0F B6 81 E7 00 00 00 83 E0 07 8B 14 C5 ? ? ? ? 66 8B 44 24 04 C1 E2 05 66 89 44 0A 52");
            rage__audRequestedSettings__SetPan = (decltype(rage__audRequestedSettings__SetPan))pattern.get_first(0);

            pattern = find_pattern("0F B6 81 E7 00 00 00 83 E0 07 8B 14 C5 ? ? ? ? 42", "0F B6 81 E7 00 00 00 83 E0 07 8B 04 C5 ? ? ? ? 83 C0 01 C1 E0 04 03");
            rage__audRequestedSettings__SetPosition = (decltype(rage__audRequestedSettings__SetPosition))pattern.get_first(0);

            pattern = hook::pattern("0F B6 81 E7 00 00 00 83 E0 07 8B 14 C5 ? ? ? ? 66 8B 44 24 04 C1 E2 05 66 89 44 0A 58");
            rage__audRequestedSettings__SetLowPassFilterCutoff = (decltype(rage__audRequestedSettings__SetLowPassFilterCutoff))pattern.get_first(0);

            pattern = hook::pattern("8A 44 24 04 88 81 E6 00 00 00");
            rage__audRequestedSettings__SetShadowSoundId = (decltype(rage__audRequestedSettings__SetShadowSoundId))pattern.get_first(0);

            pattern = find_pattern("FF 74 24 04 B9 ? ? ? ? E8 ? ? ? ? C3 CC 53 56", "8B 44 24 04 50 B9 ? ? ? ? E8 ? ? ? ? C3 83");
            rage__audWaveSlot__FindBankId = (decltype(rage__audWaveSlot__FindBankId))pattern.get_first(0);

            pattern = find_pattern("56 8B F1 57 FF 76 38 BF 03 00 00 00 E8 ? ? ? ? 66", "56 8B F1 8B 46 38 57 50 BF 03 00 00 00 E8 ? ? ? ? 66");
            rage__audWaveSlot__GetBankLoadingStatus = (decltype(rage__audWaveSlot__GetBankLoadingStatus))pattern.get_first(0);

            pattern = find_pattern("56 57 66 8B 7C 24 0C B8 FF FF 00 00 8B F1 66 3B F8 74 37 FF 76 38 E8 ? ? ? ? 83 C4 04 83 7E 08 00 77 1B C6",
                "56 57 66 8B 7C 24 0C 66 81 FF FF FF 8B F1 74 4A 8B 46 38 50 E8 ? ? ? ? 83 C4 04 83 7E 08 00 76 11 8B 4E 38 51 E8 ? ? ? ? 83 C4 04 5F 5E C2 04");
            rage__audWaveSlot__LoadBank = (decltype(rage__audWaveSlot__LoadBank))pattern.get_first(0);

            pattern = hook::pattern("A1 ? ? ? ? 50 B9 ? ? ? ? E8 ? ? ? ? 85 C0 74 ? 8B C8 E8");
            pCategoryManager = *pattern.get_first<void*>(7);
            rage__audCategoryManager__GetCategoryPtr = (decltype(rage__audCategoryManager__GetCategoryPtr))injector::GetBranchDestination(pattern.get_first(11)).as_int();

            pattern = find_pattern("8B 0D ? ? ? ? E8 ? ? ? ? 6A 21 E8", "8B 0D ? ? ? ? 50 E8 ? ? ? ? 6A 21 E8");
            pController = *pattern.get_first<void**>(2);

            pattern = hook::pattern("8B 0D ? ? ? ? 8B F8 E8 ? ? ? ? 3B F8 73 04 33 FF");
            pAudioDriver = *pattern.get_first<void**>(2);

            // PC delay parameter setter: mix-block size used for time conversion.
            pattern = find_pattern("8B 35 ? ? ? ? B8 D3 4D 62 10 F7 E6 C1 EA 06", "8B 0D ? ? ? ? B8 D3 4D 62 10 F7 E1 C1 EA 06");
            pMixBlockSamples = *pattern.get_first<uint32_t*>(2);

            // audGtaAudioEngine::ShutdownClass
            pattern = find_pattern("80 3D ? ? ? ? 00 0F 84 ? ? ? ? A1 ? ? ? ? C6", "53 33 DB 38 1D ? ? ? ? 0F 84 ? ? ? ? A1");
            shShutdownClass = safetyhook::create_inline(pattern.get_first(0), ShutdownClass);

            pattern = find_pattern("83 EC 08 80 7C 24 0C 00 53 55 56 57 8B", "51 80 7C 24 08 00 53 55 56 57 8B F1 74 15 BB 00 80");
            shRefillAdpcmBuffer = safetyhook::create_inline(pattern.get_first(0), RefillAdpcmBuffer);

            // rage::audVoicePcAdpcm::AddSampleData
            pattern = find_pattern("53 8B D9 56 8B 83 18 01 00 00 8B 73 14 57 C1 E0 06 8D BB 90 00 00 00 03 F8 B9 0E 00 00 00 F3 A5 8B 83", "53 8B D9 8B 83 18 01 00 00 56 8B 73 14 57");
            shAddAdpcmSampleData = safetyhook::create_inline(pattern.get_first(0), AddAdpcmSampleData);

            pattern = find_pattern("8B 54 24 04 53 8B 5C 24 0C 55 8B E9 8B 4C 24 14", "51 8B 54 24 14 53 8B 5C 24 14");
            rage__IMA_ADPCM__Decode = (decltype(rage__IMA_ADPCM__Decode))pattern.get_first(0);

            pattern = find_pattern("51 80 7C 24 08 00 53 55 56 57 8B F1 74 11 C7 86 10 01 00 00", "51 80 7C 24 08 00 53 55 56 57 8B F1 74 15 BB 00 00");
            shRefillPcmBuffer = safetyhook::create_inline(pattern.get_first(0), RefillPcmBuffer);

            // rage::audVoiceSoft::AddSampleData
            pattern = find_pattern("83 EC 08 53 8B D9 56 8B 83 18", "53 8B D9 8B 83 18 01 00 00 56 8B 73 14 C1");
            shAddPcmSampleData = safetyhook::create_inline(pattern.get_first(0), AddPcmSampleData);

            // The game's parsed -minspecaudio flag, from launch arguments or commandline.txt.
            pattern = find_pattern("83 3D ? ? ? ? 00 A2 ? ? ? ? 74 ? C6 05 ? ? ? ? 00 C6 05 ? ? ? ? 00 88 0D",
                "39 1D ? ? ? ? 74 ? 88 1D ? ? ? ? 88 1D ? ? ? ? C6 05 ? ? ? ? 01 F3 0F 11 0D");
            pMinSpecAudio = *pattern.get_first<const char**>(2);

            // rage::audReverbEffectPc::Process, the PC counterpart of rage::audReverbEffectXenon::Process.
            pattern = find_pattern("55 8B EC 83 E4 F0 81 EC 48 09 00 00 56 8B F1 57 8B 86 B4 00 00 00",
                "55 8B EC 83 E4 F0 81 EC A4 09 00 00 53 56 8B F1 8B 86 B4 00 00 00");
            shReverbProcess = safetyhook::create_inline(pattern.get_first(0), ProcessMenuReverb);

            // audGtaEnvironment::Update: low-spec audio bypasses two reverbs and collapses their spatial sends.
            // The engine refreshes this mode each update, so its normal setting resumes when the music releases it.
            pattern = find_pattern("83 EC 4C 53 55 8B D9 E8 ? ? ? ? 8B CB E8 ? ? ? ? 8B CB E8 ? ? ? ? 8B CB E8",
                "83 EC 50 53 55 8B E9 E8 ? ? ? ? 8B CD E8 ? ? ? ? 8B CD E8 ? ? ? ? 8B CD E8");
            static auto GtaEnvironmentUpdateHook = safetyhook::create_mid(pattern.get_first(0), [](SafetyHookContext& regs)
            {
                if (reverbProfile.load() && !*pMinSpecAudio)
                    Field<bool>(reinterpret_cast<void*>(regs.ecx), 0x3210) = false;
            });

            // audGtaEnvironment::SetUpSourceReverbFilters, before a reverb's filters are committed
            pattern = hook::pattern("8B 07 8B CF FF 50 14 8B 6C 24 14 46");
            if (!pattern.empty()) // 1.2.0.32, 1.2.0.43, 1.2.0.59
            {
                static auto SetUpSourceReverbFiltersHook = safetyhook::create_mid(pattern.get_first(0), [](SafetyHookContext& regs)
                {
                    SetSourceReverb(regs.esi, reinterpret_cast<audEffect*>(regs.edi));
                });

                static auto UpdateMusicHook = safetyhook::create_mid(updateMusic, [](SafetyHookContext& regs)
                {
                    UpdateMusic(regs, regs.ebx);
                });

                // rage::audDelayEffect::Update, after the delay times are set
                pattern = hook::pattern("8B 4E 74 8D 04 C0 8D 40 0F 8D 04 C6 50 E8 ? ? ? ? 8B CE 5E E9");
                static auto DelayEffectUpdateHook = safetyhook::create_mid(pattern.get_first(18), [](SafetyHookContext& regs)
                {
                    SetDelayTimes(reinterpret_cast<audDelayEffect*>(regs.esi));
                });

                // The game's menu music sound
                pattern = hook::pattern("FF 35 ? ? ? ? 8D 44 24 68 50 E8 ? ? ? ? 83 C4 08 8D 44 24 20 6A 00 6A 00 6A FF 50 57 68 ? ? ? ? 8B CE E8");
                pStreamingBucket = *pattern.get_first<uint8_t*>(2);
                rage__audEntity__CreateSound_PersistentReference = (decltype(rage__audEntity__CreateSound_PersistentReference))injector::GetBranchDestination(pattern.get_first(38)).as_int();

                // audGtaAudioEngine::RegisterLoudSound
                pattern = hook::pattern("8B 0D ? ? ? ? 64 A1 2C 00 00 00 8B 04 88 8B 88 ? ? ? ? 8B 44 24 04 C1 E1 06 F3 0F 10 40 04 F3 0F 10 10 F3 0F 10 48 08 81 C1");
                pTlsIndex = *pattern.get_first<uint32_t*>(2);
                listenerIndexOffset = *pattern.get_first<uint32_t>(17);
                pListenerPositions = *pattern.get_first<float*>(44);

                pattern = hook::pattern("0F B6 56 40 8B 3D ? ? ? ? 69 D2 40 6F 00 00 0F AF F8 A1");
                pSoundStride = *pattern.get_first<uint32_t*>(6);
            }
            else // 1.0.7.0, 1.0.8.0, 1.1.2.0, 1.1.3.0
            {
                pattern = hook::pattern("8B 55 00 8B 42 14 8B CD FF D0 83 44 24 14 04");
                static auto SetUpSourceReverbFiltersHook = safetyhook::create_mid(pattern.get_first(0), [](SafetyHookContext& regs)
                {
                    SetSourceReverb(regs.ebx, reinterpret_cast<audEffect*>(regs.ebp));
                });

                static auto UpdateMusicHook = safetyhook::create_mid(updateMusic, [](SafetyHookContext& regs)
                {
                    UpdateMusic(regs, *reinterpret_cast<uint32_t*>(regs.esp + 0x10));
                });

                // rage::audDelayEffect::Update, after the delay times are set
                pattern = hook::pattern("8D 4C C6 78 51 8B 4E 74 E8 ? ? ? ? 8B CE 5E E9");
                static auto DelayEffectUpdateHook = safetyhook::create_mid(pattern.get_first(13), [](SafetyHookContext& regs)
                {
                    SetDelayTimes(reinterpret_cast<audDelayEffect*>(regs.esi));
                });

                // The game's menu music sound
                pattern = hook::pattern("A1 ? ? ? ? 50 8D 8C 24 80 00 00 00 51 E8 ? ? ? ? 83 C4 08 53 53 6A FF 8D 54 24 44 52 57 68 ? ? ? ? 8B CE E8");
                pStreamingBucket = *pattern.get_first<uint8_t*>(1);
                rage__audEntity__CreateSound_PersistentReference = (decltype(rage__audEntity__CreateSound_PersistentReference))injector::GetBranchDestination(pattern.get_first(39)).as_int();

                // audGtaAudioEngine::RegisterLoudSound
                pattern = hook::pattern("A1 ? ? ? ? 64 8B 0D 2C 00 00 00 8B 14 81 8B 82 ? ? ? ? 8B 4D 08 F3 0F 10 51 08 F3 0F 10 01 F3 0F 10 49 04 C1 E0 06 05");
                pTlsIndex = *pattern.get_first<uint32_t*>(1);
                listenerIndexOffset = *pattern.get_first<uint32_t>(17);
                pListenerPositions = *pattern.get_first<float*>(42);

                pattern = hook::pattern("51 A1 ? ? ? ? 53 55 8B 6C 24 10 56 57 8B F9 8D 5F 48");
                pSoundStride = *pattern.get_first<uint32_t*>(2);

                // The game's voice chat check for its menu music
                pattern = hook::pattern("39 1D ? ? ? ? 75 ? 8B 0D ? ? ? ? 6A 66 E8 ? ? ? ? 85 C0 7E ? 8B 0D ? ? ? ? 6A 64 E8 ? ? ? ? 85 C0 74 ? B9 ? ? ? ? E8 ? ? ? ? 84 C0 75 ? B9 ? ? ? ? E8");
                pHeadphones = *pattern.get_first<int*>(2);
                pProfileSettings = *pattern.get_first<void**>(10);
                CProfileSettings__GetInt = (decltype(CProfileSettings__GetInt))injector::GetBranchDestination(pattern.get_first(16)).as_int();
                pNetworkPeerMgr = *pattern.get_first<void*>(43);
                CNetworkPeerMgr__IsAnyRemotePlayerTalking = (decltype(CNetworkPeerMgr__IsAnyRemotePlayerTalking))injector::GetBranchDestination(pattern.get_first(47)).as_int();
                CNetworkPeerMgr__IsAnyRemotePlayerAboutToTalk = (decltype(CNetworkPeerMgr__IsAnyRemotePlayerAboutToTalk))injector::GetBranchDestination(pattern.get_first(61)).as_int();
            }
        };
    }
} MenuMusicModule;
