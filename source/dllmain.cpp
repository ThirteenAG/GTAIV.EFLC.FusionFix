#include <common.hxx>

import common;
import comvars;
import dllblacklist;

injector::hook_back<void(__fastcall*)(void*, void*, int, int, int)> hbsub_92E7C0;
void __fastcall sub_92E7C0Hook(void* _this, void* edx, int a2, int a3, int a4)
{
    FusionFix::onBeforePostFX().executeAll();
    hbsub_92E7C0.fun(_this, edx, a2, a3, a4);
    FusionFix::onAfterPostFX().executeAll();
}

injector::hook_back<void(*)()> hbsub_8C4480;
void __cdecl sub_8C4480Hook()
{
    static std::once_flag of;
    std::call_once(of, []()
    {
        FusionFix::onAfterUALRestoredIATEvent().executeAll();
    });
    return hbsub_8C4480.fun();
}

injector::hook_back<void(*)()> hbCGameProcess;
void CGameProcessHook()
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

    return hbCGameProcess.fun();
}

void Init()
{
    auto pattern = hook::pattern("E8 ? ? ? ? E8 ? ? ? ? E8 ? ? ? ? B9 ? ? ? ? E8 ? ? ? ? E8 ? ? ? ? E8 ? ? ? ? E8 ? ? ? ? B9");
    hbCGameProcess.fun = injector::MakeCALL(pattern.get_first(0), CGameProcessHook, true).get();

    pattern = hook::pattern("A1 ? ? ? ? 50 8B 08 FF 51 40");
    if (!pattern.empty())
    {
        static auto Direct3DDevice = *pattern.get_first<LPDIRECT3DDEVICE9*>(1);
        struct AuxBeforeResetHook
        {
            void operator()(injector::reg_pack& regs)
            {
                *(LPDIRECT3DDEVICE9*)&regs.eax = *Direct3DDevice;
                FusionFix::onBeforeReset().executeAll();
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
                bMainEndScene = true;
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
                regs.ecx = *(uint32_t*)regs.eax;
                regs.edx = *(uint32_t*)(regs.ecx + 0x40);
                FusionFix::onBeforeReset().executeAll();
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
                bMainEndScene = true;
            }
        }; injector::MakeInline<AuxEndSceneHook>(pattern.get_first(0));
    }

    pattern = find_pattern("E8 ? ? ? ? E8 ? ? ? ? E8 ? ? ? ? 8D 54 24 08", "E8 ? ? ? ? E8 ? ? ? ? 68 ? ? ? ? 6A 00 6A 00");
    hbsub_8C4480.fun = injector::MakeCALL(pattern.get_first(0), sub_8C4480Hook, true).get();

    pattern = find_pattern("E8 ? ? ? ? 6A 0A FF B7", "E8 ? ? ? ? 8B 8E ? ? ? ? 8B 56 10");
    hbsub_92E7C0.fun = injector::MakeCALL(pattern.get_first(0), sub_92E7C0Hook, true).get();

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
