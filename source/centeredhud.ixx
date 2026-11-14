module;

#include <common.hxx>
#include <atomic>
#include <cmath>
#include <vector>

export module centeredhud;

import common;
import comvars;
import settings;

// Centered HUD for screens wider than 16:9, Complete Edition only. FUN_/DAT_ names are 1.2.0.59 addresses.
//
// HUD: FUN_008fc260(mode, pos, size, wrap) lays the 4:3 hud.dat layout out for the aspect A that FUN_008f7940 reports. With
// k = (4/3)/A it maps x by its anchor mode: left (2) x*k, right (3) x*k + 1 - k, centre (1) x*k + (1-k)/2; mode 4 picks one of
// the three by x against 0.5. The wrap pair gets the same mapping as x. The call sites that place HUD elements are pointed at
// wrappers that call it and then move an edge-anchored result into the centered 16:9 area: by +c for the left edge and -c for
// the right one, c = (1 - (16/9)/A)/2. Widths are already aspect-correct.
//
// Frontend: the menus are laid out as on a 16:9 screen (FUN_008f7940 reports 16:9 to their layout code) and squeezed into the
// centered area while their two draw commands run, through the BlitMatrix for pixel draws and the viewport projection for
// normalized ones, with black bars beside it, except behind the pause map, which WidescreenFix draws across the full width.
//
// Legal screens: their text wraps inside the centered area. WidescreenFix fits the loading screen art and the computer itself.

namespace CenteredHud
{
    constexpr float k16by9 = 16.0f / 9.0f;

    // Set once everything is found and patched
    std::atomic<bool> applied = false;

    // Everything is found before anything is changed: a lookup that fails throws, and the game stays stock

    struct Patch
    {
        uintptr_t at;
        const void* to;
        bool call;
    };
    std::vector<Patch> patches;

    void Expect(bool condition)
    {
        if (!condition)
        {
            throw hook::txn_exception();
        }
    }

    // The address offset bytes into the match of a signature
    uintptr_t Find(std::string_view signature, ptrdiff_t offset = 0)
    {
        return reinterpret_cast<uintptr_t>(hook::txn::get_pattern(signature, offset));
    }

    template<typename T>
    T Read(uintptr_t address)
    {
        return *reinterpret_cast<T*>(address);
    }

    // The function the CALL at call calls
    uintptr_t Callee(uintptr_t call)
    {
        Expect(Read<uint8_t>(call) == 0xE8);
        return call + 5 + Read<int32_t>(call + 1);
    }

    // Points the CALL at call at thunk. Returns the function it calls now, which must be callee when one is given
    template<typename F>
    uintptr_t Redirect(uintptr_t call, F thunk, uintptr_t callee = 0)
    {
        auto target = Callee(call);
        Expect(!callee || target == callee);
        patches.push_back({ call, reinterpret_cast<const void*>(thunk), true });
        return target;
    }

    // Points the 4-byte operand at operand at value
    template<typename T>
    void Point(uintptr_t operand, T value)
    {
        patches.push_back({ operand, reinterpret_cast<const void*>(value), false });
    }

    // Engine functions and data

    uint8_t(__cdecl* anchor)(int32_t, float*, float*, float*) = nullptr; // FUN_008fc260, returns in AL
    float(__thiscall* getAspect)(void*, int32_t) = nullptr;            // FUN_008f7940, RET 4, returns in ST0
    void* aspectObject = nullptr;
    uint8_t* force16by9 = nullptr;      // set while the video editor exports
    uintptr_t itemArray = 0;            // DAT_0118e7f8, the HUD items registered from hud.dat
    uintptr_t* mainViewport = nullptr;  // DAT_0118d804, the HUD's viewport, render targets (phone screen, TV) have other render ids
    int32_t* spriteRenderId = nullptr;  // DAT_01037678, the render id the single script sprite is queued for

    // The aspect the game lays itself out for: the back buffer's (FusionFix makes "auto" follow it) or the one picked in the
    // display settings, 16:9 while the video editor exports
    float Aspect()
    {
        return *force16by9 ? k16by9 : getAspect(aspectObject, 1);
    }

    // The share of the screen width the centered 16:9 area takes, 0 when the screen is not wider than 16:9 (with a 0.2% margin,
    // so 16:9 screens stay stock)
    float Share(float aspect)
    {
        return aspect > k16by9 * 1.002f ? k16by9 / aspect : 0.0f;
    }

    // Maps a full-screen x onto the centered area about the screen centre
    float Squeeze(float x, float share)
    {
        return share > 0.0f ? 0.5f + (x - 0.5f) * share : x;
    }

    bool IsMainRenderId(int32_t renderId)
    {
        auto viewport = *mainViewport;
        return viewport && Read<int32_t>(viewport + 0x540) == renderId;
    }

    // Frontend state, per thread: the frontend is laid out on the update thread and drawn on the render thread

    // Set on the update thread while the frontend's per-frame update runs
    thread_local bool updateScope = false;
    // Set from the entry of the menu-opening function until the end of the update that runs it: the first page is laid out
    // before the open flag is set
    thread_local bool opening = false;
    // Depth of squeezed draw commands on the render thread
    thread_local uint32_t squeezed = 0;
    // The squeeze the BlitMatrix gets while it is rebuilt on this thread, 0 for none
    thread_local float matrixSqueeze = 0.0f;

