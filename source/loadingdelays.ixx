module;

#include <common.hxx>
#include <atomic>

export module loadingdelays;

import common;
import comvars;
import settings;

export bool FixLoadingSynchronization()
{
    static const bool bInstalled = []()
    {
        struct LoadingPatterns
        {
            const char* renderThread;
            const char* loadingQuery;
            const char* streamingFlush;
            int nPollOffset;
            int nCompletionOffset;
            int nSleepOffset;
        };
        static constexpr LoadingPatterns builds[]{
            { // 1.0.7.0
                "8D AF D0 0F 00 00 8D B7 E0 0F 00 00 B9 ? ? ? ? E8 91 FD DF FF 56 55 E8 7A 35 DF FF 83 C4 08 "
                "84 C0 75 13 F3 0F 7E 45 00 66 0F D6 06 F3 0F 7E 45 08 66 0F D6 46 08 B9 ? ? ? ? E8 96 FD DF "
                "FF 83 3E 00 75 0A 83 7E 08 00 0F 84 86 00 00 00 8B 97 B4 0F 00 00 52 E8 5B C1 DE FF 8A D8 8B 46 "
                "0C 56 FF D0 83 C4 08 84 DB 74 2A B9 ? ? ? ? E8 72 A6 FF FF 8B 97 B8 0F 00 00 B9 01 00 00 00 "
                "2B 8F C4 0F 00 00 52 89 8F C4 0F 00 00 E8 85 C1 DE FF 83 C4 04",
                "83 EC 14 56 6A 04 8D 44 24 08 50 6A 00 8B F1 68 ? ? ? ? 8D 4C 24 18 C7 44 24 14 ? ? ? ? "
                "E8 9B 21 DF FF 8D 4C 24 08 51 81 C6 E0 0F 00 00 56 C7 44 24 1C ? ? ? ? E8 E2 21 DF FF 83 C4 "
                "08 84 C0 0F 95 C0 5E 83 C4 14 C3",
                "56 57 8B F1 33 FF 39 BE 3C 31 00 00 0F 84 7F 00 00 00 8B 86 38 31 00 00 83 C0 01 83 F8 08 75 02 "
                "33 C0 8B 84 86 14 31 00 00 8B 48 0C 51 E8 0E 12 95 FF 83 C4 04 84 C0 74 38 83 86 3C 31 00 00 FF "
                "83 86 38 31 00 00 01 83 BE 38 31 00 00 08 75 0A C7 86 38 31 00 00 00 00 00 00 8B 96 38 31 00 00 "
                "8B 84 96 14 31 00 00 50 8B CE E8 11 FF FF FF EB 17 6A 0A E8 28 14 95 FF 83 C7 0A 83 C4 04 81 FF "
                "E8 03 00 00 75 02 33 FF 83 BE 3C 31 00 00 00 75 81 5F 5E C3",
                0x57, 0x67, 0x71
            },
            { // 1.0.8.0
                "8D AF D0 0F 00 00 8D B7 E0 0F 00 00 B9 ? ? ? ? E8 51 51 BB FF 56 55 E8 EA 3C BC FF 83 C4 08 "
                "84 C0 75 13 F3 0F 7E 45 00 66 0F D6 06 F3 0F 7E 45 08 66 0F D6 46 08 B9 ? ? ? ? E8 56 51 BB "
                "FF 83 3E 00 75 0A 83 7E 08 00 0F 84 91 00 00 00 8B 97 B4 0F 00 00 52 E8 5B 47 BB FF 8A D8 8B 46 "
                "0C 56 FF D0 83 C4 08 84 DB 74 34 B9 ? ? ? ? E8 D2 1F FC FF B9 01 00 00 00 2B 8F C4 0F 00 00 "
                "89 8F C4 0F 00 00 B9 ? ? ? ? E8 B7 1C FC FF 8B 97 B8 0F 00 00 52 E8 7B 47 BB FF 83 C4 04",
                "83 EC 14 56 6A 04 8D 44 24 08 50 6A 00 8B F1 68 ? ? ? ? 8D 4C 24 18 C7 44 24 14 ? ? ? ? "
                "E8 FB 28 BC FF 8D 4C 24 08 51 81 C6 E0 0F 00 00 56 C7 44 24 1C ? ? ? ? E8 42 29 BC FF 83 C4 "
                "08 84 C0 0F 95 C0 5E 83 C4 14 C3",
                "56 57 8B F1 33 FF 39 BE 3C 31 00 00 0F 84 7F 00 00 00 8B 86 38 31 00 00 83 C0 01 83 F8 08 75 02 "
                "33 C0 8B 84 86 14 31 00 00 8B 48 0C 51 E8 CE 46 7A FF 83 C4 04 84 C0 74 38 83 86 3C 31 00 00 FF "
                "83 86 38 31 00 00 01 83 BE 38 31 00 00 08 75 0A C7 86 38 31 00 00 00 00 00 00 8B 96 38 31 00 00 "
                "8B 84 96 14 31 00 00 50 8B CE E8 11 FF FF FF EB 17 6A 0A E8 E8 48 7A FF 83 C7 0A 83 C4 04 81 FF "
                "E8 03 00 00 75 02 33 FF 83 BE 3C 31 00 00 00 75 81 5F 5E C3",
                0x57, 0x67, 0x71
            },
            { // 1.2.0.59
                "8D AF D0 0F 00 00 8D B7 E0 0F 00 00 B9 ? ? ? ? E8 E3 9C B2 FF 56 55 E8 CC 7C B3 FF 83 C4 08 "
                "84 C0 75 13 F3 0F 7E 45 00 66 0F D6 06 F3 0F 7E 45 08 66 0F D6 46 08 B9 ? ? ? ? E8 E8 9C B2 "
                "FF 83 3E 00 75 12 83 7E 08 00 75 0C 8B CF E8 36 E9 FF FF E9 8C 00 00 00 FF B7 B4 0F 00 00 E8 C6 "
                "99 B2 FF 8B 4E 0C 56 8A D8 FF D1 83 C4 08 84 DB 74 33 B9 ? ? ? ? E8 CD 81 06 00 B8 01 00 00 "
                "00 2B 87 C4 0F 00 00 B9 ? ? ? ? 89 87 C4 0F 00 00 E8 32 7F 06 00 FF B7 B8 0F 00 00 E8 E7 99 "
                "B2 FF 83 C4 04",
                "83 EC 14 56 6A 04 8D 44 24 08 50 6A 00 8B F1 68 ? ? ? ? 8D 4C 24 18 C7 44 24 14 ? ? ? ? "
                "E8 BB 8F B3 FF 8D 44 24 08 50 8D 86 E0 0F 00 00 50 C7 44 24 1C ? ? ? ? E8 02 90 B3 FF 83 C4 "
                "08 84 C0 0F 95 C0 5E 83 C4 14 C3",
                "56 57 8B F9 33 F6 39 B7 3C 31 00 00 74 78 8B FF 8B 87 38 31 00 00 40 33 C9 83 F8 08 0F 44 C1 8B "
                "84 87 14 31 00 00 FF 70 0C E8 72 89 6A FF 83 C4 04 84 C0 74 30 FF 87 38 31 00 00 8B 87 38 31 00 "
                "00 FF 8F 3C 31 00 00 33 C9 83 F8 08 0F 44 C1 89 87 38 31 00 00 FF B4 87 14 31 00 00 8B CF E8 CD "
                "04 00 00 EB 18 6A 0A E8 94 8B 6A FF 83 C6 0A 33 C0 83 C4 04 81 FE E8 03 00 00 0F 44 F0 83 BF 3C "
                "31 00 00 00 75 8A 5F 5E C3",
                0x5E, 0x6E, 0x65
            }
        };

        const LoadingPatterns* matched = nullptr;
        uint8_t* renderThread = nullptr;
        for (const auto& build : builds)
        {
            auto pattern = hook::pattern(build.renderThread);
            if (pattern.empty())
                continue;
            if (pattern.size() != 1 || matched)
                return false;
            matched = &build;
            renderThread = pattern.get_first<uint8_t>();
        }
        if (!matched)
            return false;

        auto loadingQuery = hook::pattern(matched->loadingQuery);
        auto streamingFlush = hook::pattern(matched->streamingFlush);
        if (loadingQuery.size() != 1 || streamingFlush.size() != 1)
            return false;

        static const auto LoadingRenderFunction = *loadingQuery.get_first<uintptr_t>(0x1C);
        static const auto RenderThreadInterface = *loadingQuery.get_first<uintptr_t>(0x10);
        static const auto PollSemaphore = reinterpret_cast<bool(__cdecl*)(HANDLE)>(
            injector::GetBranchDestination(renderThread + matched->nPollOffset).as_int());

        static std::array CRenderThreadInterface_Hooks{
            safetyhook::create_mid(renderThread + 0x41, [](SafetyHookContext& regs)
            {
                // The render thread owns the active functor after adopting the pending callback.
                auto active = reinterpret_cast<const uintptr_t*>(regs.esi);
                bLoadingShown.store(active[0] == LoadingRenderFunction && active[1] == 0 &&
                    active[2] == RenderThreadInterface, std::memory_order_relaxed);
            }, SafetyHookMid::StartDisabled),
            safetyhook::create_mid(renderThread + matched->nCompletionOffset, [](SafetyHookContext& regs)
            {
                // Catch a request posted during loading rendering without consuming a second token.
                if (!(regs.ebx & 0xFF) && bLoadingShown.load(std::memory_order_relaxed))
                {
                    auto request = *reinterpret_cast<HANDLE*>(regs.edi + 0xFB4);
                    if (PollSemaphore(request))
                        regs.ebx |= 1;
                }
            }, SafetyHookMid::StartDisabled)
        };
        for (auto& hook : CRenderThreadInterface_Hooks)
        {
            if (!hook || !hook.enable())
            {
                for (auto& installed : CRenderThreadInterface_Hooks)
                    installed.reset();
                return false;
            }
        }

        // Yield while CLoaderDirect waits for I/O completion.
        injector::WriteMemory<uint8_t>(streamingFlush.get_first(matched->nSleepOffset + 1), 0, true);
        return true;
    }();
    return bInstalled;
}

