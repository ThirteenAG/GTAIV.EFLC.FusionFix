#include "Native.hpp"

#include <algorithm>
#include <cstdio>
#include <windows.h>

namespace MenuMusic
{
    namespace
    {
        template<typename T> T& Field(void* object, std::size_t offset)
        {
            return *reinterpret_cast<T*>(static_cast<std::byte*>(object) + offset);
        }

        class ControllerLock
        {
        public:
            explicit ControllerLock(audController* controller)
                : section(&Field<CRITICAL_SECTION>(controller, 12816)) { EnterCriticalSection(section); }
            ~ControllerLock() { LeaveCriticalSection(section); }
            ControllerLock(const ControllerLock&) = delete;
            ControllerLock& operator=(const ControllerLock&) = delete;

        private:
            CRITICAL_SECTION* section;
        };

        struct OcclusionMetric
        {
            float occlusion, filterCutoff, reserved08, reverbWet, reverbSize;
            std::byte reserved20[16];
            bool suppressEnvironmentReverb;
        };

        OcclusionMetric& GetOcclusionMetric(audRequestedSettings* settings)
        {
            return Field<OcclusionMetric>(settings, 160);
        }
    }

    bool NativeBindings::Complete() const
    {
        return CreateSound_PersistentReference_Name && CreateSound_LocalReference_Name && GetChildSoundReference &&
            Prepare && Play && PrepareAndPlay && StopAndForget && SetVolume && SetPan && SetPosition &&
            SetLowPassFilterCutoff && SetShadowSoundId && FindBankId && GetBankLoadingStatus && LoadBank &&
            GetRandomNumberInRange_Float && GetRandomNumberInRange_Int && controller && soundBuckets &&
            soundStride >= 188 && requestedSettingsStride >= 233 && streamingBucket != 0xFF;
    }

    float NativeRandom::Range(float first, float last)
    {
        return bindings.GetRandomNumberInRange_Float(first, last);
    }

    int NativeRandom::Integer(int first, int exclusiveLast)
    {
        return exclusiveLast <= first ? first : bindings.GetRandomNumberInRange_Int(first, exclusiveLast - 1);
    }

    audRequestedSettings* NativeAudio::GetRequestedSettings(Sound* sound) const
    {
        if (!sound || Field<std::uint8_t>(sound, 4) == 0xFF)
            return nullptr;
        auto* bucket = bindings.soundBuckets + 28480 * Field<std::uint8_t>(sound, 64);
        auto* base = Field<std::byte*>(bucket, 28436);
        return base ? reinterpret_cast<audRequestedSettings*>(base + bindings.requestedSettingsStride * Field<std::uint8_t>(sound, 4)) : nullptr;
    }

    std::uint8_t NativeAudio::GetSoundId(Sound* sound) const
    {
        auto* bucket = bindings.soundBuckets + 28480 * Field<std::uint8_t>(sound, 64);
        auto* base = Field<std::byte*>(bucket, 28432);
        return static_cast<std::uint8_t>((reinterpret_cast<std::byte*>(sound) - base) / bindings.soundStride);
    }

    bool NativeAudio::Register(Sound*& output)
    {
        if (output || referenceCount == references.size() ||
            std::find(references.begin(), references.end(), &output) != references.end())
            return false;
        const auto id = Field<std::uint16_t>(entity, 4);
        if (id >= 800 || (entityId != 0xFFFF && id != entityId))
            return false;
        entityId = id;
        references[referenceCount++] = &output;
        return true;
    }

    bool NativeAudio::CreateParent(Sound*& output)
    {
        ControllerLock lock(bindings.controller);
        if (!Register(output))
            return false;
        audSoundInitParams params;
        params.volume = SilenceDb;
        params.bucket = bindings.streamingBucket;
        params.flags = 0x60;
        bindings.CreateSound_PersistentReference_Name(entity, "MENU_MUSIC_1", &output, &params, -1, 0, 0);
        return output && GetRequestedSettings(output);
    }

    bool NativeAudio::AcquireChild(Sound* parent, std::size_t child, Sound*& output, const AudioPosition& position)
    {
        ControllerLock lock(bindings.controller);
        if (!parent || child >= 7 || !Register(output))
            return false;
        const char path[]{static_cast<char>('0' + child), '\0'};
        output = bindings.GetChildSoundReference(bindings.controller, parent, path, entity, &output);
        if (!output)
            return false;
        // A duplicate lookup does not register the new output address.
        if (Field<Sound**>(output, 144) != &output)
        {
            output = nullptr;
            return false;
        }
        auto* settings = GetRequestedSettings(output);
        if (!settings)
            return false;
        Field<std::uint8_t>(settings, 231) |= 0x08;
        bindings.SetPosition(settings, &position);
        if (child < 6)
        {
            Field<std::uint8_t>(settings, 231) |= 0x20;
            bindings.SetShadowSoundId(settings, GetSoundId(output));
        }
        return true;
    }

