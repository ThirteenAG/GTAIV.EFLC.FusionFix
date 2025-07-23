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
        ShowWindow(gWnd, SW_SHOW);
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

BOOL WINAPI SetWindowPos_Hook(HWND hWnd, HWND hWndInsertAfter, int X, int Y, int cx, int cy, UINT uFlags)
{
    if (*rage::grcWindow::ms_bOnTop)
        hWndInsertAfter = HWND_TOPMOST;
    else
        hWndInsertAfter = HWND_NOTOPMOST;
    BOOL res = SetWindowPos(hWnd, hWndInsertAfter, X, Y, cx, cy, uFlags);
    SwitchWindowStyle();
    return res;
}

LONG WINAPI SetWindowLongA_Hook(HWND hWnd, int nIndex, LONG dwNewLong)
{
    if (!*rage::grcWindow::ms_bOnTop)
        if (nIndex == GWL_EXSTYLE)
            dwNewLong &= ~WS_EX_TOPMOST;

    return SetWindowLongA(hWnd, nIndex, dwNewLong);
}

injector::hook_back<void(__cdecl*)(char)> hbsub_7870A0;
void __cdecl sub_69F0C0(char a1)
{
    if (*rage::grcWindow::ms_bWindowed)
    {
        if (*rage::grcWindow::ms_bFocusLost)
            return;
    }
    return hbsub_7870A0.fun(a1);
}

class Windowed
{
public:
    Windowed()
    {
        FusionFix::onInitEvent() += []()
        {
            auto pattern = hook::pattern("33 FF 85 C0 0F 45 F8");
            if (!pattern.empty())
            {
                injector::MakeNOP(pattern.get_first(), 2);
                static auto g_cmdarg_windowed_hook = safetyhook::create_mid(pattern.get_first(),
                [](SafetyHookContext& ctx)
                {
                    ctx.edi = FusionFixSettings.Get("PREF_WINDOWED");
                });

                pattern = hook::pattern("89 3D ? ? ? ? A3 ? ? ? ? 8D 44 24 17");
                static auto g_cmdarg_windowed_hook2 = safetyhook::create_mid(pattern.get_first(),
                [](SafetyHookContext& ctx)
                {
                    FusionFixSettings.Set("PREF_WINDOWED", !ctx.eax);
                });
            }
            else
            {
                pattern = hook::pattern("33 F6 3B C7 0F 45 F0");
                injector::MakeNOP(pattern.get_first(), 2);
                static auto g_cmdarg_windowed_hook = safetyhook::create_mid(pattern.get_first(),
                [](SafetyHookContext& ctx)
                {
                    ctx.esi = FusionFixSettings.Get("PREF_WINDOWED");
                });

                pattern = hook::pattern("89 35 ? ? ? ? A3 ? ? ? ? 8D 44 24 17");
                static auto g_cmdarg_windowed_hook2 = safetyhook::create_mid(pattern.get_first(),
                [](SafetyHookContext& ctx)
                {
                    FusionFixSettings.Set("PREF_WINDOWED", !ctx.eax);
                });
            }

            IATHook::Replace(GetModuleHandleA(NULL), "USER32.DLL", 
                std::forward_as_tuple("CreateWindowExA", CreateWindowExA_Hook),
                std::forward_as_tuple("CreateWindowExW", CreateWindowExW_Hook),
                std::forward_as_tuple("MoveWindow", MoveWindow_Hook),
                std::forward_as_tuple("AdjustWindowRect", AdjustWindowRect_Hook),
                std::forward_as_tuple("SetWindowPos", SetWindowPos_Hook),
                std::forward_as_tuple("SetRect", SetRect_Hook),
                std::forward_as_tuple("SetWindowLongA", SetWindowLongA_Hook)
            );

            FusionFixSettings.SetCallback("PREF_BORDERLESS", [](int32_t value) {
                SwitchWindowStyle();
            });

            static auto bSkipWindowedCallback1 = false;
            static auto bSkipWindowedCallback2 = false;
            FusionFixSettings.SetCallback("PREF_WINDOWED", [](int32_t value) {
                if (!bSkipWindowedCallback1)
                {
                    if (*rage::grcWindow::ms_bWindowed != !!value)
                    {
                        bSkipWindowedCallback2 = true;
                        SendMessageA(gWnd, 260, 13, 0);
                    }
                }
                bSkipWindowedCallback1 = false;
            });

            pattern = find_pattern("C6 05 ? ? ? ? ? FF 75 14", "C6 05 ? ? ? ? ? 0F 94 05 ? ? ? ? FF 75 14", "C6 05 ? ? ? ? ? 8B 54 24 1C", "C6 05 ? ? ? ? ? FF 15 ? ? ? ? 5F");
            static auto AltEnterHandlerHook = safetyhook::create_mid(pattern.get_first(),
            [](SafetyHookContext& ctx)
            {
                bSkipWindowedCallback1 = true;
                if (!bSkipWindowedCallback2)
                    FusionFixSettings.Set("PREF_WINDOWED", !FusionFixSettings.Get("PREF_WINDOWED"));
                bSkipWindowedCallback2 = false;
            });

            // Do not process input on focus loss
            pattern = find_pattern("E8 ? ? ? ? A1 ? ? ? ? A3 ? ? ? ? A1 ? ? ? ? 83 C4 04", "E8 ? ? ? ? 8B 0D ? ? ? ? 8B 15 ? ? ? ? 83 C4 04 83 3D");
            hbsub_7870A0.fun = injector::MakeCALL(pattern.get_first(), sub_69F0C0).get();

            FusionFixSettings.SetCallback("PREF_BLOCKONLOSTFOCUS", [](int32_t value) {
                *rage::grcDevice::ms_bNoBlockOnLostFocus = value;
            });

            pattern = find_pattern("B9 ? ? ? ? 66 23 C1 68", "66 25 FF 03 0F B7 C8");
            static auto NoBlockOnLostFocusHook = safetyhook::create_mid(pattern.get_first(),
            [](SafetyHookContext& ctx)
            {
                if (!*rage::grcDevice::ms_bNoBlockOnLostFocus)
                {
                    FusionFixSettings.Set("PREF_BLOCKONLOSTFOCUS", *rage::grcDevice::ms_bNoBlockOnLostFocus);
                    return;
                }
                *rage::grcDevice::ms_bNoBlockOnLostFocus = FusionFixSettings.Get("PREF_BLOCKONLOSTFOCUS");
            });
        };
    }
} Windowed;