void FixRepeatedLoadingFlushes()
{
    // CRenderThreadInterface::SetLoadingRenderFunction; full signatures protect the reused stack local.
    auto pattern = hook::pattern( // 1.0.8.0
        "83 EC 28 B8 34 00 00 00 39 05 ? ? ? ? 56 8B F1 0F 84 E2 00 00 00 39 05 ? ? ? ? 0F 84 D6 00 00 00 "
        "6A 04 8D 44 24 08 50 6A 00 68 ? ? ? ? 8D 4C 24 2C C7 44 24 14 ? ? ? ? E8 1E 27 BC FF "
        "8D 4C 24 1C 51 8D 96 E0 0F 00 00 52 C7 44 24 30 ? ? ? ? E8 65 27 BC FF 83 C4 08 84 C0 75 37 "
        "E8 B9 3D BB FF 80 3D ? ? ? ? 00 0F 57 C0 A3 ? ? ? ? 89 15 ? ? ? ? F3 0F 11 05 ? ? ? ? 74 13 "
        "6A 00 B9 ? ? ? ? E8 BE A9 F7 FF C6 05 ? ? ? ? 00 6A 00 B9 ? ? ? ? E8 2B E8 FF FF "
        "6A 04 8D 44 24 0C 50 6A 00 68 ? ? ? ? 8D 4C 24 1C C7 44 24 18 ? ? ? ? E8 9C 26 BC FF "
        "F3 0F 7E 44 24 0C 83 EC 10 8B C4 66 0F D6 00 C7 44 24 28 ? ? ? ? F3 0F 7E 44 24 24 "
        "8B CE 66 0F D6 40 08 E8 A3 FD FF FF 6A 00 B9 ? ? ? ? E8 D7 E7 FF FF 5E 83 C4 28 C3");
    auto nFunctorComparisonOffset = 0x5B;
    auto nFirstFlushOffset = 0x99;
    auto nSecondFlushOffset = 0xED;
    if (pattern.empty())
    {
        pattern = hook::pattern( // 1.2.0.59
            "83 EC 28 83 3D ? ? ? ? 34 56 8B F1 0F 84 E2 00 00 00 83 3D ? ? ? ? 34 0F 84 D5 00 00 00 "
            "6A 04 8D 44 24 08 50 6A 00 68 ? ? ? ? 8D 4C 24 2C C7 44 24 14 ? ? ? ? E8 B1 7A B3 FF "
            "8D 44 24 1C 50 8D 86 E0 0F 00 00 50 C7 44 24 30 ? ? ? ? E8 F8 7A B3 FF 83 C4 08 84 C0 75 36 "
            "E8 BC 9D B2 FF 80 3D ? ? ? ? 00 A3 ? ? ? ? 89 15 ? ? ? ? C7 05 ? ? ? ? 00 00 00 00 74 13 "
            "6A 00 B9 ? ? ? ? E8 62 B1 00 00 C6 05 ? ? ? ? 00 6A 00 B9 ? ? ? ? E8 4F E8 FF FF "
            "6A 04 8D 44 24 0C 50 6A 00 68 ? ? ? ? 8D 4C 24 1C C7 44 24 18 ? ? ? ? E8 30 7A B3 FF "
            "F3 0F 7E 44 24 0C 83 EC 10 8B C4 C7 44 24 28 ? ? ? ? 66 0F D6 00 F3 0F 7E 44 24 24 "
            "8B CE 66 0F D6 40 08 E8 A7 00 00 00 6A 00 B9 ? ? ? ? E8 FB E7 FF FF 5E 83 C4 28 C3");
        nFunctorComparisonOffset = 0x58;
        nFirstFlushOffset = 0x95;
        nSecondFlushOffset = 0xE9;
    }
    if (pattern.size() != 1)
        return;

    // Skip push 0, mov ecx and Flush together (12 bytes).
    static const auto FirstFlushResume = reinterpret_cast<uintptr_t>(pattern.get_first(nFirstFlushOffset + 12));
    static const auto SecondFlushResume = reinterpret_cast<uintptr_t>(pattern.get_first(nSecondFlushOffset + 12));
    // Keep SetRenderFunction: it publishes the pending callback even when the active one matches.
    static std::array CRenderThreadInterface__SetLoadingRenderFunction_Hooks{
        safetyhook::create_mid(pattern.get_first(nFunctorComparisonOffset), [](SafetyHookContext& regs)
        {
            // Save FunctorBase equality in the dead Src local: ESP+0xC here, ESP+4 after argument cleanup.
            *reinterpret_cast<uint8_t*>(regs.esp + 0xC) = static_cast<uint8_t>(regs.eax);
        }, SafetyHookMid::StartDisabled),
        safetyhook::create_mid(pattern.get_first(nFirstFlushOffset), [](SafetyHookContext& regs)
        {
            if (*reinterpret_cast<uint8_t*>(regs.esp + 4))
                regs.eip = FirstFlushResume;
        }, SafetyHookMid::StartDisabled),
        safetyhook::create_mid(pattern.get_first(nSecondFlushOffset), [](SafetyHookContext& regs)
        {
            if (*reinterpret_cast<uint8_t*>(regs.esp + 4))
                regs.eip = SecondFlushResume;
        }, SafetyHookMid::StartDisabled)
    };

    for (auto& hook : CRenderThreadInterface__SetLoadingRenderFunction_Hooks)
    {
        if (!hook || !hook.enable())
        {
            for (auto& installed : CRenderThreadInterface__SetLoadingRenderFunction_Hooks)
                installed.reset();
            return;
        }
    }
}