    bool NativeAudio::CreateShadow(Episode episode, Sound* source, std::size_t shadow, Sound*& output, const AudioPosition& position)
    {
        ControllerLock lock(bindings.controller);
        if (!source || shadow >= 6 || !Register(output))
            return false;
        char name[32];
        std::snprintf(name, sizeof(name), "MENU_MUSIC_SHADOW_SOUND_%u", static_cast<unsigned int>(shadow));
        audSoundInitParams params;
        params.volume = SilenceDb;
        params.shadowSource = source;
        params.bucket = Field<std::uint8_t>(source, 64);
        params.flags = 0x60;
        bindings.CreateSound_PersistentReference_Name(entity, name, &output, &params, -1, 0, 0);
        auto* settings = GetRequestedSettings(output);
        if (!settings)
            return false;
        bindings.PrepareAndPlay(output, nullptr, false, 0);
        bindings.SetPosition(settings, &position);
        GetOcclusionMetric(settings).suppressEnvironmentReverb = SuppressEnvironmentReverb(episode, shadow * 2 + 1);
        return true;
    }

    PrepareStatus NativeAudio::Prepare(Sound* parent, audWaveSlot* waveSlot)
    {
        ControllerLock lock(bindings.controller);
        return static_cast<PrepareStatus>(bindings.Prepare(parent, waveSlot, true));
    }

    void NativeAudio::PlayParent(Sound* parent)
    {
        ControllerLock lock(bindings.controller);
        bindings.Play(parent);
    }

    BankStatus NativeAudio::GetBankLoadingStatus(audWaveSlot* waveSlot, const char* bank) const
    {
        if (!waveSlot || !bank)
            return BankStatus::Failed;
        const auto id = bindings.FindBankId(bank);
        return id == -1 ? BankStatus::Failed : static_cast<BankStatus>(bindings.GetBankLoadingStatus(waveSlot, id));
    }

    void NativeAudio::LoadBank(audWaveSlot* waveSlot, const char* bank) const
    {
        if (waveSlot && bank)
            if (const auto id = bindings.FindBankId(bank); id != -1)
                bindings.LoadBank(waveSlot, id);
    }

    bool NativeAudio::ReferencesValid() const
    {
        ControllerLock lock(bindings.controller);
        return referenceCount == references.size() && std::all_of(references.begin(), references.end(), [&](Sound** slot) {
            auto* settings = GetRequestedSettings(*slot);
            return settings && Field<Sound**>(*slot, 144) == slot &&
                Field<std::uint16_t>(*slot, 6) != 3 && !(Field<std::uint8_t>(settings, 232) & 0x18);
        });
    }

    int NativeAudio::ParentPlayTimeMs(Sound* parent) const
    {
        ControllerLock lock(bindings.controller);
        return parent ? Field<int>(parent, 184) : 0;
    }

    void NativeAudio::SetParentVolume(Sound* parent, float volume)
    {
        ControllerLock lock(bindings.controller);
        if (auto* settings = GetRequestedSettings(parent))
            bindings.SetVolume(settings, volume);
    }

    void NativeAudio::SetVoice(Sound* sound, const VoiceSettings& voice, const AudioPosition& position)
    {
        ControllerLock lock(bindings.controller);
        auto* settings = GetRequestedSettings(sound);
        if (!settings)
            return;
        bindings.SetVolume(settings, voice.volumeDb);
        if (voice.updatePositionAndPan)
        {
            bindings.SetPan(settings, voice.panDegrees);
            bindings.SetPosition(settings, &position);
        }
        auto& metric = GetOcclusionMetric(settings);
        metric.reverbSize = voice.reverbSize;
        metric.reverbWet = voice.reverbWet;
        if (voice.lowPassHz >= 0)
            bindings.SetLowPassFilterCutoff(settings, static_cast<unsigned int>(voice.lowPassHz));
    }