    struct
    {
        uint8_t* menuOpen = nullptr;          // DAT_011609f6
        uint8_t* videoEditor = nullptr;       // DAT_01160c3d
        int32_t* page = nullptr;              // DAT_01160c40, the same global as pMenuTab
        uintptr_t* currentViewport = nullptr; // DAT_017f583c
        void(__thiscall* ortho)(uintptr_t, float, float, float, float, float, float) = nullptr; // grcViewport::Ortho, FUN_00431640
        void(__cdecl* setCurrent)(uintptr_t, int32_t) = nullptr; // grcViewport::SetCurrent, FUN_00430bf0
        void(__cdecl* drawMenu)() = nullptr;    // FUN_005a90a0
        void(__cdecl* drawOverlay)() = nullptr; // FUN_005aa7d0
        void(__cdecl* blitRebuild)() = nullptr; // FUN_008d4b50
        void(__cdecl* textFlush)() = nullptr;   // FUN_00923950
        void(__cdecl* drawRect)(const float*, const uint32_t*) = nullptr; // FUN_008d4750
    } frontend;

    uintptr_t frontendUpdate = 0; // FUN_005a6d90

    // Floats the game reads through redirected operands
    constexpr float kUiReference = 1024.0f / 768.0f;
    float fUiReference = kUiReference;
    float fCursorGain = 0.0f;
    float fStockCursorGain = 0.0f;

    // The squeeze for a menu that is open now, outside the video editor
    float MenuSqueeze()
    {
        if (!applied || !*frontend.menuOpen || *frontend.videoEditor)
        {
            return 0.0f;
        }
        return Share(Aspect());
    }

    // The squeeze for the layout being computed on this thread: the menu is open or opening, outside the video editor
    float LayoutSqueeze()
    {
        if (!applied || (!*frontend.menuOpen && !opening) || *frontend.videoEditor)
        {
            return 0.0f;
        }
        return Share(Aspect());
    }

    // Whether this thread lays out or draws the 16:9 menu: a squeezed draw command on the render thread, or the frontend update
    bool Virtual16by9()
    {
        return squeezed > 0 || (updateScope && LayoutSqueeze() > 0.0f);
    }

    // The share for the HUD. 0 while the frontend lays out its 16:9 screen, which already places everything it draws
    float HudShare()
    {
        return Virtual16by9() ? 0.0f : Share(Aspect());
    }

    // DAT_018b6c84, the id of the HTML viewport CREATE_HTML_VIEWPORT makes for the computer, 0 once it is deleted
    int32_t* htmlViewportId = nullptr;

    bool ComputerActive()
    {
        return *htmlViewportId > 0 && !*frontend.menuOpen;
    }

    // HUD

    // 1 when mode anchors pos to the left edge, -1 for the right edge, 0 otherwise. FUN_008fc260 decides mode 4 by x against 0.5
    // at 0x008fc2c7. Called before the original overwrites pos
    int32_t AnchorEdge(int32_t mode, const float* pos)
    {
        if (mode == 2)
        {
            return 1;
        }
        if (mode == 3)
        {
            return -1;
        }
        if (mode == 4 && pos)
        {
            return *pos < 0.5f ? 1 : (*pos > 0.5f ? -1 : 0);
        }
        return 0;
    }

    // Moves an edge-anchored x and wrap pair, as FUN_008fc260 left them, into the centered area
    void Shift(int32_t edge, float* pos, float* wrap)
    {
        auto share = edge ? HudShare() : 0.0f;
        if (share == 0.0f)
        {
            return;
        }

        auto delta = 0.5f * (1.0f - share) * float(edge);
        if (pos)
        {
            pos[0] += delta;
        }
        if (wrap)
        {
            wrap[0] += delta;
            wrap[1] += delta;
        }
    }

    // Stands in for FUN_008fc260 where the HUD is placed
    uint8_t __cdecl HudAnchored(int32_t mode, float* pos, float* size, float* wrap)
    {
        auto edge = AnchorEdge(mode, pos);
        auto result = anchor(mode, pos, size, wrap);
        Shift(edge, pos, wrap);
        return result;
    }

    // The radar rectangle is recomputed when the menu closes, inside the frontend update, and gets the real aspect
    uint8_t __cdecl RadarAnchored(int32_t mode, float* pos, float* size, float* wrap)
    {
        auto scope = std::exchange(updateScope, false);
        auto open = std::exchange(opening, false);
        auto result = HudAnchored(mode, pos, size, wrap);
        updateScope = scope;
        opening = open;
        return result;
    }

    // Set while the button prompt bar is drawn for the game or a loading screen, as opposed to a frontend menu
    thread_local bool promptsInGame = false;
    uintptr_t drawPrompts = 0; // FUN_008b6c80

    uint8_t __cdecl PromptsAnchored(int32_t mode, float* pos, float* size, float* wrap)
    {
        return promptsInGame ? HudAnchored(mode, pos, size, wrap) : anchor(mode, pos, size, wrap);
    }

    void DrawPrompts(bool inGame)
    {
        auto previous = std::exchange(promptsInGame, inGame);
        reinterpret_cast<void(__cdecl*)()>(drawPrompts)();
        promptsInGame = previous;
    }

    // Stands in for FUN_008b6c80 in the loading screen's prompt draw
    void __cdecl PromptsOnLoadingScreen()
    {
        DrawPrompts(true);
    }

    // Stands in for FUN_008b6c80 in the script text draw, which draws once per render id. Only the main viewport's prompts move
    void __cdecl PromptsForScript(int32_t renderId)
    {
        DrawPrompts(IsMainRenderId(renderId));
    }

    // EBX holds the render id FUN_0094ad40 draws for
    __declspec(naked) void PromptsForScriptShim()
    {
        __asm
        {
            push ebx
            call PromptsForScript
            add esp, 4
            ret
        }
    }

