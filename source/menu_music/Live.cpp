#include "Live.hpp"
#include "Bindings.hpp"
#include "Session.hpp"
#include "NativeBackend.hpp"
#include "Effects.hpp"
#include "StreamRefill.hpp"

#include <memory>
#include <vector>
#include <windows.h>
#include <safetyhook.hpp>

namespace MenuMusic
{
    namespace
    {
        template<typename T> T& Field(void* object, std::size_t offset)
        {
            return *reinterpret_cast<T*>(static_cast<std::byte*>(object) + offset);
        }

        const ExecutableBindings* executable = nullptr;
        int(*getEpisode)() = nullptr;
        SafetyHookInline initHook, shutdownHook, engineShutdownHook, inputHook;
        SafetyHookMid updateHook, effectsHook, delayHook;
        SafetyHookInline refillHook, submitHook, pcmRefillHook, pcmSubmitHook;
        StreamRefill streamRefill;
        PcmStreamRefill pcmStreamRefill;

        class Environment final : public NativeEnvironment
        {
        public:
            Environment(audEntity* entity, void* category)
                : entity(entity), volume(Field<float>(category, 40)) {}

            AudioPosition ListenerPosition() const override
            {
                const auto* position = executable->Address<float>(Global::Listener);
                return {position[0], position[1], position[2], 0.0f};
            }

            audWaveSlot* AccentWaveSlot() const override { return Field<audWaveSlot*>(entity, 1020); }
            void SetEffects(const Profile& profile) override { effects.Set(profile); }
            void RestoreEffects() override { effects.Release(); }

            void UpdateVolume()
            {
                float attenuation = 0.0f;
                if (*executable->Address<bool>(Global::Phone) && !*executable->Address<bool>(Global::PhoneBypass))
                    attenuation = SilenceDb;
                else if (executable->legacy)
                {
                    const int headphones = *executable->Address<int>(Global::Headphones);
                    void* profile = *executable->Address<void*>(Global::Profile);
                    if (headphones == 0 && profile)
                    {
                        const auto getInt = executable->Function<int(__thiscall*)(void*, int)>(Entry::CProfileSettings__GetInt);
                        const int voiceVolume = getInt(profile, 102);
                        const int speakers = getInt(profile, 100);
                        if (voiceVolume > 0 && speakers != 0)
                        {
                            auto* peer = executable->Address<void>(Global::Peer);
                            const bool talking = executable->Function<bool(__thiscall*)(void*)>(Entry::CNetworkPeerMgr__IsAnyRemotePlayerTalking)(peer);
                            const bool about = !talking && executable->Function<bool(__thiscall*)(void*)>(Entry::CNetworkPeerMgr__IsAnyRemotePlayerAboutToTalk)(peer);
                            attenuation = VoiceAttenuation(headphones, voiceVolume, speakers, talking, about);
                        }
                    }
                }
                volume.Update(attenuation);
            }

            void Release() { effects.Release(); volume.Release(); }

        private:
            audEntity* entity;
            CategoryVolume volume;
            Effects effects;
        };

        class Runtime
        {
        public:
            Runtime(audEntity* entity, void* category, const NativeBindings& bindings)
                : entity(entity), bindings(bindings), streams(executable->Streams()), environment(entity, category),
                random(this->bindings), backend(this->bindings, streams, entity, environment), frontend(backend, random) {}

            void Update(std::uint32_t time)
            {
                if (engineStopped)
                    return;
                if (retired)
                {
                    frontend.Update(false, Episode::IV, time, false, true);
                    return;
                }
                const int episode = getEpisode();
                const bool lobby = executable->Function<bool(__cdecl*)()>(Entry::CNetwork__IsOnLobbyScreen)();
                const bool eligible = episode >= 0 && episode <= 2 && Field<bool>(entity, 1085) &&
                    executable->Function<bool(__thiscall*)(audEntity*)>(Entry::audFrontendAudioEntity__ShouldFEMusicBePlaying)(entity);
                const bool stop = Field<int>(entity, 512) == 4 && frontend.NativeState() != 4;
                environment.UpdateVolume();
                frontend.Update(eligible, episode == 2 ? Episode::TBoGT : episode == 1 ? Episode::TLAD : Episode::IV,
                    time, lobby, stop);
                Field<int>(entity, 512) = frontend.NativeState();
                if (frontend.State() == SessionState::WaitingForStream || frontend.State() == SessionState::Preparing)
                {
                    if (auto* sound = Field<Sound*>(entity, 844))
                        bindings.StopAndForget(sound, false);
                    if (auto* sound = Field<Sound*>(entity, 552))
                    {
                        bindings.StopAndForget(sound, false);
                        Field<int>(entity, 564) = 0;
                    }
                }
            }

            void Input()
            {
                if (!retired && !engineStopped)
                    frontend.NotifyInput(executable->Function<bool(__cdecl*)()>(Entry::CNetwork__IsOnLobbyScreen)());
            }

