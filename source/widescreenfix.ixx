module;

#include <common.hxx>

export module widescreenfix;

import common;
import comvars;
import settings;
import natives;

#define SCREEN_WIDTH ((float)*rage::grcDevice::ms_nActiveWidth)
#define SCREEN_HEIGHT ((float)*rage::grcDevice::ms_nActiveHeight)
#define DEFAULT_SCREEN_WIDTH 1280.0f
#define DEFAULT_SCREEN_HEIGHT 720.0f
#define DEFAULT_ASPECT_RATIO (16.0f / 9.0f)
#define SCREEN_ASPECT_RATIO Natives::GetAspectRatio()

class WidescreenFix 
{
public:
    static inline float f075 = 0.75f;
    static inline float f01152 = 1152.0f;
    static inline float bordersMult = -5.0f;
    static inline uint32_t bordersTimer = 0;
    static inline bool dontRenderBordersThisFrame = false;

    // The in-game computer (desktop, browser, websites) is laid out for a 16:9 screen: its scripts draw the
    // surroundings of the 4:3 page with stretched 2D (no widescreen format). While a web page is shown, that 2D is
    // kept in a centred 16:9 frame and the script's mouse is given in the frame's coordinates.
    static inline void* CViewportHtml__vftable = nullptr;
    struct ViewportList { void** data; uint16_t count; };
    static inline ViewportList* viewports = nullptr;

    static bool IsWebPageShown()
    {
        if (!viewports || !viewports->data || !CViewportHtml__vftable)
            return false;
        for (uint16_t i = 0; i < viewports->count; i++)
        {
            if (viewports->data[i] && *reinterpret_cast<void**>(viewports->data[i]) == CViewportHtml__vftable)
                return true;
        }
        return false;
    }

    // How much narrower the 16:9 frame is than the screen (1 without a web page and on 16:9 and narrower screens).
    // Not cached per frame: the computer pauses the game, and with it the game process event.
    static float WebFrameScale()
    {
        if (!IsWebPageShown())
            return 1.0f;
        auto aspectRatio = SCREEN_ASPECT_RATIO;
        return aspectRatio > DEFAULT_ASPECT_RATIO ? DEFAULT_ASPECT_RATIO / aspectRatio : 1.0f;
    }

    static void Update()
    {
        f075 = 0.75f * DEFAULT_ASPECT_RATIO / SCREEN_ASPECT_RATIO;
        f01152 = 1152.0f * DEFAULT_ASPECT_RATIO / SCREEN_ASPECT_RATIO;

        if (bordersTimer < (*CTimer::m_snTimeInMilliseconds))
            bordersMult = -5.0f;
    }

    // Applies a script widescreen format (SET_WIDESCREEN_FORMAT) to a 2D position, size and x range at draw time.
    // Format 0 (stretch) is left alone by the game; on the computer it goes into the 16:9 frame instead.
    static inline SafetyHookInline shApplyWidescreenFormat{};
    static bool __cdecl ApplyWidescreenFormat(int32_t format, float* pos, float* size, float* rangeX)
    {
        if (format == 0)
        {
            auto scale = WebFrameScale();
            if (scale == 1.0f)
                return false;
            if (pos)
                pos[0] = 0.5f + (pos[0] - 0.5f) * scale;
            if (size)
                size[0] *= scale;
            if (rangeX)
            {
                rangeX[0] = 0.5f + (rangeX[0] - 0.5f) * scale;
                rangeX[1] = 0.5f + (rangeX[1] - 0.5f) * scale;
            }
            return true;
        }
        return shApplyWidescreenFormat.ccall<bool>(format, pos, size, rangeX);
    }

    // GET_MOUSE_POSITION: the cursor in the 16:9 frame (the game clamps it to 0..1 the same way)
    static inline injector::hook_back<void(__cdecl*)(float*, float*)> hbGetMousePosition;
    static void __cdecl GetMousePosition(float* x, float* y)
    {
        hbGetMousePosition.fun(x, y);
        if (auto scale = WebFrameScale(); scale != 1.0f)
            *x = std::clamp(0.5f + (*x - 0.5f) / scale, 0.0f, 1.0f);
    }

    // GET_WEB_PAGE_LINK_AT_POSN: the script's frame position back on the screen, where the page is projected
    static inline injector::hook_back<int32_t(__cdecl*)(int32_t, float, float)> hbGetWebPageLinkAtPosn;
    static int32_t __cdecl GetWebPageLinkAtPosn(int32_t htmlViewport, float x, float y)
    {
        return hbGetWebPageLinkAtPosn.fun(htmlViewport, 0.5f + (x - 0.5f) * WebFrameScale(), y);
    }

