module;

#include <common.hxx>

export module loadingdelays;

import common;
import comvars;
import framelimit;
import settings;

void FixRepeatedLoadingSynchronization()
{
    // Patch 8 (1.0.8.0): verify the entire selector because the hooks reuse a dead
    // stack local. Only relocated addresses are wildcarded; other builds and
    // altered control flow must not inherit these stack offsets.
    auto pattern = hook::pattern(
        "83 EC 28 B8 34 00 00 00 39 05 ? ? ? ? 56 8B F1 0F 84 E2 00 00 00 39 05 ? ? ? ? 0F 84 D6 00 00 00 "
        "6A 04 8D 44 24 08 50 6A 00 68 ? ? ? ? 8D 4C 24 2C C7 44 24 14 ? ? ? ? E8 1E 27 BC FF "
        "8D 4C 24 1C 51 8D 96 E0 0F 00 00 52 C7 44 24 30 ? ? ? ? E8 65 27 BC FF 83 C4 08 84 C0 75 37 "
        "E8 B9 3D BB FF 80 3D ? ? ? ? 00 0F 57 C0 A3 ? ? ? ? 89 15 ? ? ? ? F3 0F 11 05 ? ? ? ? 74 13 "
        "6A 00 B9 ? ? ? ? E8 BE A9 F7 FF C6 05 ? ? ? ? 00 6A 00 B9 ? ? ? ? E8 2B E8 FF FF "
        "6A 04 8D 44 24 0C 50 6A 00 68 ? ? ? ? 8D 4C 24 1C C7 44 24 18 ? ? ? ? E8 9C 26 BC FF "
        "F3 0F 7E 44 24 0C 83 EC 10 8B C4 66 0F D6 00 C7 44 24 28 ? ? ? ? F3 0F 7E 44 24 24 "
        "8B CE 66 0F D6 40 08 E8 A3 FD FF FF 6A 00 B9 ? ? ? ? E8 D7 E7 FF FF 5E 83 C4 28 C3");
    if (pattern.size() != 1)
        return;

    static const auto afterFirstSync = reinterpret_cast<uintptr_t>(pattern.get_first(0xA5));
    static const auto afterSecondSync = reinterpret_cast<uintptr_t>(pattern.get_first(0xF9));
    static std::array hooks{
        safetyhook::create_mid(pattern.get_first(0x5B), [](SafetyHookContext& regs)
        {
            // Keep the existing comparison (including framelimit's hook). Its AL
            // result belongs to this invocation, not a shared loading-state flag.
            // The first constructor's Src at ESP+4 is now dead; the two cdecl
            // arguments are still on the stack here, making that slot ESP+0xC.
            *reinterpret_cast<uint8_t*>(regs.esp + 0xC) = static_cast<uint8_t>(regs.eax);
        }, SafetyHookMid::StartDisabled),
        safetyhook::create_mid(pattern.get_first(0x99), [](SafetyHookContext& regs)
        {
            // Skip the argument push, ECX setup and call together, so no stack
            // adjustment is needed. Callback changes still execute both waits.
            if (*reinterpret_cast<uint8_t*>(regs.esp + 4))
                regs.eip = afterFirstSync;
        }, SafetyHookMid::StartDisabled),
        safetyhook::create_mid(pattern.get_first(0xED), [](SafetyHookContext& regs)
        {
            // The callback setter still runs and uses different stack locals.
            if (*reinterpret_cast<uint8_t*>(regs.esp + 4))
                regs.eip = afterSecondSync;
        }, SafetyHookMid::StartDisabled)
    };

    // Install at startup as a group; failed creation/enabling leaves no fix active.
    for (auto& hook : hooks)
    {
        if (!hook || !hook.enable())
        {
            for (auto& installed : hooks)
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
            FixRepeatedLoadingSynchronization();

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
            IATHook::Replace(GetModuleHandleA(NULL), "kernel32.DLL", std::forward_as_tuple("Sleep", FusionSleep));
        };
    }
} LoadingDelays;