            void Retire()
            {
                if (retired || engineStopped)
                    return;
                retired = true;
                frontend.Stop();
                frontend.Update(false, Episode::IV, 0, false, true);
                environment.Release();
            }

            void EngineStopped()
            {
                engineStopped = true;
                backend.EngineStopped();
            }

            audEntity* entity;

        private:
            NativeBindings bindings;
            StreamBindings streams;
            Environment environment;
            NativeRandom random;
            NativeBackend backend;
            Frontend frontend;
            bool retired = false;
            bool engineStopped = false;
        };

        Runtime* current = nullptr;
        audEntity* controlledEntity = nullptr;

        auto& Runtimes()
        {
            // Native stream callbacks can outlive frontend entity reinitialization.
            static auto* runtimes = new std::vector<std::unique_ptr<Runtime>>;
            return *runtimes;
        }

        int __fastcall audFrontendAudioEntity__InitHook(audEntity* entity, void* edx)
        {
            if (current)
                current->Retire();
            current = nullptr;
            controlledEntity = nullptr;
            const int result = initHook.unsafe_fastcall<int>(entity, edx);
            const auto audio = executable->Audio();
            if ((result & 0xFF) && audio.Complete() && Field<std::uint16_t>(entity, 4) != 0xFFFF)
            {
                auto* categoryManager = executable->Address<void>(Global::CategoryManager);
                auto* category = executable->Function<void*(__thiscall*)(void*, std::uint32_t)>(Entry::rage__audCategoryManager__GetCategoryPtr)(categoryManager, 0xB65AE241);
                if (category)
                {
                    auto runtime = std::make_unique<Runtime>(entity, category, audio);
                    current = runtime.get();
                    controlledEntity = entity;
                    Runtimes().push_back(std::move(runtime));
                }
            }
            return result;
        }

        void __fastcall audFrontendAudioEntity__ShutdownHook(audEntity* entity, void* edx)
        {
            if (current && current->entity == entity)
            {
                current->Retire();
                current = nullptr;
            }
            shutdownHook.unsafe_fastcall<void>(entity, edx);
        }

        int __cdecl audGtaAudioEngine__ShutdownClassHook()
        {
            if (current)
                current->Retire();
            current = nullptr;
            const int result = engineShutdownHook.unsafe_ccall<int>();
            for (auto& runtime : Runtimes())
                runtime->EngineStopped();
            return result;
        }

        void __fastcall audFrontendAudioEntity__NotifyFrontendInputHook(audEntity* entity, void* edx)
        {
            if (current && current->entity == entity)
                current->Input();
            else
                inputHook.unsafe_fastcall<void>(entity, edx);
        }

        void audFrontendAudioEntity__UpdateMusicHook(SafetyHookContext& regs)
        {
            // UpdateMusic carries its unscaled time in EBX on CE and [ESP + 16] on 1.0.7/8.
            const auto time = executable->legacy ? *reinterpret_cast<const std::uint32_t*>(regs.esp + 16) : regs.ebx;
            for (auto& runtime : Runtimes())
                if (runtime.get() != current)
                    runtime->Update(time);
            if (current && current->entity == reinterpret_cast<audEntity*>(regs.esi))
            {
                current->Update(time);
                regs.eip = executable->EntryAddress(Entry::audFrontendAudioEntity__UpdateMusic_End);
            }
            else if (controlledEntity == reinterpret_cast<audEntity*>(regs.esi))
            {
                Field<int>(controlledEntity, 512) = 0;
                regs.eip = executable->EntryAddress(Entry::audFrontendAudioEntity__UpdateMusic_End);
            }
        }

        bool OwnsStreamVoice(void* voice)
        {
            auto* wave = loopingMenuWaveSlot.load();
            return wave && wave == Field<void*>(voice, 16);
        }

        unsigned int __fastcall audVoicePcAdpcm__RefillAdpcmBufferHook(void* voice, void* edx, bool initial)
        {
            streamRefill.Before(voice, initial, OwnsStreamVoice(voice), GetTickCount());
            return refillHook.unsafe_fastcall<unsigned int>(voice, edx, initial);
        }

        unsigned int __fastcall audVoicePcAdpcm__AddSampleDataHook(void* voice, void* edx, unsigned int offset)
        {
            const auto result = submitHook.unsafe_fastcall<unsigned int>(voice, edx, offset);
            streamRefill.Submitted(voice, OwnsStreamVoice(voice), GetTickCount(),
                executable->Function<StreamRefill::Decode>(Entry::rage__IMA_ADPCM__Decode));
            return result;
        }

        unsigned int __fastcall audVoiceSoft__RefillPcmBufferHook(void* voice, void* edx, bool initial)
        {
            pcmStreamRefill.Before(voice, initial, OwnsStreamVoice(voice), GetTickCount());
            return pcmRefillHook.unsafe_fastcall<unsigned int>(voice, edx, initial);
        }

