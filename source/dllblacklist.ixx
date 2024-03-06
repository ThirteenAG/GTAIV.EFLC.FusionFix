module;

#include <common.hxx>
#include <shellapi.h>
#include <Commctrl.h>
#pragma comment(lib,"Comctl32.lib")
#pragma comment(linker,"\"/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

export module dllblacklist;

import common;
import comvars;

std::vector<std::wstring> dlllist = {
    // These A-Volute DLLs shipped with various audio driver packages improperly handle
    // D3D9 exclusive fullscreen in a way that causes CreateDeviceEx() to deadlock.
    // https://github.com/moonlight-stream/moonlight-qt/issues/102
    L"NahimicOSD.dll", // ASUS Sonic Radar 3
    L"SSAudioOSD.dll", // SteelSeries headsets
    L"SS2OSD.dll", // ASUS Sonic Studio 2
    L"Nahimic2OSD.dll",
    L"NahimicMSIOSD.dll",
    L"nhAsusPhoebusOSD.dll", // ASUS Phoebus
    L"MirillisActionVulkanLayer.dll",
};

typedef void(WINAPI* LdrLoadDllFunc) (IN PWCHAR PathToFile OPTIONAL, IN PULONG Flags OPTIONAL, IN PUNICODE_STRING ModuleFileName, OUT HMODULE* ModuleHandle);
static SafetyHookInline realLdrLoadDll = {};
static void WINAPI LdrLoadDllHook(IN PWCHAR PathToFile OPTIONAL, IN PULONG Flags OPTIONAL, IN PUNICODE_STRING ModuleFileName, OUT HMODULE* ModuleHandle)
{
    if (ModuleFileName->Buffer)
    {
        std::wstring fileName(ModuleFileName->Buffer, ModuleFileName->Length / sizeof(WCHAR));
        if (std::any_of(std::begin(dlllist), std::end(dlllist), [&fileName](auto& i) {
            std::wstring str1(fileName); std::wstring str2(i);
            std::transform(str1.begin(), str1.end(), str1.begin(), ::tolower);
            std::transform(str2.begin(), str2.end(), str2.begin(), ::tolower);
            return str1.ends_with(str2);
        }))
        {
            return SetLastError(ERROR_DLL_INIT_FAILED);
        }
    }

    return realLdrLoadDll.unsafe_stdcall(PathToFile, Flags, ModuleFileName, ModuleHandle);
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

export void CompatibilityWarnings()
{
    auto bUAL = IsUALPresent();
    std::filesystem::path dllName;
    auto it = std::find_if(std::begin(dlllist), std::end(dlllist), [](auto& i) { return GetModuleHandleW(i.c_str()); });

    if (it == std::end(dlllist))
        if (GetModuleHandleW(L"xlive"))
            dllName = L"xlive";
        else
            return;
    else
        dllName = *it;

    TASKDIALOGCONFIG tdc = { sizeof(TASKDIALOGCONFIG) };
    int nClickedBtn;
    BOOL bCheckboxChecked;
    LPCWSTR szTitle = L"", szHeader = L"", szContent = L"";
    TASKDIALOG_BUTTON aCustomButtons[] = { { 1000, L"Close the program" }, { 1001, L"Continue" } };

    if (!bUAL)
    {
        szTitle = L"GTAIV.EFLC.FusionFix",
        szHeader = L"You are running GTA IV The Complete Edition Fusion Fix with an incompatible version of ASI Loader",
        szContent = L"It requires the latest version of " \
        L"<a href=\"https://github.com/ThirteenAG/Ultimate-ASI-Loader/releases/latest\">Ultimate ASI Loader</a>\n\n" \
        L"<a href=\"https://github.com/ThirteenAG/Ultimate-ASI-Loader/releases/latest\">https://github.com/ThirteenAG/Ultimate-ASI-Loader/releases/latest</a>";
    }
    else if (iequals(dllName.stem().wstring(), L"xlive"))
    {
        if (IsModuleUAL(GetModuleHandleW(L"xlive")))
            return;

        szTitle = L"GTAIV.EFLC.FusionFix",
            szHeader = L"You are running GTA IV The Complete Edition Fusion Fix in backwards compatibility mode.",
            szContent = L"It requires the latest version of " \
            L"<a href=\"https://github.com/ThirteenAG/Ultimate-ASI-Loader/releases/latest\">Ultimate ASI Loader</a>" \
            L" as xlive.dll and " \
            L"<a href=\"https://github.com/GTAmodding/XLivelessAddon/releases/tag/latest\">XLivelessAddon</a>.";
    }
    else
    {
        static auto warning = std::wstring(dllName) + L" is injected into " + GetExeModuleName().wstring() + L" process." \
        L" It is likely the game will hang or crash on startup with Fusion Fix installed.";
        szTitle = L"GTAIV.EFLC.FusionFix",
        szHeader = L"External software injected faulty modules into GTA IV The Complete Edition.",
        szContent = warning.c_str();
    }
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
    if (nClickedBtn != aCustomButtons[1].nButtonID)
        TerminateProcess(GetCurrentProcess(), 0);
}

class DLLBlacklist
{
public:
    DLLBlacklist()
    {
        LdrLoadDllFunc baseLdrLoadDll = (LdrLoadDllFunc)GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "LdrLoadDll");
        if (baseLdrLoadDll == NULL)
            return;

        realLdrLoadDll = safetyhook::create_inline(baseLdrLoadDll, LdrLoadDllHook);
    }
} DLLBlacklist;