    // Script HUD: text slots (FUN_0094ad40, EBX = render id), the rectangle and sprite queue (FUN_00949d70, EBP = render id) and
    // the single script sprite (FUN_0094b130). Only what is drawn into the main viewport moves, and a rectangle or sprite corner
    // at a screen edge belongs to a full-screen overlay (fades, tints), which stays.
    uint8_t __cdecl ScriptAnchored(int32_t mode, float* pos, float* size, float* wrap, int32_t renderId, uint32_t corner)
    {
        auto x = pos ? *pos : 0.5f;
        auto edge = AnchorEdge(mode, pos);
        auto result = anchor(mode, pos, size, wrap);
        if (IsMainRenderId(renderId) && !(corner && (x < 0.0005f || x > 0.9995f)))
        {
            Shift(edge, pos, wrap);
        }
        return result;
    }

    __declspec(naked) void ScriptTextShim()
    {
        __asm
        {
            push 0
            push ebx
            push dword ptr [esp + 0x18]
            push dword ptr [esp + 0x18]
            push dword ptr [esp + 0x18]
            push dword ptr [esp + 0x18]
            call ScriptAnchored
            add esp, 0x18
            ret
        }
    }

    __declspec(naked) void ScriptRectShim()
    {
        __asm
        {
            push 1
            push ebp
            push dword ptr [esp + 0x18]
            push dword ptr [esp + 0x18]
            push dword ptr [esp + 0x18]
            push dword ptr [esp + 0x18]
            call ScriptAnchored
            add esp, 0x18
            ret
        }
    }

    uint8_t __cdecl ScriptSpriteAnchored(int32_t mode, float* pos, float* size, float* wrap)
    {
        return ScriptAnchored(mode, pos, size, wrap, *spriteRenderId, 1);
    }

    // Text that wraps across fixed fractions of the screen width: the end credits (FUN_00ab5440, FUN_00ab5350) and the legal
    // screens (FUN_005cd010)

    uintptr_t setWrap = 0;   // FUN_00924610(start, end)
    uintptr_t print = 0;     // FUN_00921da0(x, y, text, ...)
    uintptr_t lineCount = 0; // FUN_0091c740(x, y, text, flags), lays a paragraph out without drawing it

    void __cdecl SqueezedWrap(float start, float end)
    {
        auto share = HudShare();
        reinterpret_cast<void(__cdecl*)(float, float)>(setWrap)(Squeeze(start, share), Squeeze(end, share));
    }

    uint32_t __cdecl SqueezedPrint(float x, float y, const void* text, int32_t a, int32_t b)
    {
        return reinterpret_cast<uint32_t(__cdecl*)(float, float, const void*, int32_t, int32_t)>(print)(Squeeze(x, HudShare()), y, text, a, b);
    }

    int32_t __cdecl SqueezedLineCount(float x, float y, const void* text, int32_t flags)
    {
        return reinterpret_cast<int32_t(__cdecl*)(float, float, const void*, int32_t)>(lineCount)(Squeeze(x, HudShare()), y, text, flags);
    }

    // Cellphone: CViewportMobilePhone sets its window (0.6 W, 0.5 H, 0.4 W, 0.5 H of the screen size it holds) per frame in
    // FUN_005d4b80. Mapped onto the centered area, the window has the shape it has on a 16:9 screen, and WidescreenFix gives the
    // phone's x scale the value for that shape (PhoneAspect).

    uintptr_t setWindow = 0; // grcViewport::SetWindow, FUN_00431110, thiscall, RET 0x18

    void __fastcall PhoneSetWindow(uintptr_t viewport, void*, int32_t x, int32_t y, int32_t w, int32_t h, float zNear, float zFar)
    {
        // The screen size the viewport holds, CViewport+0x2c0 and +0x2c4; SetWindow gets CViewport+0x10
        auto width = float(Read<int32_t>(viewport + 0x2c0 - 0x10));
        auto height = float(Read<int32_t>(viewport + 0x2c4 - 0x10));
        if (auto share = height > 0.0f ? Share(width / height) : 0.0f; share > 0.0f)
        {
            x = int32_t(width / 2.0f + (float(x) - width / 2.0f) * share);
            w = int32_t(float(w) * share);
        }
        reinterpret_cast<void(__thiscall*)(uintptr_t, int32_t, int32_t, int32_t, int32_t, float, float)>(setWindow)(viewport, x, y, w, h, zNear, zFar);
    }

    // Frontend

    // Updates the floats the game reads through redirected operands for the layout about to run
    void Refresh()
    {
        if (!applied)
        {
            return;
        }

        auto squeeze = LayoutSqueeze();
        fUiReference = squeeze > 0.0f ? kUiReference / squeeze : kUiReference;
        fCursorGain = squeeze > 0.0f ? fStockCursorGain / squeeze : fStockCursorGain;
    }

    // Stands in for FUN_005a6d90, the frontend's per-frame update, which runs every hit test, page layout, menu open and close
    void __cdecl Tick()
    {
        auto scope = std::exchange(updateScope, true);
        Refresh();
        reinterpret_cast<void(__cdecl*)()>(frontendUpdate)();
        updateScope = scope;
        opening = false;
        Refresh();
    }

    void SetProjection(uintptr_t viewport, float left, float right)
    {
        frontend.ortho(viewport, left, right, 1.0f, 0.0f, 0.0f, 1.0f);
        frontend.setCurrent(viewport, 1);
    }

