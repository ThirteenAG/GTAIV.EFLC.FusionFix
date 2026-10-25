module;

#include <common.hxx>
#include <upscaler/graphics_api.h>
#include <WindowStyle.hpp>
#include <Commctrl.h>
#include "OverlayInput.hpp"

export module windowed;

import common;
import comvars;
import settings;

// Retain the display before the runtime or game temporarily moves the window during a mode change.
HMONITOR modeSwitchMonitor = nullptr;

void RememberWindowMonitor()
{
    if (gWnd && !IsIconic(gWnd))
        modeSwitchMonitor = MonitorFromWindow(gWnd, MONITOR_DEFAULTTONEAREST);
}

BOOL WINAPI SetRect_Hook(LPRECT lprc, int xLeft, int yTop, int xRight, int yBottom)
{
    gRect = { xLeft, yTop, xRight, yBottom };
    return SetRect(lprc, xLeft, yTop, xRight, yBottom);
}

// The window for the game's client size (gRect: the resolution, from the SetRect before its AdjustWindowRect), on the
// window's monitor (FusionFixUtils include/WindowStyle.hpp): a bordered window at the monitor's resolution has its
// client area exactly on the monitor and the title bar and borders outside the screen (the game renders at the chosen
// resolution, never shrunk for the frame); a smaller one is centered; borderless has no frame. No rounded corners.
bool BorderlessFullscreen();

BOOL WINAPI MoveWindow_Hook(HWND hWnd, int X, int Y, int nWidth, int nHeight, BOOL bRepaint)
{
    LONG width = gRect.right - gRect.left;
    LONG height = gRect.bottom - gRect.top;
    if (width <= 0 || height <= 0)
    {
        // No client size yet: the size asked for, without the frame
        auto frame = WindowStyle::FrameOf(hWnd);
        width = nWidth - frame.Width();
        height = nHeight - frame.Height();
    }
    if (BorderlessFullscreen())
    {
        auto monitor = WindowStyle::MonitorRect(hWnd, modeSwitchMonitor);
        width = monitor.right - monitor.left;
        height = monitor.bottom - monitor.top;
    }
    WindowStyle::Place(hWnd, width, height, bRepaint, modeSwitchMonitor);
    return TRUE;
}

// MAIN.WindowMode: 0 fullscreen, 1 windowed, 2 borderless window. Fullscreen keeps the window style of the last window
// mode (Alt+Enter goes back to it), borderless until a window mode was chosen.
enum WindowMode { ModeFullscreen = 0, ModeWindow = 1, ModeBorderless = 2 };
int32_t lastWindowMode = ModeBorderless;

int32_t GetWindowMode()
{
    auto mode = FusionFixSettings.Get("PREF_WINDOWMODE");
    if (mode == ModeWindow || mode == ModeBorderless)
        lastWindowMode = mode;
    return mode;
}

bool IsWindowedMode()
{
    return GetWindowMode() != ModeFullscreen;
}

bool IsBorderless()
{
    GetWindowMode();
    return lastWindowMode == ModeBorderless;
}

// Fullscreen uses a borderless window when graphics_api.h disallows exclusive presentation: on an older D3D12
// runtime without private present interfaces, or when HDR keeps the driver's flip swap chain for the session.
// The game retains its fullscreen state, so Alt+Enter and the menu continue to select the usual window modes.
bool BorderlessFullscreen()
{
    return GetWindowMode() == ModeFullscreen && !FusionFixGraphicsApi::ExclusiveFullscreenAllowed();
}

void SwitchWindowStyle()
{
    if (gWnd)
    {
        LONG lStyle = GetWindowLong(gWnd, GWL_STYLE);
        if (IsBorderless() || BorderlessFullscreen())
            lStyle &= ~(WS_CAPTION | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_SYSMENU);
        else
            lStyle |= (WS_CAPTION | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_SYSMENU);
        SetWindowLong(gWnd, GWL_STYLE, lStyle);
        // Placed for the game's client size with the new style's frame (MoveWindow_Hook)
        RECT client{};
        GetClientRect(gWnd, &client);
        MoveWindow_Hook(gWnd, 0, 0, client.right, client.bottom, TRUE);
        ShowWindow(gWnd, SW_SHOW);
    }
}

// Refresh fullscreen placement after the device's presentation mode changes at Reset.
export void RefreshFullscreenWindow()
{
    if (!gWnd || GetWindowMode() != ModeFullscreen)
        return;
    if (BorderlessFullscreen())
        SwitchWindowStyle();
    else
        WindowStyle::Unplace(gWnd);
}

// A fullscreen window can stretch a lower-resolution back buffer over the desktop. Its physical
// size is not a request to change the game's render resolution, unlike a resized ordinary window.
LRESULT CALLBACK FullscreenWindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam, UINT_PTR, DWORD_PTR)
{
    if (message == WM_SIZE && BorderlessFullscreen())
        return 0;
    return DefSubclassProc(window, message, wParam, lParam);
}

HWND WINAPI CreateWindowExA_Hook(DWORD dwExStyle, LPCSTR lpClassName, LPCSTR lpWindowName, DWORD dwStyle, int X, int Y, int nWidth, int nHeight, HWND hWndParent, HMENU hMenu, HINSTANCE hInstance, LPVOID lpParam)
{
    gWnd = CreateWindowExA(dwExStyle, lpClassName, lpWindowName, 0, X, Y, nWidth, nHeight, hWndParent, hMenu, hInstance, lpParam);
    GTAIVOverlay::AttachWindow(gWnd);
    SetWindowSubclass(gWnd, FullscreenWindowProc, 0x4646574D, 0);
    SwitchWindowStyle();
    return gWnd;
}