    static inline void* CSprite2d__DrawRectAddr;
    static void CSprite2d__DrawRect(Vector4 const& pos, Color32 const& col)
    {
        reinterpret_cast<void(__cdecl*)(Vector4 const*, Color32 const*)>(CSprite2d__DrawRectAddr)(&pos, &col);
    }

    static inline void* CSprite2d__DrawAddr;
    static void CSprite2d__Draw(Vector4 const& pos, Color32 const& col) 
    {
        reinterpret_cast<void(__cdecl*)(Vector4 const*, Color32 const*)>(CSprite2d__DrawAddr)(&pos, &col);
    }

    static void DrawBorders(bool letter, bool pillar, bool force) 
    {
        // MISC.CutsceneBorders: 1 Letterbox, 2 Pillarbox, 3 both
        static auto borders = FusionFixSettings.GetRef("PREF_CUTSCENEBORDERS");

        float aspectRatio = SCREEN_ASPECT_RATIO;
        if (force)
            bordersMult = 1.0f;

        // Letterbox
        if (!dontRenderBordersThisFrame && (borders->get() & 1) && letter && aspectRatio < DEFAULT_ASPECT_RATIO)
        {
            float letterboxHeight = SCREEN_WIDTH / DEFAULT_ASPECT_RATIO;
            float letterboxY = (SCREEN_HEIGHT - letterboxHeight) / 2.0f;

            Color32 col = { 0, 0, 0, 255 };
            Vector4 rect = { 0.0f, 0.0f, SCREEN_WIDTH, letterboxY * bordersMult };
            CSprite2d__DrawRect(rect, col);

            rect = { 0.0f, SCREEN_HEIGHT - (letterboxY * bordersMult), SCREEN_WIDTH, SCREEN_HEIGHT };
            CSprite2d__DrawRect(rect, col);
        }

        // Pillarbox
        if (!dontRenderBordersThisFrame && (borders->get() & 2) && pillar && aspectRatio > DEFAULT_ASPECT_RATIO)
        {
            float pillarWidth = SCREEN_WIDTH - (SCREEN_HEIGHT * DEFAULT_ASPECT_RATIO);
            float halfPillarWidth = pillarWidth / 2.0f;
            Color32 col = { 0, 0, 0, 255 };
            Vector4 rect = { 0.0f, 0.0f, halfPillarWidth * bordersMult, SCREEN_HEIGHT };
            CSprite2d__DrawRect(rect, col);

            rect = { SCREEN_WIDTH - (halfPillarWidth * bordersMult), 0.0f, SCREEN_WIDTH, SCREEN_HEIGHT };
            CSprite2d__DrawRect(rect, col);
        }

        bordersTimer = (*CTimer::m_snTimeInMilliseconds) + 500;
        bordersMult += 2.0f * (*CTimer::fTimeStep);
        if (bordersMult >= 1.0f)
            bordersMult = 1.0f;

        dontRenderBordersThisFrame = false;
    }

    static void DrawCutsceneBorders() 
    {
        auto dc = new T_CB_Generic_NoArgs([]() { DrawBorders(true, true, false); });
        if (dc)
            dc->Append();
    }

    static void DrawLoadingScreen(Vector4 rect, float z, float min1, float max1, float min2, float max2, Color32 const& col) 
    {
        // Wide screens use the 16:9 set of loadingscreens*.dat, laid out on the whole screen, so every object (the
        // art and the black fill) goes into a centred 16:9 frame: pillarboxed on wider screens, cropped on 16:10.
        // All of them: telling them apart by their pixel height made the art stretch below 720 lines.
        float aspectRatio = SCREEN_ASPECT_RATIO;
        bool isWide = (aspectRatio > 1.5f);

        if (isWide)
        {
            float scale = DEFAULT_ASPECT_RATIO / aspectRatio;
            float centre = SCREEN_WIDTH / 2.0f;
            rect.fX = centre + (rect.fX - centre) * scale;
            rect.fZ = centre + (rect.fZ - centre) * scale;
        }

        CSprite2d__Draw(rect, col);

        if (isWide) 
            DrawBorders(false, true, true);
    }

    static inline injector::hook_back<void(__cdecl*)(void)> hbInitEpisodeSelect;
    static void InitEpisodeSelect() 
    {
        Update();
        hbInitEpisodeSelect.fun();
    }

