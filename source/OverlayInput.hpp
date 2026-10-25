#pragma once
#include <Windows.h>
struct IDirect3DDevice9;
// In-game settings overlay (source/Overlay.cpp): the hooks of other modules call these.
namespace GTAIVOverlay
{
    void AttachWindow(HWND window);
    // At the game's final EndScene, after console gamma, HDR output and the automation grab
    void Present(IDirect3DDevice9* device);
    bool BlocksInput();
}
