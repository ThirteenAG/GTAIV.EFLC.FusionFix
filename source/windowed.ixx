module;

#include <common.hxx>

export module windowed;

import common;
import comvars;
import settings;

BOOL WINAPI SetRect_Hook(LPRECT lprc, int xLeft, int yTop, int xRight, int yBottom)
{
    gRect = { xLeft, yTop, xRight, yBottom };
    return SetRect(lprc, xLeft, yTop, xRight, yBottom);
}

BOOL WINAPI MoveWindow_Hook(HWND hWnd, int X, int Y, int nWidth, int nHeight, BOOL bRepaint)
{
    RECT rect = { X, Y, nWidth, nHeight };
    HMONITOR monitor = MonitorFromWindow(hWnd, MONITOR_DEFAULTTONEAREST);
    MONITORINFO info = {};
    info.cbSize = sizeof(MONITORINFO);
    GetMonitorInfo(monitor, &info);
    int32_t DesktopResW = info.rcMonitor.right - info.rcMonitor.left;
    int32_t DesktopResH = info.rcMonitor.bottom - info.rcMonitor.top;
    if ((rect.right - rect.left >= DesktopResW) || (rect.bottom - rect.top >= DesktopResH))
        rect = gRect;
    rect.left = (LONG)(((float)DesktopResW / 2.0f) - ((float)rect.right / 2.0f));
    rect.top = (LONG)(((float)DesktopResH / 2.0f) - ((float)rect.bottom / 2.0f));
    return MoveWindow(hWnd, rect.left, rect.top, rect.right, rect.bottom, bRepaint);
}

void SwitchWindowStyle()
{
    if (gWnd)
    {
        RECT rect = gRect;
        LONG lStyle = GetWindowLong(gWnd, GWL_STYLE);
        if (FusionFixSettings("PREF_BORDERLESS"))
        {
            lStyle &= ~(WS_CAPTION | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_SYSMENU);
        }
        else
        {
            GetWindowRect(gWnd, &rect);
            lStyle |= (WS_CAPTION | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_SYSMENU);
        }
        AdjustWindowRect(&rect, lStyle, FALSE);
        SetWindowLong(gWnd, GWL_STYLE, lStyle);
        MoveWindow_Hook(gWnd, 0, 0, rect.right - rect.left, rect.bottom - rect.top, TRUE);
    }
}

HWND WINAPI CreateWindowExA_Hook(DWORD dwExStyle, LPCSTR lpClassName, LPCSTR lpWindowName, DWORD dwStyle, int X, int Y, int nWidth, int nHeight, HWND hWndParent, HMENU hMenu, HINSTANCE hInstance, LPVOID lpParam)
{
    gWnd = CreateWindowExA(dwExStyle, lpClassName, lpWindowName, 0, X, Y, nWidth, nHeight, hWndParent, hMenu, hInstance, lpParam);
    SwitchWindowStyle();
    return gWnd;
}

HWND WINAPI CreateWindowExW_Hook(DWORD dwExStyle, LPCWSTR lpClassName, LPCWSTR lpWindowName, DWORD dwStyle, int X, int Y, int nWidth, int nHeight, HWND hWndParent, HMENU hMenu, HINSTANCE hInstance, LPVOID lpParam)
{
    gWnd = CreateWindowExW(dwExStyle, lpClassName, lpWindowName, 0, X, Y, nWidth, nHeight, hWndParent, hMenu, hInstance, lpParam);
    SwitchWindowStyle();
    return gWnd;
}

BOOL WINAPI AdjustWindowRect_Hook(LPRECT lpRect, DWORD dwStyle, BOOL bMenu)
{
    if (FusionFixSettings("PREF_BORDERLESS"))
        dwStyle &= ~(WS_CAPTION | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_SYSMENU);
    else
        dwStyle |= (WS_CAPTION | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_SYSMENU);

    return AdjustWindowRect(lpRect, dwStyle, bMenu);
}

class Windowed
{
public:
    Windowed()
    {
        FusionFix::onInitEvent() += []()
        {
            IATHook::Replace(GetModuleHandleA(NULL), "USER32.DLL", 
                std::forward_as_tuple("CreateWindowExA", CreateWindowExA_Hook),
                std::forward_as_tuple("CreateWindowExW", CreateWindowExW_Hook),
                std::forward_as_tuple("MoveWindow", MoveWindow_Hook),
                std::forward_as_tuple("AdjustWindowRect", AdjustWindowRect_Hook),
                std::forward_as_tuple("SetRect", SetRect_Hook)
            );

            FusionFixSettings.SetCallback("PREF_BORDERLESS", [](int32_t value) {
                SwitchWindowStyle();
            });
        };
    }
} Windowed;