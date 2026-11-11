#include "Bindings.hpp"

#include <cstring>
#include <windows.h>

namespace MenuMusic
{
    namespace
    {
        #include "Profiles.inl"
    }

    const ExecutableBindings* FindExecutableBindings()
    {
        const auto base = reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
        const auto delta = base - 0x400000;
        const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
        const auto* pe = reinterpret_cast<const IMAGE_NT_HEADERS*>(base + dos->e_lfanew);
        for (const auto& profile : Profiles)
        {
            if (pe->FileHeader.TimeDateStamp != profile.timestamp || pe->OptionalHeader.SizeOfImage != profile.imageSize)
                continue;
            bool valid = true;
            for (const auto& entry : profile.entries)
                valid &= entry.Matches(reinterpret_cast<const void*>(entry.address + delta), delta);
            if (valid)
            {
                static ExecutableBindings relocated;
                relocated = profile.Rebased(delta);
                return &relocated;
            }
        }
        return nullptr;
    }

    NativeBindings ExecutableBindings::Audio() const
    {
        NativeBindings result;
        result.CreateSound_PersistentReference_Name = Function<NativeBindings::CreateSound>(Entry::rage__audEntity__CreateSound_PersistentReference_Name);
        result.CreateSound_LocalReference_Name = Function<NativeBindings::CreateSound>(Entry::rage__audEntity__CreateSound_LocalReference_Name);
        result.GetChildSoundReference = Function<decltype(result.GetChildSoundReference)>(Entry::rage__audController__GetChildSoundReference);
        result.Prepare = Function<decltype(result.Prepare)>(Entry::rage__audSound__Prepare);
        result.Play = Function<decltype(result.Play)>(Entry::rage__audSound__Play);
        result.PrepareAndPlay = Function<decltype(result.PrepareAndPlay)>(Entry::rage__audSound__PrepareAndPlay);
        result.StopAndForget = Function<decltype(result.StopAndForget)>(Entry::rage__audSound__StopAndForget);
        result.SetVolume = Function<decltype(result.SetVolume)>(Entry::rage__audRequestedSettings__SetVolume);
        result.SetPan = Function<decltype(result.SetPan)>(Entry::rage__audRequestedSettings__SetPan);
        result.SetPosition = Function<decltype(result.SetPosition)>(Entry::rage__audRequestedSettings__SetPosition);
        result.SetLowPassFilterCutoff = Function<decltype(result.SetLowPassFilterCutoff)>(Entry::rage__audRequestedSettings__SetLowPassFilterCutoff);
        result.SetShadowSoundId = Function<decltype(result.SetShadowSoundId)>(Entry::rage__audRequestedSettings__SetShadowSoundId);
        result.FindBankId = Function<decltype(result.FindBankId)>(Entry::rage__audWaveSlot__FindBankId);
        result.GetBankLoadingStatus = Function<decltype(result.GetBankLoadingStatus)>(Entry::rage__audWaveSlot__GetBankLoadingStatus);
        result.LoadBank = Function<decltype(result.LoadBank)>(Entry::rage__audWaveSlot__LoadBank);
        result.GetRandomNumberInRange_Float = Function<decltype(result.GetRandomNumberInRange_Float)>(Entry::rage__audEngineUtil__GetRandomNumberInRange_Float);
        result.GetRandomNumberInRange_Int = Function<decltype(result.GetRandomNumberInRange_Int)>(Entry::rage__audEngineUtil__GetRandomNumberInRange_Int);
        result.controller = *Address<audController*>(Global::Controller);
        result.soundBuckets = *Address<std::byte*>(Global::SoundBuckets);
        result.soundStride = *Address<std::uint32_t>(Global::SoundStride);
        result.requestedSettingsStride = *Address<std::uint32_t>(Global::SettingsStride);
        result.streamingBucket = *Address<std::uint8_t>(Global::StreamBucket);
        return result;
    }

    StreamBindings ExecutableBindings::Streams() const
    {
        StreamBindings result;
        result.AllocateSlot = Function<decltype(result.AllocateSlot)>(Entry::audStreamSlot__AllocateSlot);
        result.Free = Function<decltype(result.Free)>(Entry::audStreamSlot__Free);
        return result;
    }
}
