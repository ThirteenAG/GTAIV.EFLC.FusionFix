module;

#include <common.hxx>
#include <shellapi.h>
#include <Commctrl.h>
#pragma comment(lib,"Comctl32.lib")
#pragma comment(linker,"\"/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

export module compat;

import common;
import comvars;

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
    TASKDIALOGCONFIG tdc = { sizeof(TASKDIALOGCONFIG) };
    int nClickedBtn;
    BOOL bCheckboxChecked;
    LPCWSTR szTitle = L"GTAIV.EFLC.FusionFix", szHeader = L"", szContent = L"";
    TASKDIALOG_BUTTON aCustomButtons[] = { { 1000, L"Close the program" }, { 1001, L"Continue" } };

    if (GetModuleHandleW(L"d3dx9_43.dll") == NULL)
    {
        szHeader = L"You are missing a DirectX 9 component.";
        szContent = L"It requires the latest version of " \
            L"<a href=\"https://www.microsoft.com/en-us/download/details.aspx?id=35\">DirectX End-User Runtimes</a>\n\n" \
            L"<a href=\"https://www.microsoft.com/en-us/download/details.aspx?id=35\">https://www.microsoft.com/en-us/download/details.aspx?id=35</a>";
    }
    else if (!IsUALPresent())
    {
        szHeader = L"You are running GTA IV The Complete Edition Fusion Fix with an incompatible version of ASI Loader";
        szContent = L"It requires the latest version of " \
            L"<a href=\"https://github.com/ThirteenAG/Ultimate-ASI-Loader/releases/latest\">Ultimate ASI Loader</a>\n\n" \
            L"<a href=\"https://github.com/ThirteenAG/Ultimate-ASI-Loader/releases/latest\">https://github.com/ThirteenAG/Ultimate-ASI-Loader/releases/latest</a>";
    }
    else if (GetModuleHandleW(L"xlive"))
    {
        if (IsModuleUAL(GetModuleHandleW(L"xlive")))
            return;

        szHeader = L"You are running GTA IV The Complete Edition Fusion Fix in backwards compatibility mode.";
        szContent = L"It requires the latest version of " \
            L"<a href=\"https://github.com/ThirteenAG/GTAIV.EFLC.FusionFix/releases/latest/download/GTAIV.EFLC.FusionFixLegacyAddon.zip\">Fusion Fix Legacy Addon</a>.";
    }
    else
        return;

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
