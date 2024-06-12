module;

#include <common.hxx>

export module loadingdelays;

import common;
import framelimit;
import comvars;

//bool bRestoreSleep = false;
void WINAPI FusionSleep(DWORD dwMilliseconds)
{
    //if (!bRestoreSleep)
    {
        auto bMenuActive = CMenuManager::m_MenuActive && *CMenuManager::m_MenuActive;
        auto bLoadscreenActive = (CMenuManager::bLoadscreenShown && *CMenuManager::bLoadscreenShown) || bLoadingShown;

        if (!bMenuActive && bLoadscreenActive)
            return Sleep(0);
    }
    return Sleep(dwMilliseconds);
}

class LoadingDelays
{
public:
    LoadingDelays()
    {
        FusionFix::onInitEvent() += []()
        {
            auto pattern = find_pattern("68 ? ? ? ? FF 15 ? ? ? ? 83 3D ? ? ? ? ? 75 05", "68 ? ? ? ? FF 15 ? ? ? ? 39 1D");
            injector::WriteMemory(pattern.get_first(1), 0, true);

            pattern = find_pattern("FF 35 ? ? ? ? EB 06 FF 35 ? ? ? ? B9 ? ? ? ? E8 ? ? ? ? C6 44 24", "8B 0D ? ? ? ? 51 EB 07 8B 15 ? ? ? ? 52 B9 ? ? ? ? E8 ? ? ? ? C6 44 24");
            injector::MakeJMP(pattern.get_first(), hook::get_pattern("6A FF 6A 01 6A 00 6A 01 6A 01 6A 00 6A 01 6A 01 6A 00 6A 00 6A 00 6A 00 6A 00 6A 00 B9 ? ? ? ? C6 46 04 00"));
        };

        FusionFix::onInitEvent() += []()
        {
            IATHook::Replace(GetModuleHandleA(NULL), "kernel32.DLL",
                std::forward_as_tuple("Sleep", FusionSleep)
            );
        };

        //FusionFix::onGameInitEvent() += []()
        //{
        //    bRestoreSleep = true;
        //};
    }
} LoadingDelays;