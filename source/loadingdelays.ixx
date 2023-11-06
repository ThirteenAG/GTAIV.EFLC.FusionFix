module;

#include <common.hxx>

export module loadingdelays;

import common;
import framelimit;
import comvars;

void WINAPI FusionSleep(DWORD dwMilliseconds)
{
    auto bMenuActive = CMenuManager__m_MenuActive && *CMenuManager__m_MenuActive;
    auto bLoadscreenActive = (bLoadscreenShown && *bLoadscreenShown) || bLoadingShown;

    if (!bMenuActive && bLoadscreenActive)
        return Sleep(0);

    return Sleep(dwMilliseconds);
}

class LoadingDelays
{
public:
    LoadingDelays()
    {
        FusionFix::onAfterUALRestoredIATEvent() += []()
        {
            IATHook::Replace(GetModuleHandleA(NULL), "kernel32.DLL",
                std::forward_as_tuple("Sleep", FusionSleep)
            );
        };
    }
} LoadingDelays;