    // Paints the two side bands outside the centered area black, in the viewport's pixel size (target size times window size)
    void DrawSideBars(uintptr_t viewport, float share)
    {
        auto width = std::trunc(float(Read<int32_t>(viewport + 0x2b0)) * Read<float>(viewport + 0x288));
        auto height = std::trunc(float(Read<int32_t>(viewport + 0x2b4)) * Read<float>(viewport + 0x28c));
        auto bar = 0.5f * (1.0f - share) * width;
        uint32_t black = 0xFF000000;
        float left[4] = { 0.0f, 0.0f, bar, height };
        float right[4] = { width - bar, 0.0f, width, height };
        frontend.drawRect(left, &black);
        frontend.drawRect(right, &black);
    }

    // Runs one of the frontend's draw commands laid out at 16:9 and squeezed into the centered area. Text is drawn before the
    // BlitMatrix goes back to stock. With bars, the screen outside the area is painted black afterwards, except behind the full-
    // width pause map (page 3) and the pages FusionFix shows the game behind.
    void DrawSqueezed(void(__cdecl* original)(), bool bars)
    {
        auto viewport = *frontend.currentViewport;
        auto share = MenuSqueeze();
        if (share == 0.0f || !viewport)
        {
            original();
            return;
        }

        auto half = 0.5f / share;
        SetProjection(viewport, 0.5f - half, 0.5f + half);
        matrixSqueeze = share;
        frontend.blitRebuild();
        squeezed++;
        original();
        squeezed--;
        frontend.textFlush();
        matrixSqueeze = 0.0f;
        frontend.blitRebuild();
        SetProjection(viewport, 0.0f, 1.0f);

        auto page = *frontend.page;
        if (bars && page != 3 && !shouldModifyMenuBackground(page))
        {
            DrawSideBars(viewport, share);
        }
    }

    // Stands in for FUN_005a90a0: pages, dims, header and footer, toolkit
    void __cdecl DrawMenu()
    {
        DrawSqueezed(frontend.drawMenu, true);
    }

    // Stands in for FUN_005aa7d0: prompt bar, status text and cursor
    void __cdecl DrawOverlay()
    {
        DrawSqueezed(frontend.drawOverlay, false);
    }

    // Stands in for FUN_008f7940 at the frontend's layout call sites: 16:9 while the menu is laid out or drawn
    float __fastcall UiAspect(void* object, void*, int32_t arg)
    {
        if (arg && Virtual16by9())
        {
            return k16by9;
        }
        return getAspect(object, arg);
    }

    // Stands in for FUN_00901310 in FUN_009011e0, which the pause map's mouse consumers (blip hover, drag, waypoint) pass the
    // cursor through: maps the zoom-scaled point from the squeezed menu cursor's space to the screen
    uintptr_t toPixels = 0;

    void __cdecl ToPixels(float* out, float* point)
    {
        if (point)
        {
            *point = Squeeze(*point, MenuSqueeze());
        }
        reinterpret_cast<void(__cdecl*)(float*, float*)>(toPixels)(out, point);
    }

    // Black bars beside the computer's screen, which WidescreenFix centres, queued as a draw command after the script UI like
    // its cutscene borders, since the script UI may be recorded into a draw list rather than drawn
    // cutscene borders, since the script UI may be recorded into a draw list rather than drawn
    uintptr_t drawScriptGraphics = 0; // FUN_0094ad40(render id, layer)

    void QueueComputerBars()
    {
        if (!ComputerActive() || Share(Aspect()) == 0.0f)
        {
            return;
        }

        auto dc = new T_CB_Generic_NoArgs([]()
        {
            if (auto viewport = *frontend.currentViewport; viewport)
            {
                DrawSideBars(viewport, Share(Aspect()));
            }
        });
        if (dc)
        {
            dc->Append();
        }
    }

    // The last script layer of a viewport's 2D pass (FUN_00b017c0)
    void __cdecl ViewportScriptGraphics(int32_t renderId, int32_t layer)
    {
        reinterpret_cast<void(__cdecl*)(int32_t, int32_t)>(drawScriptGraphics)(renderId, layer);
        if (IsMainRenderId(renderId))
        {
            QueueComputerBars();
        }
    }

    // CRenderPhaseScript2d's draw (FUN_00addcd0)
    void __cdecl Script2dGraphics(int32_t renderId, int32_t layer)
    {
        reinterpret_cast<void(__cdecl*)(int32_t, int32_t)>(drawScriptGraphics)(renderId, layer);
        QueueComputerBars();
    }