void WINAPI FusionSleep(DWORD dwMilliseconds)
{
    auto bMenuActive = CMenuManager::m_MenuActive && *CMenuManager::m_MenuActive;
    auto bLoadscreenActive = (CMenuManager::bLoadscreenShown && *CMenuManager::bLoadscreenShown) || bLoadingShown;

    if (!bMenuActive && bLoadscreenActive)
        return Sleep(0);

    return Sleep(dwMilliseconds);
}

class LoadingDelays
{
public:
    LoadingDelays()
    {
        FusionFix::onInitEvent() += []()
        {
            FixRepeatedLoadingFlushes();

            SettingsReader reader;

            // [MISC]
            auto bSkipLoadingSpinner = reader.ReadInteger("MISC", "SkipLoadingSpinner", 1) != 0;

            auto pattern = find_pattern("68 ? ? ? ? FF 15 ? ? ? ? 83 3D ? ? ? ? ? 75 ? E8 ? ? ? ? 8D 0C 24", "68 ? ? ? ? FF 15 ? ? ? ? 39 1D");
            injector::WriteMemory(pattern.get_first(1), 0, true);

            if (bSkipLoadingSpinner)
            {
                pattern = find_pattern("FF 35 ? ? ? ? EB ? FF 35 ? ? ? ? B9 ? ? ? ? E8 ? ? ? ? C6 44 24", "8B 0D ? ? ? ? 51 EB ? 8B 15 ? ? ? ? 52 B9 ? ? ? ? E8 ? ? ? ? C6 44 24");
                injector::MakeJMP(pattern.get_first(0), hook::get_pattern("6A ? 6A ? 6A ? 6A ? 6A ? 6A ? 6A ? 6A ? 6A ? 6A ? 6A ? 6A ? 6A ? 6A ? B9 ? ? ? ? C6 46"));
            }
        };

        FusionFix::onInitEvent() += []()
        {
            if (!FixLoadingSynchronization())
                IATHook::Replace(GetModuleHandleA(NULL), "kernel32.DLL", std::forward_as_tuple("Sleep", FusionSleep));
        };
    }
} LoadingDelays;