    static inline injector::hook_back<void(__fastcall*)(int32_t*, void*, int32_t, const char*, int32_t*, int32_t)> hbUITexture__Load;
    static void __fastcall UITexture__Load(int32_t* ui, void*, int32_t txdSlot, const char* texName, int32_t* arg4, int32_t arg5) 
    {
        float aspectRatio = SCREEN_ASPECT_RATIO;
        const char* name = texName;
        if (aspectRatio >= (21.0f / 9.0f))
            name = "offline_21by9";
        else if (aspectRatio >= (16.0f / 9.0f))
            name = "offline_w";
        else if (aspectRatio >= (16.0f / 10.0f))
            name = "offline_16by10";

        hbUITexture__Load.fun(ui, 0, txdSlot, name, arg4, arg5);

        // There is no backdrop wider than 21:9 (64:27, as 2560x1080), so on wider screens (32:9) it is kept at its own
        // aspect, centred, with black sides instead of stretched. The frame is anchored to the four sides of the screen
        // before the texture is set (offsets in UI units, the screen being 768 high); its left and right anchors move in.
        constexpr float backdropAspectRatio = 64.0f / 27.0f;
        if (aspectRatio > 2.4f)
        {
            struct UIAnchor { int32_t point; int32_t pad[3]; float offsetX; };
            struct UIAnchorList { UIAnchor** data; uint16_t count; uint16_t capacity; };
            auto anchors = reinterpret_cast<UIAnchorList*>(reinterpret_cast<uint8_t*>(ui) + 0xBC);
            auto inset = (aspectRatio - backdropAspectRatio) * 768.0f / 2.0f;
            for (uint16_t i = 0; i < anchors->count; i++)
            {
                auto anchor = anchors->data[i];
                if (anchor && anchor->point == 2) // left
                    anchor->offsetX = inset;
                else if (anchor && anchor->point == 8) // right
                    anchor->offsetX = -inset;
            }
        }
    }

    static inline injector::hook_back<void(*)()> hbDrawCameraOverlay;
    static void DrawCameraOverlay()
    {
        dontRenderBordersThisFrame = true;
        return hbDrawCameraOverlay.fun();
    }

