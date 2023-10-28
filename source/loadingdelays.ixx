module;

#include <common.hxx>

export module loadingdelays;

import common;
import framelimit;

void WINAPI FusionSleep(DWORD dwMilliseconds)
{
    if (bLoadscreenShown && !*bLoadscreenShown && !bLoadingShown)
        return Sleep(dwMilliseconds);
    else
        return Sleep(0);
}

injector::hook_back<void(*)()> hbsub_8C4480;
void __cdecl sub_8C4480Hook()
{
    static std::once_flag of;
    std::call_once(of, []()
    {
        IATHook::Replace(GetModuleHandleA(NULL), "kernel32.DLL",
            std::forward_as_tuple("Sleep", FusionSleep)
        );
    });

    return hbsub_8C4480.fun();
}

class LoadingDelays
{
public:
    LoadingDelays()
    {
        FusionFix::onInitEvent() += []()
        {
            auto pattern = find_pattern("E8 ? ? ? ? E8 ? ? ? ? E8 ? ? ? ? 8D 54 24 08", "E8 ? ? ? ? E8 ? ? ? ? 68 ? ? ? ? 6A 00 6A 00");
            hbsub_8C4480.fun = injector::MakeCALL(pattern.get_first(0), sub_8C4480Hook, true).get();
        };
    }
} LoadingDelays;