    void Init()
    {
        SettingsReader reader;
        if (!reader.ReadInteger("MISC", "CenteredHUD", 1))
        {
            return;
        }

        uintptr_t subtitleCall = 0, uploadCall = 0, openingEntry = 0;
        try
        {
            // FUN_008fc260: PUSH EBX; MOV EBX,[ESP+8]; TEST EBX,EBX; JNZ; XOR AL,AL; POP EBX; RET; PUSH 1; MOV ECX,object;
            // CALL FUN_008f7940; FSTP [ESP+8]; CMP byte [force],0. Matched past its first 5 bytes, which WidescreenFix hooks
            auto anchorFunction = Find("85 DB 75 04 32 C0 5B C3 6A 01 B9 ? ? ? ? E8 ? ? ? ? D9 5C 24 08 80 3D ? ? ? ? 00", -5);
            anchor = reinterpret_cast<decltype(anchor)>(anchorFunction);
            getAspect = reinterpret_cast<decltype(getAspect)>(Callee(anchorFunction + 20));
            aspectObject = Read<void*>(anchorFunction + 16);
            force16by9 = Read<uint8_t*>(anchorFunction + 31);
            // FUN_008ff3d0: SUB ESP,0x20; PUSH ESI; MOV ESI,[ESP+0x28]; MOV EAX,[ESI*4+items]; CMP word [EAX+0x6c],0
            itemArray = Read<uintptr_t>(Find("83 EC 20 56 8B 74 24 28 8B 04 B5 ? ? ? ? 66 83 78 6C 00", 11));
            // FUN_008fe950: CMP ECX,-1; JZ; MOV EAX,[viewport]; CMP ECX,[EAX+0x540]; JNZ
            mainViewport = Read<uintptr_t*>(Find("83 F9 FF 74 ? A1 ? ? ? ? 3B 88 40 05 00 00 0F 85", 6));
            // FUN_0094b130: PUSH 0; PUSH 6; CALL; MOV EAX,[ESP+x]; ADD ESP,0x28; CMP EAX,[sprite render id]; JNZ
            spriteRenderId = Read<int32_t*>(Find("6A 00 6A 06 E8 ? ? ? ? 8B 44 24 ? 83 C4 28 3B 05 ? ? ? ? 0F 85", 18));

            // The HUD items registered from hud.dat (cash, wanted stars, ammo, weapon, area, street and vehicle names, mission clock and
            // counters, phone message icons, the loading screen's label): FUN_008fe950 dispatches on the item type to these draws
            Redirect(Find("51 8D 4C 24 14 51 50 E8 ? ? ? ? F3 0F 10 54 24 24 F3 0F 10 4C 24 1C", 7), HudAnchored, uintptr_t(anchor));           // rects, FUN_008fede0
            Redirect(Find("51 8D 4C 24 20 51 50 E8 ? ? ? ? F3 0F 10 4C 24 30 83 C4 10", 7), HudAnchored, uintptr_t(anchor));                  // sprites, FUN_008ff000
            Redirect(Find("8D 44 24 14 50 6A 00 6A 02 E8 ? ? ? ? F3 0F 10 44 24 2C 83 C4 10", 9), HudAnchored, uintptr_t(anchor));              // text, FUN_008ff3d0
            Redirect(Find("8D 4C 24 1C 51 F3 0F 11 44 24 2C FF 70 14 E8 ? ? ? ? 8B 14 BD ? ? ? ? 83 C4 18", 14), HudAnchored, uintptr_t(anchor)); // bars, FUN_008fec40
            Redirect(Find("51 8D 4C 24 10 51 50 E8 ? ? ? ? F3 0F 10 54 24 20 F3 0F 10 4C 24 18", 7), HudAnchored, uintptr_t(anchor));           // phone box, FUN_008ff8e0
            // The help message box (FUN_008f9a00), the loading bar, the saving spinner and its label (FUN_008e3510), the network
            // status line (FUN_005ba3f0)
            Redirect(Find("50 8D 44 24 38 50 6A 03 E8 ? ? ? ? F3 0F 10 44 24 40 83 C4 10", 8), HudAnchored, uintptr_t(anchor));
            Redirect(Find("50 6A 03 C7 44 24 40 00 00 00 00 F3 0F 11 44 24 3C E8 ? ? ? ? F3 0F 10 05 ? ? ? ? F3 0F 58 44 24 3C", 17), HudAnchored, uintptr_t(anchor));
            Redirect(Find("6A 00 56 55 6A 02 E8 ? ? ? ? F3 0F 10 6D 00 F3 0F 10 65 04", 6), HudAnchored, uintptr_t(anchor));
            Redirect(Find("6A 00 55 53 6A 02 E8 ? ? ? ? F3 0F 10 45 04 83 C4 14", 6), HudAnchored, uintptr_t(anchor));
            Redirect(Find("6A 00 8D 44 24 44 50 6A 02 E8 ? ? ? ? 8D 44 24 40 68 96 00 00 00", 9), HudAnchored, uintptr_t(anchor));
            // FUN_00908d20 anchors the radar rectangle DAT_010344b0 left unless the pause map is open; the radar viewport and the blip
            // and GPS mappings read it
            Redirect(Find("6A 00 68 ? ? ? ? 68 ? ? ? ? 6A 02 E8 ? ? ? ? 83 C4 10 E8 ? ? ? ?", 14), RadarAnchored, uintptr_t(anchor));

            // The button prompt bar FUN_008b6c80, right-anchored. It moves when the script text draw or the loading screen draws it;
            // the frontend draws it from about 50 places and places it itself
            Redirect(Find("6A 00 8D 44 24 18 50 6A 03 E8 ? ? ? ? 83 C4 10 8D 4C 24 64", 9), PromptsAnchored, uintptr_t(anchor));
            Redirect(Find("6A 00 8D 44 24 34 50 6A 03 E8 ? ? ? ? F3 0F 10 44 24 3C F3 0F 11 44 24 20", 9), PromptsAnchored, uintptr_t(anchor));
            // ADD ESI,0x7c; CMP EDI,ECX; JC (end of the text slot loop); CALL FUN_00923950; CALL FUN_008b6c80
            drawPrompts = Redirect(Find("83 C6 7C 3B F9 0F 82 ? ? ? ? E8 ? ? ? ? E8 ? ? ? ? 6A 00 E8 ? ? ? ?", 16), PromptsForScriptShim);
            Redirect(Find("68 ? ? ? ? 68 ? ? ? ? E8 ? ? ? ? 83 C4 0C E8 ? ? ? ? C7 05 ? ? ? ? 00 00 00 00 8D 44 24 08", 18), PromptsOnLoadingScreen, drawPrompts);

            // Script HUD
            Redirect(Find("FF 76 C6 F3 0F 11 44 24 30 F3 0F 10 46 0A F3 0F 11 44 24 34 E8 ? ? ? ? F3 0F 10 44 24 3C 83 C4 10", 20), ScriptTextShim, uintptr_t(anchor));
            Redirect(Find("FF B4 31 ? ? ? ? F3 0F 11 44 24 30 F3 0F 10 84 31 ? ? ? ? F3 0F 11 44 24 34 E8 ? ? ? ? 6A 00 6A 00", 28), ScriptRectShim, uintptr_t(anchor));
            Redirect(Find("50 A1 ? ? ? ? 69 C0 64 3B 00 00 FF B4 30 ? ? ? ? E8 ? ? ? ? E8 ? ? ? ? 6A 00", 19), ScriptRectShim, uintptr_t(anchor));
            Redirect(Find("FF 35 ? ? ? ? F3 0F 11 44 24 1C F3 0F 10 05 ? ? ? ? F3 0F 11 44 24 20 E8 ? ? ? ? 6A 00 6A 00", 26), ScriptSpriteAnchored, uintptr_t(anchor));
            Redirect(Find("6A 00 8D 44 24 24 50 FF 35 ? ? ? ? E8 ? ? ? ? E8 ? ? ? ? 6A 00", 13), ScriptSpriteAnchored, uintptr_t(anchor));

            // The subtitles' wrap pair, set in the HUD text draw
            subtitleCall = Find("83 EC 08 F3 0F 11 44 24 04 F3 0F 10 44 24 20 F3 0F 11 04 24 E8 ? ? ? ? F3 0F 10 44 24 1C F3 0F 11 44 24 04", 20);
            setWrap = Callee(subtitleCall);
            // The end credits: each column's wrap pair and each line's x are literal screen fractions
            print = Redirect(Find("83 EC 08 F3 0F 11 44 24 04 F3 0F 10 44 24 24 F3 0F 11 04 24 E8 ? ? ? ? 83 C4 14 F3 0F 10 44 24 20", 20), SqueezedPrint);
            Redirect(Find("56 83 EC 08 C7 44 24 04 33 33 73 3F C7 04 24 CD CC 4C 3D E8 ? ? ? ? 6A 00 E8 ? ? ? ?", 19), SqueezedWrap, setWrap);
            Redirect(Find("0F 84 ? ? ? ? 83 EC 08 C7 44 24 04 66 66 66 3F C7 04 24 CD CC CC 3D E8 ? ? ? ? 8B 04 B5 ? ? ? ? 83 C0 04", 24), SqueezedWrap, setWrap);
            Redirect(Find("74 7E 83 EC 08 C7 44 24 04 1F 85 6B 3F C7 04 24 0A D7 A3 3D E8 ? ? ? ? 6A 00 E8 ? ? ? ?", 20), SqueezedWrap, setWrap);
            // The legal screens: each paragraph wraps at 0.15..0.85, is counted with a dry-run layout at x = 0.15 to centre it
            // vertically, and is printed
            Redirect(Find("83 EC 08 F3 0F 11 4C 24 04 F3 0F 11 04 24 E8 ? ? ? ? 6A 00 8D 44 24 24 50 6A 00 6A 07 E8", 14), SqueezedWrap, setWrap);
            lineCount = Redirect(Find("C7 44 24 04 00 00 00 3F C7 04 24 9A 99 19 3E E8 ? ? ? ? 8B F0 83 C4 10 83 FE 02", 15), SqueezedLineCount);
            Redirect(Find("F3 0F 11 44 24 04 C7 04 24 9A 99 19 3E E8 ? ? ? ? 83 C4 14 E8 ? ? ? ? 5E 5B 83 C4 24 C3", 13), SqueezedPrint, print);

            // The cellphone's window
            setWindow = Redirect(Find("66 0F 6E 86 C4 02 00 00 66 0F 6E 8E C0 02 00 00 0F 5B C0 0F 5B C9 F3 0F 59 05 ? ? ? ? 83 EC 08 8D 4E 10 "
                "F3 0F 2C D0 0F 28 C1 F3 0F 59 05 ? ? ? ? F3 0F 59 0D ? ? ? ? F3 0F 2C C0 C7 44 24 04 00 00 80 3F C7 04 24 00 00 00 00 52 50 "
                "F3 0F 2C C1 52 50 E8 ? ? ? ? 5E C3", 86), PhoneSetWindow);

            // The frontend's per-frame update, called from the pre-game loop FUN_005c2060 and the in-game frame code at 0x005c2670
            frontendUpdate = Redirect(Find("8B 0D ? ? ? ? E8 ? ? ? ? E8 ? ? ? ? 8B 0D ? ? ? ? 6A 00 E8 ? ? ? ? 80 3D ? ? ? ? 00 75", 11), Tick);
            Redirect(Find("FF 35 ? ? ? ? E8 ? ? ? ? E8 ? ? ? ? E8 ? ? ? ? E8 ? ? ? ? 8B 0D ? ? ? ? 6A 00 E8", 21), Tick, frontendUpdate);
            // The calls of FUN_008f7940 the frontend's layout depends on: k of every anchor call, the cursor width, the tab header's
            // exact-16:9 test and the episode select. The other 24 test aspect buckets that answer the same at 16:9 and wider, or set
            // up the 3D camera, which keeps the real aspect
            Redirect(anchorFunction + 20, UiAspect, uintptr_t(getAspect));
            Redirect(Find("E8 ? ? ? ? D9 5C 24 20 80 3D ? ? ? ? 00 74 0A F3 0F 10 05 ? ? ? ? EB 06"), UiAspect, uintptr_t(getAspect));
            Redirect(Find("0F 5B C9 0F 5B C0 F3 0F 59 C8 6A 01 B9 ? ? ? ? 0F 2F 0D ? ? ? ? 0F 97 44 24 0B E8 ? ? ? ? D9 5C 24 08", 29), UiAspect, uintptr_t(getAspect));
            Redirect(Find("6A 01 B9 ? ? ? ? F3 0F 11 54 24 28 F3 0F 11 5C 24 2C E8 ? ? ? ? F3 0F 10 0D ? ? ? ? D9 5C 24 1C", 19), UiAspect, uintptr_t(getAspect));
            // CRenderPhaseFrontEnd's draw-list builder FUN_005e1580 records two T_CB_Generic_NoArgs commands: MOV [EAX],vftable;
            // MOV [EAX+8],function
            auto drawList = Find("6A 00 6A 0C E8 ? ? ? ? 83 C4 08 85 C0 74 2A 8B 15 ? ? ? ? 8B 48 04 33 CA 81 E1 FF 3F 00 00 31 48 04 42 "
                "89 15 ? ? ? ? C7 00 ? ? ? ? C7 40 08 ? ? ? ? EB 02 33 C0 50 E8 ? ? ? ? 6A 00 6A 0C E8 ? ? ? ? 83 C4 0C 85 C0 74 30 "
                "8B 15 ? ? ? ? 8B 48 04 33 CA 81 E1 FF 3F 00 00 31 48 04 42 50 89 15 ? ? ? ? C7 00 ? ? ? ? C7 40 08 ? ? ? ?");
            frontend.drawMenu = Read<decltype(frontend.drawMenu)>(drawList + 52);
            frontend.drawOverlay = Read<decltype(frontend.drawOverlay)>(drawList + 119);
            Point(drawList + 52, DrawMenu);
            Point(drawList + 119, DrawOverlay);
            // Sprites, rectangles, glyphs and the toolkit's images take pixels through the BlitMatrix FUN_008d4b60 builds from the
            // current viewport: LEA EAX,[ESP+0x10] (matrix); PUSH EAX; PUSH [handle]; ...; CALL FUN_00451200 (upload)
            uploadCall = Find("8D 44 24 10 50 FF 35 ? ? ? ? C7 44 24 48 00 00 80 BF F3 0F 11 4C 24 2C C7 44 24 4C 00 00 80 3F E8 ? ? ? ? 5F 5E", 33);
            Callee(uploadCall);
            frontend.currentViewport = Read<uintptr_t*>(Find("80 3D ? ? ? ? 00 56 57 0F 85 ? ? ? ? A1 ? ? ? ? 85 C0 74 30 66 0F 6E 80 B0 02 00 00 0F 5B C0 F3 0F 59 80 88 02 00 00", 16));
            // FUN_008d4b50: PUSH 1; MOV byte [lock],0; CALL FUN_008d4b60; POP ECX; RET
            frontend.blitRebuild = reinterpret_cast<decltype(frontend.blitRebuild)>(Find("6A 01 C6 05 ? ? ? ? 00 E8 ? ? ? ? 59 C3"));
            // Normalized quads go through the current viewport's projection: Ortho(0,1,1,0,0,1) on phase+0xb0 in FUN_005e13c0, and
            // CSetCurrentViewportDC::Execute
            frontend.ortho = reinterpret_cast<decltype(frontend.ortho)>(Callee(Find("8D 8E B0 00 00 00 C7 44 24 14 00 00 80 3F C7 44 24 10 00 00 00 00 "
                "C7 44 24 0C 00 00 00 00 C7 44 24 08 00 00 80 3F C7 44 24 04 00 00 80 3F C7 04 24 00 00 00 00 E8 ? ? ? ? 8D 44 24 30 39 05", 53)));
            frontend.setCurrent = reinterpret_cast<decltype(frontend.setCurrent)>(Callee(Find("80 B9 00 04 00 00 00 6A 01 74 0B 6A 00 E8 ? ? ? ? 83 C4 08 C3 8D 41 10 50 E8", 13)));
            // The end of FUN_005a90a0: CALL FUN_00923950 (text flush), then the toolkit draw; and its page dim, CALL FUN_008d4750(rect, colour)
            frontend.textFlush = reinterpret_cast<decltype(frontend.textFlush)>(Callee(Find("E8 ? ? ? ? 68 ? ? ? ? B9 ? ? ? ? E8 ? ? ? ? 8B 10 8B C8 FF 92 5C 01 00 00")));
            frontend.drawRect = reinterpret_cast<decltype(frontend.drawRect)>(Callee(Find("50 8D 44 24 1C 50 C7 44 24 20 00 00 00 00 C7 44 24 2C 00 00 00 00 F3 0F 11 44 24 24 E8 ? ? ? ? 83 C4 08", 28)));
            // The menu flag (FUN_005d48a0), the video editor flag and the page (FUN_005a90a0)
            frontend.menuOpen = Read<uint8_t*>(Find("80 3D ? ? ? ? 00 8B 4D 08 8B 01 75 0E 6A 00 6A 10 68 50 09 00 00", 2));
            frontend.videoEditor = Read<uint8_t*>(Find("80 3D ? ? ? ? 00 75 17 68 3B 28 AA 3F B9", 2));
            frontend.page = Read<int32_t*>(Find("83 3D ? ? ? ? 03 75 1D 6A 00 E8 ? ? ? ? 83 C4 04 84 C0 74 0F E8 ? ? ? ? B9 ? ? ? ? E8 ? ? ? ?", 2));
            // The toolkit sizes its widgets by DAT_017a65b0 (1024/768) / (W/H) in FUN_00db1b30 and FUN_00db1e80
            Point(Find("F3 0F 10 54 24 20 F3 0F 5E C8 F3 0F 10 05 ? ? ? ? F3 0F 5E 15", 14), &fUiReference);
            Point(Find("F3 0F 10 54 24 28 F3 0F 5E C8 F3 0F 10 05 ? ? ? ? F3 0F 5E 15", 14), &fUiReference);
            Point(Find("F3 0F 10 54 24 30 F3 0F 5E C8 F3 0F 10 05 ? ? ? ? F3 0F 5E 15", 14), &fUiReference);
            Point(Find("F3 0F 10 57 04 F3 0F 5E C8 F3 0F 10 05 ? ? ? ? F3 0F 59 15", 13), &fUiReference);
            // The menu cursor integrates mouse deltas scaled by 1/1500 in FUN_0069db90
            auto cursorGain = Find("0F 5B E4 F3 0F 59 05 ? ? ? ? F3 0F 59 C4 F3 0F 58 C8", 7);
            fStockCursorGain = fCursorGain = *Read<float*>(cursorGain);
            Point(cursorGain, &fCursorGain);
            // FUN_0059f940, menu open and page setup: PUSH EBP; MOV EBP,ESP; AND ESP,-16; MOV EAX,0x2e68; CALL (stack probe)
            openingEntry = Find("55 8B EC 83 E4 F0 B8 68 2E 00 00 E8 ? ? ? ? A1 ? ? ? ? 33 C4 89 84 24 64 2E 00 00 56 57 C7 05");

            // The pause map, which WidescreenFix draws across the full width, picks under the squeezed menu cursor
            toPixels = Redirect(Find("8D 44 24 08 50 8D 44 24 14 50 F3 0F 59 C4 F3 0F 58 C1 F3 0F 11 44 24 14 E8 ? ? ? ? 57 8D 44 24 1C 56 50 E8", 24), ToPixels);

            // FUN_00b017c0: PUSH 0; PUSH [ESI+0x540] (render id); CALL FUN_0094ad40 (last script layer); PUSH 1; PUSH 1; ...
            drawScriptGraphics = Redirect(Find("6A 00 FF B6 40 05 00 00 E8 ? ? ? ? 6A 01 6A 01 FF B6 40 05 00 00 E8", 8), ViewportScriptGraphics);
            // FUN_00addcd0: PUSH ESI; MOV ESI,ECX; PUSH 0; PUSH [ESI+0x940]; CALL FUN_0094ad40; ADD ESP,8; MOV byte [ESI+0x20],1
            Redirect(Find("56 8B F1 6A 00 FF B6 40 09 00 00 E8 ? ? ? ? 83 C4 08 C6 46 20 01", 11), Script2dGraphics, drawScriptGraphics);
            // The in-game frame code: CMP [html viewport id],0; MOV BL,[menu open]; MOV byte [x],1; JG
            htmlViewportId = Read<int32_t*>(Find("83 3D ? ? ? ? 00 8A 1D ? ? ? ? C6 05 ? ? ? ? 01 7F", 2));

        }
        catch (const hook::txn_exception&)
        {
            return;
        }

        for (auto& patch : patches)
        {
            if (patch.call)
            {
                injector::MakeCALL(patch.at, patch.to, true);
            }
            else
            {
                injector::WriteMemory(patch.at, patch.to, true);
            }
        }

        // FUN_008ff3d0 keeps the HUD item index in ESI; the subtitles (item type 6) wrap at fixed screen fractions, 0.22..0.73 in
        // game and 0.1..0.9 in cutscenes
        static auto subtitleWrapHook = safetyhook::create_mid(subtitleCall, [](SafetyHookContext& regs)
        {
            auto item = Read<uintptr_t>(itemArray + regs.esi * 4);
            if (item && Read<uint32_t>(item + 8) == 6)
            {
                auto wrap = reinterpret_cast<float*>(regs.esp);
                auto share = HudShare();
                wrap[0] = Squeeze(wrap[0], share);
                wrap[1] = Squeeze(wrap[1], share);
            }
        });

        // While the menu is drawn, m00 = 2s/W and m30 = -s map pixel x in [0, W] onto the middle s of clip space
        static auto blitMatrixHook = safetyhook::create_mid(uploadCall, [](SafetyHookContext& regs)
        {
            if (matrixSqueeze > 0.0f)
            {
                auto matrix = *reinterpret_cast<float**>(regs.esp + 4);
                matrix[0] *= matrixSqueeze;
                matrix[12] *= matrixSqueeze;
            }
        });

        // The first page is laid out before the open flag is set
        static auto menuOpeningHook = safetyhook::create_mid(openingEntry, [](SafetyHookContext& regs)
        {
            opening = true;
            Refresh();
        });

        applied = true;
    }
}

export namespace CenteredHud
{
    // The aspect the phone's x scale is computed for: 16:9 while the phone is drawn in the centered area
    float PhoneAspect(float screenAspect)
    {
        return (applied && Share(screenAspect) > 0.0f) ? k16by9 : screenAspect;
    }

    // The share of the screen width the menu cursor moves in while the menu is squeezed, 1 otherwise
    float MenuCursorShare()
    {
        auto share = MenuSqueeze();
        return share > 0.0f ? share : 1.0f;
    }

    // The aspect the frontend lays itself out for on this thread: 16:9 while it lays out or draws the squeezed menu
    float MenuLayoutAspect(float screenAspect)
    {
        return Virtual16by9() ? k16by9 : screenAspect;
    }
}

class CenteredHudFix
{
public:
    CenteredHudFix()
    {
        FusionFix::onInitEventAsync() += []()
        {
            CenteredHud::Init();
        };
    }
} CenteredHudFix;