HWND WINAPI CreateWindowExW_Hook(DWORD dwExStyle, LPCWSTR lpClassName, LPCWSTR lpWindowName, DWORD dwStyle, int X, int Y, int nWidth, int nHeight, HWND hWndParent, HMENU hMenu, HINSTANCE hInstance, LPVOID lpParam)
{
    gWnd = CreateWindowExW(dwExStyle, lpClassName, lpWindowName, 0, X, Y, nWidth, nHeight, hWndParent, hMenu, hInstance, lpParam);
    GTAIVOverlay::AttachWindow(gWnd);
    SetWindowSubclass(gWnd, FullscreenWindowProc, 0x4646574D, 0);
    SwitchWindowStyle();
    return gWnd;
}

BOOL WINAPI AdjustWindowRect_Hook(LPRECT lpRect, DWORD dwStyle, BOOL bMenu)
{
    if (IsBorderless() || BorderlessFullscreen())
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
    if (!modeSwitchMonitor)
        RememberWindowMonitor();
    BOOL res = SetWindowPos(hWnd, hWndInsertAfter, X, Y, cx, cy, uFlags);
    SwitchWindowStyle();
    modeSwitchMonitor = nullptr;
    return res;
}

LONG WINAPI SetWindowLongA_Hook(HWND hWnd, int nIndex, LONG dwNewLong)
{
    if (!*rage::grcWindow::ms_bOnTop)
        if (nIndex == GWL_EXSTYLE)
            dwNewLong &= ~WS_EX_TOPMOST;

    return SetWindowLongA(hWnd, nIndex, dwNewLong);
}

// The game's choice (command line, Alt+Enter): fullscreen, or the last window mode
void SetWindowedPref(bool windowed)
{
    GetWindowMode();
    FusionFixSettings.Set("PREF_WINDOWMODE", windowed ? lastWindowMode : ModeFullscreen);
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
                    ctx.edi = IsWindowedMode();
                });

                pattern = hook::pattern("89 3D ? ? ? ? A3 ? ? ? ? 8D 44 24 17");
                static auto g_cmdarg_windowed_hook2 = safetyhook::create_mid(pattern.get_first(),
                [](SafetyHookContext& ctx)
                {
                    SetWindowedPref(!ctx.eax);
                });
            }
            else
            {
                pattern = hook::pattern("33 F6 3B C7 0F 45 F0");
                injector::MakeNOP(pattern.get_first(), 2);
                static auto g_cmdarg_windowed_hook = safetyhook::create_mid(pattern.get_first(),
                [](SafetyHookContext& ctx)
                {
                    ctx.esi = IsWindowedMode();
                });

                pattern = hook::pattern("89 35 ? ? ? ? A3 ? ? ? ? 8D 44 24 17");
                static auto g_cmdarg_windowed_hook2 = safetyhook::create_mid(pattern.get_first(),
                [](SafetyHookContext& ctx)
                {
                    SetWindowedPref(!ctx.eax);
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

            // One setting for what were Windowed and Windowed Borderless: the window style follows at once, fullscreen and
            // window are switched like Alt+Enter
            static auto bSkipWindowedCallback1 = false;
            static auto bSkipWindowedCallback2 = false;
            FusionFixSettings.SetCallback("PREF_WINDOWMODE", [](int32_t value)
            {
                RememberWindowMonitor();
                if (value == ModeWindow || value == ModeBorderless)
                    lastWindowMode = value;
                // A window mode while the device is still exclusive fullscreen (Alt+Enter, the menu): the game restyles
                // and places its window itself after the Reset to a window (SetWindowPos_Hook). Restyling and placing it
                // over the monitor before (MarkFullscreenWindow, DWM attributes) loses exclusive fullscreen on DirectX 12:
                // the runtime reports the device lost and the game resets it fullscreen again.
                if (value != ModeFullscreen && *rage::grcWindow::ms_bWindowed)
                    SwitchWindowStyle();
                else if (gWnd && !BorderlessFullscreen())
                    WindowStyle::Unplace(gWnd);
                if (!bSkipWindowedCallback1)
                {
                    if (*rage::grcWindow::ms_bWindowed != (value != ModeFullscreen))
                    {
                        bSkipWindowedCallback2 = true;
                        SendMessageA(gWnd, 260, 13, 0);
                    }
                }
                bSkipWindowedCallback1 = false;
            });

            pattern = find_pattern("C6 05 ? ? ? ? ? FF 75 14", "C6 05 ? ? ? ? ? 0F 94 05 ? ? ? ? FF 75 14", "C6 05 ? ? ? ? 01 8B 54 24 1C", "C6 05 ? ? ? ? ? FF 15 ? ? ? ? 5F");
            static auto AltEnterHandlerHook = safetyhook::create_mid(pattern.get_first(),
            [](SafetyHookContext& ctx)
            {
                RememberWindowMonitor();
                bSkipWindowedCallback1 = true;
                if (!bSkipWindowedCallback2)
                    FusionFixSettings.Set("PREF_WINDOWMODE", IsWindowedMode() ? ModeFullscreen : lastWindowMode);
                bSkipWindowedCallback2 = false;
            });

            // Do not process input on focus loss
            pattern = find_pattern("E8 ? ? ? ? A1 ? ? ? ? A3 ? ? ? ? A1 ? ? ? ? 83 C4 04", "E8 ? ? ? ? 8B 0D ? ? ? ? 8B 15 ? ? ? ? 83 C4 04 83 3D");
            hbsub_7870A0.fun = injector::MakeCALL(pattern.get_first(), sub_69F0C0).get();

            FusionFixSettings.SetCallback("PREF_BLOCKONLOSTFOCUS", [](int32_t value)
            {
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