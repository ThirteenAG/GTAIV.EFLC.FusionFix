#include <common.hxx>
#include <shellapi.h>
#include <Commctrl.h>
#pragma comment(lib,"Comctl32.lib")
#pragma comment(linker,"\"/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

import common;
import comvars;

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

    static auto futures = FusionFix::onInitEventAsync().executeAllAsync();

    FusionFix::onGameInitEvent() += []()
    {
        for (auto& f : futures.get())
            f.wait();
        futures.get().clear();
    };

    FusionFix::onInitEvent().executeAll();
}

HRESULT CALLBACK TaskDialogCallbackProc(HWND hwnd, UINT uNotification, WPARAM wParam, LPARAM lParam, LONG_PTR dwRefData)
{
    switch (uNotification)
    {
    case TDN_HYPERLINK_CLICKED:
        ShellExecuteW(hwnd, L"open", (LPCWSTR)lParam, NULL, NULL, SW_SHOW);
        break;
    }

    return S_OK;
}

void XliveCompat()
{
    if (IsModuleUAL(GetModuleHandleW(L"xlive")))
        return;

    TASKDIALOGCONFIG tdc = { sizeof(TASKDIALOGCONFIG) };
    int nClickedBtn;
    BOOL bCheckboxChecked;
    LPCWSTR 
        szTitle = L"GTAIV.EFLC.FusionFix",
        szHeader = L"You are running GTA IV The Complete Edition Fusion Fix in backwards compatibility mode.",
        szContent = L"It requires the latest version of " \
        L"<a href=\"https://github.com/ThirteenAG/Ultimate-ASI-Loader/releases/latest\">Ultimate ASI Loader</a>" \
        L" as xlive.dll and " \
        L"<a href=\"https://github.com/GTAmodding/XLivelessAddon/releases/tag/latest\">XLivelessAddon</a>.";
    TASKDIALOG_BUTTON aCustomButtons[] = { { 1000, L"Close the program" } };
    
    tdc.hwndParent = gWnd;
    tdc.dwFlags = TDF_USE_COMMAND_LINKS | TDF_ENABLE_HYPERLINKS | TDF_SIZE_TO_CONTENT | TDF_CAN_BE_MINIMIZED;
    tdc.pButtons = aCustomButtons;
    tdc.cButtons = _countof(aCustomButtons);
    tdc.pszWindowTitle = szTitle;
    tdc.pszMainIcon = TD_INFORMATION_ICON;
    tdc.pszMainInstruction = szHeader;
    tdc.pszContent = szContent;
    tdc.pfCallback = TaskDialogCallbackProc;
    tdc.lpCallbackData = 0;
    
    auto hr = TaskDialogIndirect(&tdc, &nClickedBtn, NULL, &bCheckboxChecked);
    TerminateProcess(GetCurrentProcess(), 0);
}

void UALCompat()
{
    if (IsUALPresent())
        return;

    TASKDIALOGCONFIG tdc = { sizeof(TASKDIALOGCONFIG) };
    int nClickedBtn;
    BOOL bCheckboxChecked;
    LPCWSTR
        szTitle = L"GTAIV.EFLC.FusionFix",
        szHeader = L"You are running GTA IV The Complete Edition Fusion Fix with an incompatible version of ASI Loader",
        szContent = L"It requires the latest version of " \
        L"<a href=\"https://github.com/ThirteenAG/Ultimate-ASI-Loader/releases/latest\">Ultimate ASI Loader</a>\n\n" \
        L"<a href=\"https://github.com/ThirteenAG/Ultimate-ASI-Loader/releases/latest\">https://github.com/ThirteenAG/Ultimate-ASI-Loader/releases/latest</a>";
    TASKDIALOG_BUTTON aCustomButtons[] = { { 1000, L"Close the program" } };
    
    tdc.hwndParent = gWnd;
    tdc.dwFlags = TDF_USE_COMMAND_LINKS | TDF_ENABLE_HYPERLINKS | TDF_SIZE_TO_CONTENT | TDF_CAN_BE_MINIMIZED;
    tdc.pButtons = aCustomButtons;
    tdc.cButtons = _countof(aCustomButtons);
    tdc.pszWindowTitle = szTitle;
    tdc.pszMainIcon = TD_INFORMATION_ICON;
    tdc.pszMainInstruction = szHeader;
    tdc.pszContent = szContent;
    tdc.pfCallback = TaskDialogCallbackProc;
    tdc.lpCallbackData = 0;
    
    auto hr = TaskDialogIndirect(&tdc, &nClickedBtn, NULL, &bCheckboxChecked);
    TerminateProcess(GetCurrentProcess(), 0);
}

extern "C"
{
    void __declspec(dllexport) InitializeASI()
    {
        std::call_once(CallbackHandler::flag, []()
        {
            CallbackHandler::RegisterCallback(Init, hook::pattern("F3 0F 10 44 24 ? F3 0F 59 05 ? ? ? ? EB ? E8"));
            CallbackHandler::RegisterCallback(L"xlive.dll", XliveCompat);
            UALCompat();
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
