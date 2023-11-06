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

            auto pattern = find_pattern("0F 84 ? ? ? ? 8B 46 08 33 C9", "80 7E 04 00 0F 84 ? ? ? ? 8B 4E 08");
            injector::MakeJMP(pattern.get_first(), hook::get_pattern("6A FF 6A 01 6A 00 6A 01 6A 01 6A 00 6A 01 6A 01 6A 00 6A 00 6A 00 6A 00 6A 00 6A 00 B9 ? ? ? ? C6 46 04 00"));
        };
    }
} LoadingDelays;