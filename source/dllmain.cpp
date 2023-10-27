#include <common.hxx>
import common;
import comvars;

injector::hook_back<void(__cdecl*)(int)> hbCGameProcess;
void __cdecl CGameProcessHook(int a1)
{
    static std::once_flag of;
    std::call_once(of, []()
    {
        FusionFix::onGameInitEvent().executeAll();
    });

    if (CTimer__m_UserPause && CTimer__m_CodePause)
    {
        static auto oldMenuState = 0;

        if (!*CTimer__m_UserPause && !*CTimer__m_CodePause)
        {
            uint32_t curMenuState = false;
            if (curMenuState != oldMenuState) {
                FusionFix::onMenuExitEvent().executeAll();
            }
            oldMenuState = curMenuState;
            FusionFix::onGameProcessEvent().executeAll();
        }
        else
        {
            uint32_t curMenuState = true;
            if (curMenuState != oldMenuState) {
                FusionFix::onMenuEnterEvent().executeAll();
            }
            oldMenuState = curMenuState;
            FusionFix::onMenuDrawingEvent().executeAll();
        }
    }

    return hbCGameProcess.fun(a1);
}

void Init()
{
    auto pattern = hook::pattern("E8 ? ? ? ? E8 ? ? ? ? E8 ? ? ? ? B9 ? ? ? ? E8 ? ? ? ? E8 ? ? ? ? E8 ? ? ? ? E8 ? ? ? ? B9");
    hbCGameProcess.fun = injector::MakeCALL(pattern.get_first(0), CGameProcessHook, true).get();

    FusionFix::onInitEvent().executeAll();
}

extern "C"
{
    void __declspec(dllexport) InitializeASI()
    {
        std::call_once(CallbackHandler::flag, []()
        {
            CallbackHandler::RegisterCallback(Init, hook::pattern("F3 0F 10 44 24 ? F3 0F 59 05 ? ? ? ? EB ? E8"));
        });
    }
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID lpReserved)
{
    if (reason == DLL_PROCESS_ATTACH)
    {
        if (!IsUALPresent()) { InitializeASI(); }
    }
    if (reason == DLL_PROCESS_DETACH)
    {
        FusionFix::onShutdownEvent().executeAll();
    }
    return TRUE;
}
