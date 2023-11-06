module;

#include <common.hxx>

export module loadingdelays;

import common;
import framelimit;
import comvars;

void WINAPI FusionSleep(DWORD dwMilliseconds)
{
    if ((!CMenuManager__m_MenuActive || *CMenuManager__m_MenuActive) || (bLoadscreenShown && !*bLoadscreenShown && !bLoadingShown))
        return Sleep(dwMilliseconds);
    else
        return Sleep(0);
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