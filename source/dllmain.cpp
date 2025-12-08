#include <common.hxx>

import common;
import comvars;
import compat;
import fusiondxhook;

injector::hook_back<void(*)()> hbCGameProcess;
void CGameProcessHook()
{
    if (CTimer::m_UserPause && CTimer::m_CodePause)
    {
        if (!*CTimer::m_UserPause && !*CTimer::m_CodePause)
        {
            static std::once_flag of;
            std::call_once(of, []() {
                FusionFix::onGameInitEvent().executeAll();
            });

            FusionFix::onGameProcessEvent().executeAll();
        }

        static bool oldMenuState = false;
        bool curMenuState = CMenuManager::m_MenuActive && *CMenuManager::m_MenuActive;
        if (curMenuState != oldMenuState)
        {
            if (curMenuState)
                FusionFix::onMenuEnterEvent().executeAll();
            else
                FusionFix::onMenuExitEvent().executeAll();
        }
        oldMenuState = curMenuState;
        if (curMenuState)
            FusionFix::onMenuDrawingEvent().executeAll();
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

    pattern = find_pattern("C6 05 ? ? ? ? ? 85 C0 74 02 FF D0 E8", "C6 05 ? ? ? ? ? 74 02 FF D0 E8");
    static auto ActivateAppHook = safetyhook::create_mid(pattern.get_first(0), [](SafetyHookContext& regs)
    {
        FusionFix::onActivateApp().executeAll(true);
    });

    pattern = find_pattern("C6 05 ? ? ? ? ? 85 C0 74 02 FF D0 80 3D", "C6 05 ? ? ? ? ? 74 02 FF D0 8B 54 24 10", "C6 05 ? ? ? ? ? 74 02 FF D0 80 3D");
    static auto DeactivateAppHook = safetyhook::create_mid(pattern.get_first(0), [](SafetyHookContext& regs)
    {
        FusionFix::onActivateApp().executeAll(false);
    });

    pattern = find_pattern("81 EC ? ? ? ? A1 ? ? ? ? 33 C4 89 84 24 ? ? ? ? 8B 84 24 ? ? ? ? 53 56 68", "81 EC ? ? ? ? A1 ? ? ? ? 33 C4 89 84 24 ? ? ? ? 8B 84 24 ? ? ? ? 53 55 56 68 ? ? ? ? 50 E8");
    static auto ReadGameConfigHook = safetyhook::create_mid(pattern.get_first(0), [](SafetyHookContext& regs)
    {
        FusionFix::onReadGameConfig().executeAll();
    });

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