    void NativeAudio::TriggerMusicOneshot(Accent accent, int predelay, audWaveSlot* waveSlot, const AudioPosition& position, Random& random)
    {
        if (!waveSlot)
            return;
        ControllerLock lock(bindings.controller);
        audSoundInitParams params;
        params.predelay = predelay;
        params.pan = random.Integer(0, 360);
        params.position = &position;
        Sound* sound = nullptr;
        bindings.CreateSound_LocalReference_Name(entity, accent == Accent::Dulcimer ? "MENU_MUSIC_DULCIMER" : "MENU_MUSIC_PERCUSSION", &sound, &params, -1, 0, 0);
        if (auto* settings = GetRequestedSettings(sound))
        {
            auto& metric = GetOcclusionMetric(settings);
            metric.reverbSize = random.Range(0.5f, 1.0f);
            metric.reverbWet = random.Range(0.5f, 0.3f);
            metric.suppressEnvironmentReverb = random.Range(0.0f, 1.0f) < 0.3f;
            bindings.PrepareAndPlay(sound, waveSlot, false, 0);
        }
    }

    void NativeAudio::InvalidateEntitySoundReference(Sound*& sound)
    {
        ControllerLock lock(bindings.controller);
        if (auto* settings = GetRequestedSettings(sound))
        {
            Field<std::uint8_t>(settings, 232) |= 0x10;
            sound = nullptr;
        }
    }

    void NativeAudio::StopAndForget(Sound*& sound)
    {
        ControllerLock lock(bindings.controller);
        if (sound && GetRequestedSettings(sound))
            bindings.StopAndForget(sound, false);
    }

    bool NativeAudio::HasRegisteredSounds(std::uint16_t id) const
    {
        auto index = Field<std::uint16_t>(bindings.controller, 4004 + 8 * id);
        for (std::size_t visited = 0; index != 0xFFFF; ++visited)
        {
            if (index >= 1000 || visited >= 1000)
                return true;
            auto* reference = reinterpret_cast<std::byte*>(bindings.controller) + 4 * index;
            auto* bucket = bindings.soundBuckets + 28480 * Field<std::uint8_t>(reference, 2);
            auto* base = Field<std::byte*>(bucket, 28432);
            if (!base)
                return true;
            auto* sound = base + bindings.soundStride * Field<std::uint8_t>(reference, 3);
            auto** slot = Field<Sound**>(sound, 144);
            if (slot && std::find(references.begin(), references.end(), slot) != references.end())
                return true;
            index = Field<std::uint16_t>(reference, 0);
        }
        return false;
    }

    bool NativeAudio::CleanupComplete() const
    {
        if (engineStopped)
            return true;
        ControllerLock lock(bindings.controller);
        if (entityId == 0xFFFF)
            return true;
        if (Field<std::uint16_t>(entity, 4) == entityId && HasRegisteredSounds(entityId))
            return false;
        return !HasRegisteredSounds(0);
    }

    bool NativeAudio::ResetReferences()
    {
        if (!CleanupComplete() || std::any_of(references.begin(), references.end(), [](Sound** slot) { return slot && *slot; }))
            return false;
        references = {};
        referenceCount = 0;
        entityId = 0xFFFF;
        return true;
    }

    bool NativeStream::Owns(const audStreamClientSettings& settings) const
    {
        return settings.userData == this && settings.HasMusicStoppedCallback == HasMusicStoppedCallback;
    }

    Stream* NativeStream::Allocate()
    {
        if (allocated || !cleanupComplete(owner))
            return nullptr;
        stopRequested = false;
        audStreamClientSettings settings;
        settings.OnMusicStopCallback = OnMusicStopCallback;
        settings.HasMusicStoppedCallback = HasMusicStoppedCallback;
        settings.userData = this;
        settings.priority = 5;
        allocated = bindings.AllocateSlot(&settings);
        return allocated;
    }

    StreamStatus NativeStream::Status(Stream* stream) const
    {
        if (!stream || stream != allocated || stopRequested)
            return StreamStatus::Lost;
        if (stream->state == 1 && Owns(stream->active))
            return StreamStatus::Ready;
        if (Owns(stream->requested))
            return StreamStatus::Pending;
        return StreamStatus::Lost;
    }

    void NativeStream::Free(Stream* stream)
    {
        if (!stream || stream != allocated)
            return;
        stopRequested = true;
        if (Owns(stream->requested))
            stream->requested = {};
        if (Owns(stream->active) && stream->state == 1)
            bindings.Free(stream);
    }

    bool NativeStream::CleanupComplete()
    {
        if (!cleanupComplete(owner))
            return false;
        if (allocated && (Owns(allocated->active) || Owns(allocated->requested)))
            return false;
        allocated = nullptr;
        return true;
    }

    bool __cdecl NativeStream::OnMusicStopCallback(void* context)
    {
        static_cast<NativeStream*>(context)->stopRequested = true;
        return true;
    }

    bool __cdecl NativeStream::HasMusicStoppedCallback(void* context)
    {
        const auto* stream = static_cast<NativeStream*>(context);
        return stream->cleanupComplete(stream->owner);
    }
}
