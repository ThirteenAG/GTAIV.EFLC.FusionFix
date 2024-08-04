#include <common.hxx>

import common;
import comvars;
import dllblacklist;
import fusiondxhook;

injector::hook_back<void(*)()> hbCGameProcess;
void CGameProcessHook()
{
    static std::once_flag of;
    std::call_once(of, []()
    {
        FusionFix::onGameInitEvent().executeAll();
    });

    if (CTimer::m_UserPause && CTimer::m_CodePause)
    {
        static auto oldMenuState = 0;

        if (!*CTimer::m_UserPause && !*CTimer::m_CodePause)
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

    return hbCGameProcess.fun();
}

void Init()
{
    auto pattern = hook::pattern("E8 ? ? ? ? E8 ? ? ? ? E8 ? ? ? ? B9 ? ? ? ? E8 ? ? ? ? E8 ? ? ? ? E8 ? ? ? ? E8 ? ? ? ? B9");
    hbCGameProcess.fun = injector::MakeCALL(pattern.get_first(0), CGameProcessHook, true).get();

    pattern = find_pattern("A1 ? ? ? ? 50 8B 08 FF 51 40", "A1 ? ? ? ? 68 ? ? ? ? 8B 08 50 FF 51 40 8B F8");
    if (!pattern.empty())
    {
        static auto Direct3DDevice = *pattern.get_first<LPDIRECT3DDEVICE9*>(1);
        struct AuxBeforeResetHook
        {
            void operator()(injector::reg_pack& regs)
            {
                FusionFix::onBeforeReset().executeAll();
                *(LPDIRECT3DDEVICE9*)&regs.eax = *Direct3DDevice;
            }
        };
        
        injector::MakeInline<AuxBeforeResetHook>(pattern.get_first(0));
        pattern = hook::pattern("A1 ? ? ? ? C7 05 ? ? ? ? ? ? ? ? 8B 08 68");
        injector::MakeInline<AuxBeforeResetHook>(pattern.get_first(0));
        pattern = hook::pattern("A1 ? ? ? ? 68 ? ? ? ? 8B 08 50 FF 51 40 A1");
        injector::MakeInline<AuxBeforeResetHook>(pattern.get_first(0));

        pattern = hook::pattern("A1 ? ? ? ? 50 8B 08 FF 91 ? ? ? ? 8B 3D");
        struct AuxEndSceneHook
        {
            void operator()(injector::reg_pack& regs)
            {
                *(LPDIRECT3DDEVICE9*)&regs.eax = *Direct3DDevice;
                FusionFix::onEndScene().executeAll();
            }
        }; injector::MakeInline<AuxEndSceneHook>(pattern.get_first(0));
    }
    else
    {
        pattern = hook::pattern("8B 08 8B 51 40 68 ? ? ? ? 50 FF D2 8B");
        struct AuxBeforeResetHook
        {
            void operator()(injector::reg_pack& regs)
            {
                FusionFix::onBeforeReset().executeAll();
                regs.ecx = *(uint32_t*)regs.eax;
                regs.edx = *(uint32_t*)(regs.ecx + 0x40);
            }
        };
        
        injector::MakeInline<AuxBeforeResetHook>(pattern.get_first(0));
        pattern = hook::pattern("8B 08 8B 51 40 68 ? ? ? ? 50 FF D2 85 C0");
        injector::MakeInline<AuxBeforeResetHook>(pattern.get_first(0));
        pattern = hook::pattern("8B 08 8B 51 40 68 ? ? ? ? 50 FF D2 A1 ? ? ? ? 8B 0D");
        injector::MakeInline<AuxBeforeResetHook>(pattern.get_first(0));
        pattern = hook::pattern("8B 08 8B 51 40 68 ? ? ? ? 50 FF D2 A1 ? ? ? ? 85 C0");
        injector::MakeInline<AuxBeforeResetHook>(pattern.get_first(0));

        pattern = hook::pattern("A1 ? ? ? ? 8B 10 50");
        static auto Direct3DDevice = *pattern.get_first<LPDIRECT3DDEVICE9*>(1);
        struct AuxEndSceneHook
        {
            void operator()(injector::reg_pack& regs)
            {
                *(LPDIRECT3DDEVICE9*)&regs.eax = *Direct3DDevice;
                FusionFix::onEndScene().executeAll();
            }
        }; injector::MakeInline<AuxEndSceneHook>(pattern.get_first(0));
    }

    static auto futures = FusionFix::onInitEventAsync().executeAllAsync();

    FusionFix::onGameInitEvent() += []()
    {
        for (auto& f : futures.get())
            f.wait();
        futures.get().clear();
    };

    FusionFix::onInitEvent().executeAll();
}

extern "C"
{
    void __declspec(dllexport) InitializeASI()
    {
        std::call_once(CallbackHandler::flag, []()
        {
            CompatibilityWarnings();
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
