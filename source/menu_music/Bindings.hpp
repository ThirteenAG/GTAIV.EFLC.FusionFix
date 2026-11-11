#pragma once

#include "Native.hpp"

#include <cstring>

namespace MenuMusic
{
    enum class Entry : std::size_t
    {
        rage__audEntity__CreateSound_PersistentReference_Name,
        rage__audEntity__CreateSound_LocalReference_Name,
        rage__audController__GetChildSoundReference,
        rage__audSound__Prepare,
        rage__audSound__Play,
        rage__audSound__PrepareAndPlay,
        rage__audSound__StopAndForget,
        rage__audRequestedSettings__SetVolume,
        rage__audRequestedSettings__SetPan,
        rage__audRequestedSettings__SetPosition,
        rage__audRequestedSettings__SetLowPassFilterCutoff,
        rage__audRequestedSettings__SetShadowSoundId,
        rage__audWaveSlot__FindBankId,
        rage__audWaveSlot__GetBankLoadingStatus,
        rage__audWaveSlot__LoadBank,
        rage__audEngineUtil__GetRandomNumberInRange_Float,
        rage__audEngineUtil__GetRandomNumberInRange_Int,
        audStreamSlot__AllocateSlot,
        audStreamSlot__Free,
        rage__audCategoryManager__GetCategoryPtr,
        audFrontendAudioEntity__ShouldFEMusicBePlaying,
        CNetwork__IsOnLobbyScreen,
        CProfileSettings__GetInt,
        CNetworkPeerMgr__IsAnyRemotePlayerTalking,
        CNetworkPeerMgr__IsAnyRemotePlayerAboutToTalk,
        audFrontendAudioEntity__Init,
        audFrontendAudioEntity__Shutdown,
        audGtaAudioEngine__ShutdownClass,
        audFrontendAudioEntity__NotifyFrontendInput,
        audFrontendAudioEntity__UpdateMusic_StateSwitch,
        audFrontendAudioEntity__UpdateMusic_End,
        rage__audVoicePcAdpcm__RefillAdpcmBuffer,
        rage__audVoicePcAdpcm__AddSampleData,
        rage__IMA_ADPCM__Decode,
        audGtaEnvironment__SetUpSourceReverbFilters_Commit,
        rage__audDelayEffect__Update_AfterSetParameters,
        rage__audVoiceSoft__RefillPcmBuffer,
        rage__audVoiceSoft__AddSampleData,
        Count
    };

    enum class Global : std::size_t
    {
        Controller, SoundBuckets, SoundStride, SettingsStride, StreamBucket, CategoryManager,
        Listener, Headphones, Profile, Peer, Phone, PhoneBypass, AudioDriver, FrameSamples, Count
    };

    struct EntryPoint
    {
        std::uintptr_t address;
        std::array<std::uint8_t, 8> bytes;
        std::uint8_t relocations = 0;

        bool Matches(const void* loaded, std::uintptr_t delta) const
        {
            auto expected = bytes;
            for (std::size_t i = 0; i < expected.size(); ++i)
            {
                if (!(relocations & (1u << i)))
                    continue;
                std::uint32_t value = 0;
                const auto count = expected.size() - i < 4 ? expected.size() - i : 4;
                for (std::size_t j = 0; j < count; ++j)
                    value |= static_cast<std::uint32_t>(expected[i + j]) << (j * 8);
                value += static_cast<std::uint32_t>(delta);
                for (std::size_t j = 0; j < count; ++j)
                    expected[i + j] = static_cast<std::uint8_t>(value >> (j * 8));
            }
            return std::memcmp(loaded, expected.data(), expected.size()) == 0;
        }
    };

    struct ExecutableBindings
    {
        std::uint32_t timestamp;
        std::uint32_t imageSize;
        bool legacy;
        std::array<EntryPoint, static_cast<std::size_t>(Entry::Count)> entries;
        std::array<std::uintptr_t, static_cast<std::size_t>(Global::Count)> globals;

        std::uintptr_t EntryAddress(Entry entry) const
        {
            return entries[static_cast<std::size_t>(entry)].address;
        }

        template<typename T> T Function(Entry entry) const
        {
            return reinterpret_cast<T>(EntryAddress(entry));
        }

        template<typename T> T* Address(Global global) const
        {
            return reinterpret_cast<T*>(globals[static_cast<std::size_t>(global)]);
        }

        ExecutableBindings Rebased(std::uintptr_t delta) const
        {
            auto result = *this;
            for (auto& entry : result.entries)
                entry.address += delta;
            for (auto& global : result.globals)
                global += delta;
            return result;
        }

        NativeBindings Audio() const;
        StreamBindings Streams() const;
    };

    const ExecutableBindings* FindExecutableBindings();
}