    WidescreenFix()
    {
        FusionFix::onInitEventAsync() += []()
        {
            // CSprite2d
            auto pattern = find_pattern("6A 00 E8 ? ? ? ? 8B 44 24 08 83 C4 04 F3 0F 10 40", "6A 00 E8 ? ? ? ? D9 EE 8B 44 24 0C");
            CSprite2d__DrawRectAddr = pattern.get_first(0);

            pattern = find_pattern("8B 44 24 04 6A 00 FF 74 24 0C", "8B 44 24 08 D9 E8 6A 00");
            CSprite2d__DrawAddr = pattern.get_first(0);

            // Fix "auto" aspect ratio
            pattern = find_pattern("75 67 E8 75 CB FC FF", "75 67 E8 ? ? ? ? 85 C0 7E 5E E8 ? ? ? ? 48", "75 5B E8 ? ? ? ? 85 C0 7E 52 E8");
            injector::WriteMemory<uint8_t>(pattern.get_first(0), 0xEB, true);

            pattern = find_pattern("6A 01 F3 0F 5E C8 B9", "6A 01 B9 ? ? ? ? E8 ? ? ? ? D9 54 24 1C");
            injector::WriteMemory<uint8_t>(pattern.get_first(1), 0, true);

            // Fix cellphone
            pattern = find_pattern("F3 0F 59 0D ? ? ? ? 0F 28 C1 F3 0F 59 44 24 ? F3 0F 11 44 24 ? 0F 28 C1 F3 0F 59 44 24 ? F3 0F 59 4C 24 ? F3 0F 11 44 24 ? F3 0F 10 44 24", "F3 0F 59 0D ? ? ? ? 0F 28 D1 F3 0F 59 54 24");
            injector::WriteMemory<void*>(pattern.get_first(4), &f075, true);

            // Fix episode menu select
            pattern = find_pattern("E8 ? ? ? ? 8B 4E 08 6A 00 8B 01 6A 10 8B 40 08 68 ? ? ? ? FF D0 85 C0 74 13 68 ? ? ? ? 68 ? ? ? ? 8B C8 E8 ? ? ? ? EB 02 33 C0 8D 4C 24 14 51 8B C8 A3 ? ? ? ? C7 44 24 ? ? ? ? ? E8 ? ? ? ? E8 ? ? ? ? E9", "E8 ? ? ? ? 64 8B 0D ? ? ? ? 8B 11 8B 4A 08 8B 01 8B 50 08 53 6A 10 68 ? ? ? ? FF D2 3B C3 74 1A");
            hbInitEpisodeSelect.fun = injector::GetBranchDestination(pattern.get_first(0)).get();
            injector::MakeCALL(pattern.get_first(0), InitEpisodeSelect);

            // Add new tex offline_21by9
            pattern = find_pattern("E8 ? ? ? ? 64 A1 ? ? ? ? 6A 00 8B 38 6A 10 8B 4F 08 68 ? ? ? ? 8B 01 8B 40 08 FF D0 85 C0 74 15", "E8 ? ? ? ? 55 68 ? ? ? ? 68");
            hbUITexture__Load.fun = injector::GetBranchDestination(pattern.get_first(0)).get();
            injector::MakeCALL(pattern.get_first(0), UITexture__Load);

            pattern = find_pattern("F3 0F 59 0D ? ? ? ? F3 0F 59 D3 F3 0F 5C C1", "F3 0F 59 05 ? ? ? ? F3 0F 59 CA F3 0F 59 CC");
            injector::WriteMemory<void*>(pattern.get_first(4), &f01152, true);

            // Cutscene borders
            pattern = hook::pattern("83 EC 14 B9 ? ? ? ? E8 ? ? ? ? 84 C0");
            injector::MakeJMP(pattern.get_first(0), DrawCutsceneBorders, true);

            // Loadingscreens
            pattern = find_pattern("E8 ? ? ? ? 83 C4 2C E8 ? ? ? ? E9", "E8 ? ? ? ? 83 C4 2C E8 ? ? ? ? EB 73");
            injector::MakeCALL(pattern.get_first(0), DrawLoadingScreen, true);

            pattern = find_pattern("E8 ? ? ? ? 83 C4 2C E8 ? ? ? ? 8A 54 24 0F", "E8 ? ? ? ? 83 C4 2C E8 ? ? ? ? 8B 35");
            injector::MakeCALL(pattern.get_first(0), DrawLoadingScreen, true);

            pattern = find_pattern("E8 ? ? ? ? 83 C4 2C 5F 5E 5B 83 C4 28", "E8 ? ? ? ? 83 C4 2C 5F 5E 5D 5B 83 C4 20");
            injector::MakeCALL(pattern.get_first(0), DrawLoadingScreen, true);

            // Camera overlay
            pattern = find_pattern("31 47 04 5F 5E C3 5F 5E E9", "31 46 04 5F 5E C3 5F 5E E9");
            hbDrawCameraOverlay.fun = injector::MakeJMP(pattern.get_first(8), DrawCameraOverlay, true).get();

            pattern = find_pattern("C7 47 ? ? ? ? ? EB 02 33 FF 8B 07 8B CF FF 50 08 25 ? ? ? ? 79 05 48 83 C8 F0 40 BE ? ? ? ? 2B F0 81 E6 ? ? ? ? 79 05 4E 83 CE F0 46 8B 07 8B CF FF 50 08 03 C6 99 83 E2 0F 03 C2 C1 F8 04 C1 E0 0E 33 47 04 25 ? ? ? ? 31 47 04 5F 5E C3 5F",
                                   "C7 40 ? ? ? ? ? EB 02 33 F6 8B 16 8B 42 08 8B CE FF D0 25 ? ? ? ? 79 05 48 83 C8 F0 40 BF ? ? ? ? 2B F8 81 E7 ? ? ? ? 79 05 4F 83 CF F0 47 8B 16 8B 42 08 8B CE FF D0 03 C7 99 83 E2 0F 03 C2 C1 F8 04 C1 E0 0E 33 46 04 25 ? ? ? ? 31 46 04 5F 5E C3");
            injector::WriteMemory(pattern.get_first(3), DrawCameraOverlay, true);

            // Pause menu map: the radar viewport's camera uses a fixed 16:9 (or 4:3) aspect in the frontend, so the map was
            // stretched on wider screens. It covers the whole screen there, so the screen's aspect is the right one.
            // The radar itself (1.0, a square) is left alone.
            pattern = find_pattern("80 3D ? ? ? ? 00 F3 0F 10 05 ? ? ? ? 56 57 8B F9 74 20", "80 3D ? ? ? ? 00 56 57 8B F9 75 0A F3 0F 10 05");
            static auto FrontendMapAspectHook = safetyhook::create_mid(pattern.get_first(0x42), [](SafetyHookContext& regs)
            {
                if (regs.xmm0.f32[0] != 1.0f)
                    regs.xmm0.f32[0] = SCREEN_ASPECT_RATIO;
            });

            // Zoomed in, the pause menu map only draws the tiles three columns left and right of the centre one, which is
            // as wide as a 16:9 screen; wider screens got a cut-off map. The range grows with the screen, kept inside
            // the map (a column past its edge would draw a tile of the next row).
            {
                static auto mapColumns = *find_pattern("A1 ? ? ? ? 48 0F 28 CF F3 0F 58 0D", "A1 ? ? ? ? 0F 28 D8 F3 0F 58 05").get_first<int32_t*>(1);
                auto pattern = hook::pattern("55 57 8B FE 2B F8 8D 2C 30 3B FD");
                static auto FrontendMapTilesHook = safetyhook::create_mid(pattern.get_first(9), [](SafetyHookContext& regs)
                {
                    // eax: the column range (3 in the frontend, 1 for the radar), esi: the centre column, edi..ebp: the range
                    if (regs.eax != 3)
                        return;
                    auto range = (int32_t)std::ceil(3.0f * SCREEN_ASPECT_RATIO / DEFAULT_ASPECT_RATIO - 0.01f);
                    if (range <= 3)
                        return;
                    regs.edi = std::max((int32_t)regs.esi - range, 0);
                    regs.ebp = std::min((int32_t)regs.esi + range, *mapColumns - 1);
                });
            }

            // Area blips on the pause menu map: the vertical radius is the horizontal one times that same fixed aspect
            pattern = hook::pattern("74 0A F3 0F 59 05 ? ? ? ? EB 08 F3 0F 59 05 ? ? ? ? F3 0F 11 44 24");
            static auto FrontendMapAreaBlipHook = safetyhook::create_mid(pattern.get_first(20), [](SafetyHookContext& regs)
            {
                // al: the widescreen test the multiplier was picked by
                regs.xmm0.f32[0] *= SCREEN_ASPECT_RATIO / ((regs.eax & 0xFF) ? DEFAULT_ASPECT_RATIO : (4.0f / 3.0f));
            });

            // Websites: the page is 4:3, pillarboxed on wide screens with a fixed 16:9 orthographic window ([-1/6, 7/6]
            // across), so it was stretched on 16:10 and wider screens. The window follows the screen's aspect instead;
            // link picking goes through the same projection, so the mouse keeps hitting the links.
            {
                auto pattern = hook::pattern("C7 04 24 AB AA 2A BE E8");
                auto addr = !pattern.empty() ? pattern.get_first(7) : hook::get_pattern("D9 1C 24 E8 ? ? ? ? B8 ? ? ? ? 89 44 24 24", 3);
                static auto WebPageWindowHook = safetyhook::create_mid(addr, [](SafetyHookContext& regs)
                {
                    // ortho(left, right, bottom, top, near, far); the 4:3 branch passes left = 0
                    auto window = reinterpret_cast<float*>(regs.esp);
                    if (window[0] < 0.0f)
                    {
                        auto halfWidth = SCREEN_ASPECT_RATIO * (3.0f / 4.0f) * 0.5f;
                        window[0] = 0.5f - halfWidth;
                        window[1] = 0.5f + halfWidth;
                    }
                });
            }

            // In-game computer: its stretched 2D goes into a centred 16:9 frame while a web page is shown
            {
                auto pattern = find_pattern("53 8B 5C 24 08 85 DB 75 04 32 C0 5B C3 6A 01 B9", "56 8B 74 24 08 85 F6 75 04 32 C0 5E C3 6A 01 B9");
                viewports = *pattern.get_first<ViewportList*>(16);
                shApplyWidescreenFormat = safetyhook::create_inline(pattern.get_first(0), ApplyWidescreenFormat);

                pattern = find_pattern("C7 06 ? ? ? ? C7 86 60 07 00 00", "C7 06 ? ? ? ? 89 9E 60 07 00 00");
                CViewportHtml__vftable = *pattern.get_first<void*>(2);
            }

            FusionFix::onGameProcessEvent() += []()
            {
                Update();
            };
        };

        FusionFix::onInitEvent() += []()
        {
            // Script
            {
                hbGetMousePosition.fun = NativeOverride::Register(Natives::NativeHashes::GET_MOUSE_POSITION, GetMousePosition, "E8 ? ? ? ? 83 C4 08", 30);
                hbGetWebPageLinkAtPosn.fun = NativeOverride::Register(Natives::NativeHashes::GET_WEB_PAGE_LINK_AT_POSN, GetWebPageLinkAtPosn, "E8 ? ? ? ? 8B 0E 83 C4 0C", 40);
            }
        };
    }
} WidescreenFix;