module;

#include <common.hxx>

export module loadingdelays;

import common;
import comvars;
import framelimit;

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
            CIniReader iniReader("");

            // [MISC]
            auto bSkipLoadingSpinner = iniReader.ReadInteger("MISC", "SkipLoadingSpinner", 1) != 0;

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