        unsigned int __fastcall audVoiceSoft__AddSampleDataHook(void* voice, void* edx, unsigned int offset)
        {
            const auto result = pcmSubmitHook.unsafe_fastcall<unsigned int>(voice, edx, offset);
            pcmStreamRefill.Submitted(voice, OwnsStreamVoice(voice), GetTickCount());
            return result;
        }

        void audGtaEnvironment__SetUpSourceReverbFiltersHook(SafetyHookContext& regs)
        {
            const auto route = executable->legacy ? regs.ebx : regs.esi;
            auto* chain = reinterpret_cast<audEffect*>(executable->legacy ? regs.ebp : regs.edi);
            Effects::BeforeCommit(route, chain);
        }

        void audDelayEffect__UpdateHook(SafetyHookContext& regs)
        {
            auto* driver = *executable->Address<void*>(Global::AudioDriver);
            if (driver)
                Effects::AfterDelayUpdate(reinterpret_cast<audDelayEffect*>(regs.esi),
                    Field<unsigned int>(driver, 128), *executable->Address<unsigned int>(Global::FrameSamples));
        }

        void ResetHooks()
        {
            delayHook.reset();
            effectsHook.reset();
            updateHook.reset();
            pcmSubmitHook.reset();
            pcmRefillHook.reset();
            submitHook.reset();
            refillHook.reset();
            inputHook.reset();
            engineShutdownHook.reset();
            shutdownHook.reset();
            initHook.reset();
        }
    }

    bool InstallLiveHooks(int(*currentEpisode)())
    {
        executable = FindExecutableBindings();
        if (!executable || !currentEpisode)
        {
            OutputDebugStringA("Fusion Fix: dynamic menu music bindings did not match.\n");
            return false;
        }
        getEpisode = currentEpisode;
        initHook = safetyhook::create_inline(executable->Function<void*>(Entry::audFrontendAudioEntity__Init), audFrontendAudioEntity__InitHook, SafetyHookInline::StartDisabled);
        shutdownHook = safetyhook::create_inline(executable->Function<void*>(Entry::audFrontendAudioEntity__Shutdown), audFrontendAudioEntity__ShutdownHook, SafetyHookInline::StartDisabled);
        engineShutdownHook = safetyhook::create_inline(executable->Function<void*>(Entry::audGtaAudioEngine__ShutdownClass), audGtaAudioEngine__ShutdownClassHook, SafetyHookInline::StartDisabled);
        inputHook = safetyhook::create_inline(executable->Function<void*>(Entry::audFrontendAudioEntity__NotifyFrontendInput), audFrontendAudioEntity__NotifyFrontendInputHook, SafetyHookInline::StartDisabled);
        updateHook = safetyhook::create_mid(executable->Function<void*>(Entry::audFrontendAudioEntity__UpdateMusic_StateSwitch), audFrontendAudioEntity__UpdateMusicHook, SafetyHookMid::StartDisabled);
        refillHook = safetyhook::create_inline(executable->Function<void*>(Entry::rage__audVoicePcAdpcm__RefillAdpcmBuffer), audVoicePcAdpcm__RefillAdpcmBufferHook, SafetyHookInline::StartDisabled);
        submitHook = safetyhook::create_inline(executable->Function<void*>(Entry::rage__audVoicePcAdpcm__AddSampleData), audVoicePcAdpcm__AddSampleDataHook, SafetyHookInline::StartDisabled);
        pcmRefillHook = safetyhook::create_inline(executable->Function<void*>(Entry::rage__audVoiceSoft__RefillPcmBuffer), audVoiceSoft__RefillPcmBufferHook, SafetyHookInline::StartDisabled);
        pcmSubmitHook = safetyhook::create_inline(executable->Function<void*>(Entry::rage__audVoiceSoft__AddSampleData), audVoiceSoft__AddSampleDataHook, SafetyHookInline::StartDisabled);
        effectsHook = safetyhook::create_mid(executable->Function<void*>(Entry::audGtaEnvironment__SetUpSourceReverbFilters_Commit), audGtaEnvironment__SetUpSourceReverbFiltersHook, SafetyHookMid::StartDisabled);
        delayHook = safetyhook::create_mid(executable->Function<void*>(Entry::rage__audDelayEffect__Update_AfterSetParameters), audDelayEffect__UpdateHook, SafetyHookMid::StartDisabled);
        if (!delayHook || !effectsHook || !initHook || !shutdownHook || !engineShutdownHook || !inputHook || !updateHook || !refillHook || !submitHook || !pcmRefillHook || !pcmSubmitHook ||
            !delayHook.enable() || !effectsHook.enable() || !initHook.enable() || !shutdownHook.enable() || !engineShutdownHook.enable() || !inputHook.enable() || !refillHook.enable() || !submitHook.enable() || !pcmRefillHook.enable() || !pcmSubmitHook.enable() || !updateHook.enable())
        {
            ResetHooks();
            OutputDebugStringA("Fusion Fix: dynamic menu music hooks could not be installed.\n");
            return false;
        }
        return true;
    }
}
