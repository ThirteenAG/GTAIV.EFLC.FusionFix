module;

#include <common.hxx>
#include <upscaler/dxvk_interop.hpp>
#include <upscaler/graphics_api.h>
#include <d3dx9tex.h>
#include <intrin.h>
#include <atomic>

export module hdr;

import common;
import comvars;
import d3dx9_43;
import settings;
import windowed;

#define IDR_HDR_OUTPUT_PS 3101
#define IDR_HDR_CALIBRATION_PS 3102
#define IDR_HDR_CALIBRATION_LOGO 3103

#ifndef SAFE_RELEASE
#define SAFE_RELEASE(p) { if (p) { (p)->Release(); (p)=NULL; } }
#endif

// HDR output through DXVK (Vulkan) or Fusion Fix's D3D9On12 driver (DirectX 12).
//
// With HDR enabled the back buffer is 16-bit float. The frame is composited exactly like in SDR, only nothing clips at
// 1.0 any more: the game's post processing writes gamma encoded color with highlights above 1.0 and the interface is
// drawn on top of it. The last pass of the frame converts the back buffer to scRGB, with paper white and a roll-off
// towards the peak brightness of the display, and the swap chain is switched to the extended sRGB color space.
// - Vulkan: the device is created with a D3DFMT_A16B16G16R16F back buffer (DXVK has to unlock that format), the color
//   space and the HDR metadata are set with DXVK's swap chain extension.
// - DirectX 12: the system's D3D9 runtime refuses float back buffers, so the back buffer keeps its 8-bit format for the
//   runtime and the game, and the driver stores it as 16-bit float and presents it in scRGB with the HDR metadata
//   (D3D9On12SetHdrOutput, FusionFixUtils upscaler/D3D9On12Extensions.h). Only a window's swap chain has a color
//   space: with HDR on, fullscreen is a window over the monitor instead of exclusive (graphics_api.h
//   SetWindowedOnDriver, ApplyResetWindowMode; windowed.ixx places it). Keep this presentation path for the
//   session, including SDR, so focus recovery never transfers the window between runtime-owned exclusive
//   presentation and a driver-owned flip swap chain.
//   Turning HDR on or off resets the device like on
//   Vulkan, at any time (OnGameProcess, ModeChangeGate).

// The brightness sliders of the HDR category of Display are in nits: HDR.PeakBrightnessNits and
// HDR.GameBrightnessNits (brightness of the SDR white of the game and the interface, the default is close to 203, the
// BT.2408 reference white) of settings/GTAIV.EFLC.FusionFix.json
//
// While the HDR page of the menu shows a brightness slider, a calibration card is drawn right of the rows after the
// output pass, in absolute brightness:
// - Peak Brightness: a field at the slider's value with the Fusion Fix logo at 10000 nits, the most the signal
//   carries. The display clips both to its own peak: the logo shows while the slider is below it and disappears into
//   the field once the slider reaches it.
// - Game/UI Brightness: a middle grey card with the logo at the slider's value, the white of the menus and the HUD.
// The rows' info text says what to do (FF_HDRCAL_* texts of text/*FF.txt).

class HDR
{
public:
    static inline bool bFormatsUnlocked = false;      // DXVK
    static inline bool bBackBufferFloat = false;
    static inline bool bOutputActive = false;         // swap chain is in the extended sRGB color space
    static inline int32_t nAppliedState = -1;
    static inline float fPeak = 1000.0f;
    static inline float fPaperWhite = 200.0f;
    static inline float fRollOffStart = 0.8f;
    static inline D3DPRESENT_PARAMETERS* pPresentParams = nullptr;
    static inline D3DFORMAT OriginalBackBufferFormat = D3DFMT_UNKNOWN;

    // DirectX 12: the driver's HDR output (null without the capability), whether the game window's display is in HDR
    // mode (Windows HDR on) and when that was asked last, whether the device is a window over the monitor for HDR
    static inline FusionFixGraphicsApi::HdrOutputFunctions Driver{};
    static inline bool bDisplayHdr = false;
    static inline bool bDisplayChecked = false;
    static inline DWORD nDisplayCheckTick = 0;
    static inline bool bRefreshWindow = false;
    static inline int32_t nResetWanted = -1;          // the output the last Reset requests were for, how many, and
    static inline int32_t nResetRequests = 0;         // whether one is still to come
    static inline bool bResetPending = false;
    static inline DWORD nResetRequestTick = 0;
    static inline UINT nBackBufferWidth = 0;
    static inline UINT nBackBufferHeight = 0;
    // The present parameters the game resets the device with (its own, static), and whether its fullscreen is a window
    // over the monitor since the last Reset (ApplyResetWindowMode)
    static inline D3DPRESENT_PARAMETERS* pResetParams = nullptr;
    static inline bool bForcedWindowed = false;
    static inline IDirect3DDevice9* pStatusDevice = nullptr;
    static inline ID3D12Device* pD3D12Device = nullptr;

    static inline rage::grcRenderTargetPC* CompositeRT = nullptr;
    static inline IDirect3DPixelShader9* OutputPS = nullptr;
    static inline IDirect3DPixelShader9* CalibrationPS = nullptr;
    static inline IDirect3DTexture9* CalibrationLogo = nullptr;

    // The brightest signal: 10000 nits in scRGB, where 1.0 is 80 nits
    static constexpr float MaxSignal = 10000.0f / 80.0f;
    // Middle grey of the Game/UI Brightness card, of the white
    static constexpr float MiddleGrey = 0.18f;
    // The card: height and top of the screen's height, right edge from the screen's centre in screen heights, width
    // to height
    static constexpr float CardHeight = 0.22f;
    static constexpr float CardTop = 0.42f;
    static constexpr float CardRight = 0.62f;
    static constexpr float CardAspect = 2.0f;

    // The game has a preference named PREF_HDR of its own
    static constexpr auto PreferenceName = "PREF_HDR_OUTPUT";

    // The display mode the game switches to before the next frame, with a full device reset (resolution change)
    static inline int32_t* pRequestedWidth = nullptr;
    static inline int32_t* pRequestedHeight = nullptr;
    static inline int32_t* pRequestedRefreshRate = nullptr;
    static inline int32_t* pRefreshRate = nullptr;

    static bool IsEnabled()
    {
        static auto hdr = FusionFixSettings.GetRef(PreferenceName);
        return hdr && hdr->get();
    }

    // ---------------------------------------------------------------------------------------------
    // DirectX 12

    static bool OnDriver()
    {
        return Driver.set != nullptr;
    }

    static HWND GameWindow()
    {
        if (gWnd)
            return gWnd;
        return pPresentParams && pPresentParams->hDeviceWindow ? pPresentParams->hDeviceWindow : GetDesktopWindow();
    }

    // Whether the game window's display is in HDR mode. Cheap: the driver keeps DXGI's outputs until they change.
    static bool CheckDisplay()
    {
        D3D9ON12_HDR_OUTPUT_STATUS status{ sizeof(status) };
        bDisplayHdr = OnDriver() && SUCCEEDED(Driver.query(nullptr, GameWindow(), &status)) && (status.Flags & D3D9ON12_HDR_STATUS_DISPLAY_HDR);
        bDisplayChecked = true;
        nDisplayCheckTick = GetTickCount();
        return bDisplayHdr;
    }

    // HDR output on the driver: chosen, and the display in HDR mode
    static bool DriverWanted()
    {
        if (!OnDriver())
            return false;
        if (!bDisplayChecked)
            CheckDisplay();
        return IsEnabled() && bDisplayHdr;
    }

    // For every device of the game: the float swap chain buffers are made at the next Reset, the metadata applies at once
    static void SendDriverOutput()
    {
        D3D9ON12_HDR_OUTPUT output{ sizeof(output) };
        output.Flags = DriverWanted() ? D3D9ON12_HDR_OUTPUT_ENABLE : 0;
        output.ColorSpace = D3D9ON12_HDR_COLOR_SPACE_SCRGB_LINEAR;
        output.MaxLuminance = GetPeak();
        output.MinLuminance = 0.0f;
        output.MaxContentLightLevel = GetPeak();
        output.MaxFrameAverageLightLevel = GetPaperWhite();
        Driver.set(nullptr, &output);
    }

    // The driver's state of the device: float buffers, scRGB present
    static bool QueryDevice(IDirect3DDevice9* device, D3D9ON12_HDR_OUTPUT_STATUS& status)
    {
        if (device != pStatusDevice)
        {
            pStatusDevice = device;
            pD3D12Device = nullptr;
            IDirect3DDevice9On12* on12 = nullptr;
            if (SUCCEEDED(device->QueryInterface(__uuidof(IDirect3DDevice9On12), reinterpret_cast<void**>(&on12))) && on12)
            {
                // Only identifies the device, the D3D9 device keeps it alive
                if (SUCCEEDED(on12->GetD3D12Device(IID_PPV_ARGS(&pD3D12Device))) && pD3D12Device)
                    pD3D12Device->Release();
                on12->Release();
            }
        }
        status = { sizeof(status) };
        return pD3D12Device && SUCCEEDED(Driver.query(pD3D12Device, nullptr, &status));
    }

    // On the game's thread, every frame
    static void OnGameProcess()
    {
        if (!OnDriver())
            return;

        // Once a second: the window moved to another display, Windows HDR turned on or off. The menu follows
        // (availability), the output the next frame.
        if (GetTickCount() - nDisplayCheckTick >= 1000)
        {
            auto before = bDisplayHdr;
            if (CheckDisplay() != before)
                FusionFixSettings.RefreshAvailability(PreferenceName);
        }

        // The swap chain buffers follow HDR on or off with a Reset: one request at a time, the next only after
        // the Reset (the game's mode change must not start again while it runs), two per change in case the driver can't make them float (a wrapper's device).
        // Any time, also during gameplay: the mode change waits for the start of the game's next frame
        // (ModeChangeGate).
        int32_t wanted = DriverWanted() ? 1 : 0;
        if (wanted != nResetWanted)
        {
            nResetWanted = wanted;
            nResetRequests = 0;
        }
        if (bResetPending && GetTickCount() - nResetRequestTick > 10000)
            bResetPending = false;
        if (bBackBufferFloat != (wanted != 0) && !bResetPending && nResetRequests < 2 && RequestReset())
        {
            ++nResetRequests;
            bResetPending = true;
            nResetRequestTick = GetTickCount();
        }

        // The window follows a change in the selected fullscreen mode
        if (bRefreshWindow)
        {
            bRefreshWindow = false;
            RefreshFullscreenWindow();
        }
    }

    // The game's fullscreen choice (a window over the monitor on the HDR-capable driver)
    static bool GameFullscreen()
    {
        return rage::grcWindow::ms_bWindowed && !*rage::grcWindow::ms_bWindowed;
    }

    // Before the game's Reset: keep fullscreen on the driver's windowed presentation path for both HDR and SDR.
    // graphics_api.h does it at device creation and at its Reset hook (SetWindowedOnDriver); the
    // game's own present parameters are changed here too, right before its Reset call (the game builds them anew for
    // every Reset). The game keeps its fullscreen state (Alt+Enter and the menu work as usual).
    static void ApplyResetWindowMode()
    {
        auto pp = pResetParams;
        if (!pp)
            return;
        bool forced = GameFullscreen();
        if (forced)
        {
            pp->Windowed = TRUE;
            pp->FullScreen_RefreshRateInHz = 0;
        }
        // The window follows after the Reset (windowed.ixx)
        if (forced != bForcedWindowed && GameFullscreen())
            bRefreshWindow = true;
        bForcedWindowed = forced;
    }
    // On the render thread, before the output pass
    static void ApplyDriverOutput(IDirect3DDevice9* device)
    {
        int32_t wanted = DriverWanted() ? 1 : 0;
        if (wanted != nAppliedState)
        {
            nAppliedState = wanted;
            SendDriverOutput();
        }

        D3D9ON12_HDR_OUTPUT_STATUS status{};
        if (QueryDevice(device, status))
            bBackBufferFloat = (status.Flags & D3D9ON12_HDR_STATUS_FLOAT_BUFFERS) != 0;
        // Float buffers go out in scRGB: the output pass converts the frame also when HDR was turned off since the last
        // Reset (then as SDR, RenderOutput)
        bOutputActive = bBackBufferFloat;
    }

    // ---------------------------------------------------------------------------------------------

    // DXVK or the driver with a display in HDR mode (on DXVK the extended sRGB color space is also available through
    // HDR10)
    static bool IsSupported()
    {
        if (OnDriver())
            return CheckDisplay();

        // Vulkan: DXVK's instance of this run must have been made for HDR output (graphics_api.h SetVulkanHdr)
        if (!FusionFixGraphicsApi::VulkanHdrPresentable())
            return false;
        auto device = bFormatsUnlocked ? rage::grcDevice::GetD3DDevice() : nullptr;
        IDirect3DSwapChain9* swapChain = nullptr;
        if (!device || FAILED(device->GetSwapChain(0, &swapChain)) || !swapChain)
            return false;

        bool supported = false;
        ID3D9VkExtSwapchain* ext = nullptr;
        if (SUCCEEDED(swapChain->QueryInterface(__uuidof(ID3D9VkExtSwapchain), reinterpret_cast<void**>(&ext))) && ext)
        {
            supported = ext->CheckColorSpaceSupport(VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT);
            ext->Release();
        }
        swapChain->Release();
        return supported;
    }

    // The back buffer format only changes with a device reset: the game resets the device in the current mode. True
    // when requested.
    static bool RequestReset()
    {
        if (!pRequestedWidth || *pRequestedWidth != 0)
            return false;
        // The game's present parameters (Vulkan: CreateDevice), or the back buffer's size of the last frame
        UINT width = pPresentParams ? pPresentParams->BackBufferWidth : nBackBufferWidth;
        UINT height = pPresentParams ? pPresentParams->BackBufferHeight : nBackBufferHeight;
        if (!width || !height)
            return false;
        *pRequestedHeight = static_cast<int32_t>(height);
        *pRequestedRefreshRate = *pRefreshRate;
        *pRequestedWidth = static_cast<int32_t>(width);
        return true;
    }

    // DirectX 12: OnGameProcess and ApplyDriverOutput, in the next frame
    static void OnOutputChanged(int32_t)
    {
        nAppliedState = -1;
        if (bFormatsUnlocked && bBackBufferFloat != IsEnabled())
            RequestReset();
    }

    static float GetPaperWhite()
    {
        return fPaperWhite;
    }

    static float GetPeak()
    {
        return fPeak;
    }

    // Saved in nits in the [HDR] section of the cfg, with the other menu settings. The values in effect, also after
    // a change in the menu or an editor.
    static void LoadBrightness()
    {
        auto& settings = SettingsPlugin();
        fPeak = static_cast<float>(std::max(settings.Int("HDR.PeakBrightnessNits"), 1));
        fPaperWhite = static_cast<float>(std::max(settings.Int("HDR.GameBrightnessNits"), 1));
        // The HDR metadata of the swap chain follows
        nAppliedState = -1;
    }

    // ---------------------------------------------------------------------------------------------
    // Mode changes (Reset) only while the render thread is idle
    //
    // The game changes the display mode (a resolution, Alt+Enter, HDR on or off: the device's Reset with the release
    // and the recreation of every render target) on its update thread, wherever it finds the request: at the start of
    // its frame, after it waited for the render thread to finish the last one, and in its streaming and loading loops,
    // while the render thread may still draw the last frame. There the render targets go away under the frame (the
    // ambient occlusion pass read a destroyed target). During gameplay a request found in those loops waits for the
    // start of the next frame, a few milliseconds later; elsewhere (loading, starting, no frames) the game changes the
    // mode where it always did.
    static inline uint8_t* pModeChangeRequested = nullptr;
    static inline uint8_t* pDeviceLost = nullptr;
    static inline void* pFrameStartReturn = nullptr;
    static inline std::atomic<DWORD> nLastFrameStartTick = 0;
    static inline SafetyHookInline ModeChangeHook{};

    static void __cdecl ModeChange()
    {
        bool frames = GetTickCount() - nLastFrameStartTick.load() < 500;
        if (frames && _ReturnAddress() != pFrameStartReturn && *pModeChangeRequested && !*pDeviceLost)
            return;
        ModeChangeHook.unsafe_ccall();
    }

    static void InstallModeChangeGate()
    {
        // The update thread's frame start: the render thread has finished the last frame, the mode change is called
        // if requested
        auto frameStart = hook::pattern("80 3D ? ? ? ? 00 57 BF 01 00 00 00 74 ? A1 ? ? ? ? 66 39 78 0A 76 04 66 89 78 0A A1 ? ? ? ? 66 39 78 0A 76 04 66 89 78 0A E8");
        auto modeChange = find_pattern("51 80 3D ? ? ? ? 00 56 57 BF 01 00 00 00 0F 84", "53 56 33 DB 38 1D ? ? ? ? 57 BF 01 00 00 00 0F 84");
        if (frameStart.empty() || modeChange.empty())
            return;
        auto call = frameStart.get_first<uint8_t>(45);
        if (injector::GetBranchDestination(call).as_int() != reinterpret_cast<uintptr_t>(modeChange.get_first()))
            return;
        pModeChangeRequested = *frameStart.get_first<uint8_t*>(2);
        pDeviceLost = *modeChange.get_first<uint8_t*>(*modeChange.get_first<uint8_t>(0) == 0x51 ? 3 : 6);
        pFrameStartReturn = call + 5;
        static auto FrameStart = safetyhook::create_mid(frameStart.get_first(), [](SafetyHookContext&)
        {
            nLastFrameStartTick = GetTickCount();
        });
        ModeChangeHook = safetyhook::create_inline(modeChange.get_first(), ModeChange);
    }

    // ---------------------------------------------------------------------------------------------
    // Device creation

    static void ApplyBackBufferFormat(D3DPRESENT_PARAMETERS* pp)
    {
        if (!pp)
            return;

        if (pp->BackBufferFormat != D3DFMT_A16B16G16R16F)
            OriginalBackBufferFormat = pp->BackBufferFormat;

        if (bFormatsUnlocked && IsEnabled())
            pp->BackBufferFormat = D3DFMT_A16B16G16R16F;
        else if (pp->BackBufferFormat == D3DFMT_A16B16G16R16F && OriginalBackBufferFormat != D3DFMT_UNKNOWN)
            pp->BackBufferFormat = OriginalBackBufferFormat;

        // Render targets recreated after the reset follow the new format
        bBackBufferFloat = pp->BackBufferFormat == D3DFMT_A16B16G16R16F;
    }

    static inline HRESULT(__stdcall* RealCreateDevice)(IDirect3D9*, UINT, D3DDEVTYPE, HWND, DWORD, D3DPRESENT_PARAMETERS*, IDirect3DDevice9**) = nullptr;
    static HRESULT __stdcall CreateDevice(IDirect3D9* d3d, UINT adapter, D3DDEVTYPE type, HWND window, DWORD flags, D3DPRESENT_PARAMETERS* pp, IDirect3DDevice9** device)
    {
        pPresentParams = pp;
        ApplyBackBufferFormat(pp);

        // The game's first attempt asks for a pure device without a vertex-processing mode (0x14).
        // Its fallback uses hardware processing and state queries, which the post-processing passes also need.
        constexpr DWORD vertexProcessing = D3DCREATE_SOFTWARE_VERTEXPROCESSING | D3DCREATE_HARDWARE_VERTEXPROCESSING | D3DCREATE_MIXED_VERTEXPROCESSING;
        if ((flags & D3DCREATE_PUREDEVICE) && !(flags & vertexProcessing))
            flags = (flags & ~D3DCREATE_PUREDEVICE) | D3DCREATE_HARDWARE_VERTEXPROCESSING;

        auto hr = RealCreateDevice(d3d, adapter, type, window, flags, pp, device);
        if (FAILED(hr) && pp && pp->BackBufferFormat == D3DFMT_A16B16G16R16F && OriginalBackBufferFormat != D3DFMT_UNKNOWN)
        {
            // HDR back buffer refused, keep the game running in SDR
            pp->BackBufferFormat = OriginalBackBufferFormat;
            hr = RealCreateDevice(d3d, adapter, type, window, flags, pp, device);
        }

        bBackBufferFloat = SUCCEEDED(hr) && pp && (OnDriver() ? DriverWanted() : pp->BackBufferFormat == D3DFMT_A16B16G16R16F);
        nAppliedState = -1;
        return hr;
    }

    static void OnDirect3DCreated(IDirect3D9* d3d)
    {
        if (!d3d)
            return;

        // DirectX 12: the device's first swap chain buffers are float when HDR output is on
        if (OnDriver())
        {
            // The game's window exists now: the HDR choice can be in effect if it's on an HDR display
            CheckDisplay();
            FusionFixSettings.RefreshAvailability(PreferenceName);
            SendDriverOutput();
            bBackBufferFloat = DriverWanted();
            bForcedWindowed = GameFullscreen();
            nAppliedState = -1;
        }
        else
        {
            ID3D9VkExtInterface* ext = nullptr;
            if (FAILED(d3d->QueryInterface(__uuidof(ID3D9VkExtInterface), reinterpret_cast<void**>(&ext))) || !ext)
                return;

            ext->UnlockAdditionalFormats();
            ext->Release();
            bFormatsUnlocked = true;
        }

        auto vtable = *reinterpret_cast<void***>(d3d);
        constexpr auto CreateDeviceIndex = 16;
        if (vtable[CreateDeviceIndex] != reinterpret_cast<void*>(&CreateDevice))
        {
            RealCreateDevice = reinterpret_cast<decltype(RealCreateDevice)>(vtable[CreateDeviceIndex]);
            injector::WriteMemory(&vtable[CreateDeviceIndex], &CreateDevice, true);
        }
    }

    // ---------------------------------------------------------------------------------------------
    // Swap chain color space

    static void ApplyColorSpace(IDirect3DDevice9* device)
    {
        if (OnDriver())
        {
            ApplyDriverOutput(device);
            return;
        }

        int32_t wanted = IsEnabled() && bBackBufferFloat ? 1 : 0;
        if (wanted == nAppliedState)
            return;

        nAppliedState = wanted;
        bOutputActive = false;

        IDirect3DSwapChain9* swapChain = nullptr;
        if (FAILED(device->GetSwapChain(0, &swapChain)) || !swapChain)
            return;

        ID3D9VkExtSwapchain* ext = nullptr;
        if (SUCCEEDED(swapChain->QueryInterface(__uuidof(ID3D9VkExtSwapchain), reinterpret_cast<void**>(&ext))) && ext)
        {
            if (wanted && ext->CheckColorSpaceSupport(VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT) &&
                SUCCEEDED(ext->SetColorSpace(VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT)))
            {
                VkHdrMetadataEXT metadata{};
                metadata.sType = VK_STRUCTURE_TYPE_HDR_METADATA_EXT;
                metadata.displayPrimaryRed = { 0.640f, 0.330f };
                metadata.displayPrimaryGreen = { 0.300f, 0.600f };
                metadata.displayPrimaryBlue = { 0.150f, 0.060f };
                metadata.whitePoint = { 0.3127f, 0.3290f };
                metadata.maxLuminance = GetPeak();
                metadata.minLuminance = 0.0f;
                metadata.maxContentLightLevel = GetPeak();
                metadata.maxFrameAverageLightLevel = GetPaperWhite();
                ext->SetHDRMetaData(&metadata);
                bOutputActive = true;
            }
            else
            {
                ext->SetColorSpace(VK_COLOR_SPACE_SRGB_NONLINEAR_KHR);
            }
            ext->Release();
        }
        swapChain->Release();
    }

    // ---------------------------------------------------------------------------------------------
    // Output pass

    static HMODULE GetOwnModule()
    {
        HMODULE hm = nullptr;
        GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCWSTR)&GetOwnModule, &hm);
        return hm;
    }

    static IDirect3DPixelShader9* LoadPixelShader(IDirect3DDevice9* device, int resource)
    {
        IDirect3DPixelShader9* shader = nullptr;
        auto hm = GetOwnModule();
        if (auto hRes = FindResourceW(hm, MAKEINTRESOURCEW(resource), RT_RCDATA))
            if (auto hGlob = LoadResource(hm, hRes))
                if (auto buffer = LockResource(hGlob))
                    device->CreatePixelShader((DWORD*)buffer, &shader);
        return shader;
    }

    static bool CreateResources(IDirect3DDevice9* device, const D3DSURFACE_DESC& desc)
    {
        if (!OutputPS)
            OutputPS = LoadPixelShader(device, IDR_HDR_OUTPUT_PS);

        if (CompositeRT && (CompositeRT->mWidth != desc.Width || CompositeRT->mHeight != desc.Height))
            ReleaseResources();

        if (!CompositeRT)
        {
            rage::grcRenderTargetDesc rtDesc{};
            rtDesc.mMultisampleCount = 0;
            rtDesc.field_0 = 1;
            rtDesc.field_12 = 1;
            rtDesc.mDepthRT = nullptr;
            rtDesc.field_8 = 1;
            rtDesc.field_10 = 1;
            rtDesc.field_11 = 1;
            rtDesc.field_24 = false;
            rtDesc.mFormat = rage::GRCFMT_A16B16G16R16F;

            CompositeRT = rage::grcTextureFactory::GetInstance()->CreateRenderTarget("HDRComposite", 3, desc.Width, desc.Height, 64, &rtDesc);
            if (CompositeRT)
            {
                rage::grcDevice::grcResolveFlags resolveFlags{};
                rage::grcTextureFactoryPC::GetInstance()->LockRenderTarget(0, CompositeRT, nullptr);
                rage::grcTextureFactoryPC::GetInstance()->UnlockRenderTarget(0, &resolveFlags);
            }
        }

        return OutputPS && CompositeRT && CompositeRT->mD3DTexture;
    }

    static void ReleaseResources()
    {
        if (CompositeRT)
        {
            CompositeRT->Destroy();
            CompositeRT = nullptr;
        }
    }

    // The calibration card of the selected row of the HDR page, right of the rows. Drawn with the states of the output
    // pass; the card's size follows the height of the screen, like the menu.
    static void DrawCalibration(IDirect3DDevice9* device, const D3DSURFACE_DESC& desc, const std::string& row)
    {
        float card = 0.0f, field = 0.0f, logo = 0.0f;
        if (row == "HDR.PeakBrightnessNits")
        {
            field = GetPeak() / 80.0f;
            logo = MaxSignal;
        }
        else if (row == "HDR.GameBrightnessNits")
        {
            logo = GetPaperWhite() / 80.0f;
            field = logo * MiddleGrey;
        }
        else
            return;

        if (!CalibrationPS)
            CalibrationPS = LoadPixelShader(device, IDR_HDR_CALIBRATION_PS);
        if (!CalibrationLogo)
            D3DXCreateTextureFromResourceExW(device, GetOwnModule(), MAKEINTRESOURCEW(IDR_HDR_CALIBRATION_LOGO), D3DX_DEFAULT_NONPOW2, D3DX_DEFAULT_NONPOW2,
                D3DX_DEFAULT, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, D3DX_FILTER_NONE, D3DX_FILTER_BOX, 0, nullptr, nullptr, &CalibrationLogo);
        if (!CalibrationPS || !CalibrationLogo)
            return;

        D3DSURFACE_DESC logoDesc{};
        CalibrationLogo->GetLevelDesc(0, &logoDesc);

        // The card, in pixels
        auto screenWidth = static_cast<float>(desc.Width);
        auto screenHeight = static_cast<float>(desc.Height);
        auto height = std::round(screenHeight * CardHeight);
        auto width = std::round(height * CardAspect);
        auto right = std::round(screenWidth * 0.5f + screenHeight * CardRight);
        auto left = right - width;
        auto top = std::round(screenHeight * CardTop);
        auto bottom = top + height;

        // The field inside a dark frame, the logo across most of the field
        auto frame = std::round(height * 0.08f);
        auto logoWidth = (width - 2.0f * frame) * 0.9f;
        auto logoHeight = logoWidth * static_cast<float>(logoDesc.Height) / std::max(static_cast<float>(logoDesc.Width), 1.0f);
        float constants[3][4] =
        {
            { card, field, logo, 0.0f },
            { frame / width, frame / height, 1.0f - frame / width, 1.0f - frame / height },
            { 0.5f - logoWidth * 0.5f / width, 0.5f - logoHeight * 0.5f / height, 0.5f + logoWidth * 0.5f / width, 0.5f + logoHeight * 0.5f / height },
        };

        device->SetPixelShader(CalibrationPS);
        device->SetPixelShaderConstantF(0, &constants[0][0], 3);
        device->SetTexture(0, CalibrationLogo);
        device->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
        device->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
        device->SetSamplerState(0, D3DSAMP_MIPFILTER, D3DTEXF_LINEAR);

        struct ScreenVertex { float x, y, z, rhw, u, v; };
        ScreenVertex vertices[4] =
        {
            { left - 0.5f,  top - 0.5f,    0.0f, 1.0f, 0.0f, 0.0f },
            { left - 0.5f,  bottom - 0.5f, 0.0f, 1.0f, 0.0f, 1.0f },
            { right - 0.5f, top - 0.5f,    0.0f, 1.0f, 1.0f, 0.0f },
            { right - 0.5f, bottom - 0.5f, 0.0f, 1.0f, 1.0f, 1.0f },
        };
        device->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, vertices, sizeof(ScreenVertex));
    }

    static void RenderOutput()
    {
        // The HDR page of the menu was drawn in this frame
        auto pageRow = FusionFixSettings.TakeHdrPageRow();

        auto device = rage::grcDevice::GetD3DDevice();
        if (!device)
            return;

        IDirect3DSurface9* backBuffer = nullptr;
        if (FAILED(device->GetRenderTarget(0, &backBuffer)) || !backBuffer)
            return;

        D3DSURFACE_DESC desc{};
        backBuffer->GetDesc(&desc);
        nBackBufferWidth = desc.Width;
        nBackBufferHeight = desc.Height;
        // On the driver the game's back buffer keeps its 8-bit format, the driver tells (ApplyColorSpace)
        if (!OnDriver())
            bBackBufferFloat = desc.Format == D3DFMT_A16B16G16R16F;

        // Also with the 8-bit back buffer: after HDR was turned off, the swap chain goes back to sRGB
        ApplyColorSpace(device);

        if (!bOutputActive || !CreateResources(device, desc))
        {
            backBuffer->Release();
            return;
        }

        IDirect3DSurface9* compositeSurface = nullptr;
        CompositeRT->mD3DTexture->GetSurfaceLevel(0, &compositeSurface);
        if (!compositeSurface || FAILED(device->StretchRect(backBuffer, nullptr, compositeSurface, nullptr, D3DTEXF_POINT)))
        {
            SAFE_RELEASE(compositeSurface);
            backBuffer->Release();
            return;
        }

        // Save the state touched by the pass
        IDirect3DPixelShader9* oldPS = nullptr;
        IDirect3DVertexShader9* oldVS = nullptr;
        IDirect3DVertexDeclaration9* oldDecl = nullptr;
        IDirect3DBaseTexture9* oldTexture = nullptr;
        IDirect3DVertexBuffer9* oldStream = nullptr;
        UINT oldOffset = 0, oldStride = 0;
        DWORD oldFVF = 0;
        D3DVIEWPORT9 oldViewport{};
        float oldConstants[3][4]{};
        device->GetPixelShader(&oldPS);
        device->GetVertexShader(&oldVS);
        device->GetVertexDeclaration(&oldDecl);
        device->GetFVF(&oldFVF);
        device->GetTexture(0, &oldTexture);
        device->GetStreamSource(0, &oldStream, &oldOffset, &oldStride);
        device->GetViewport(&oldViewport);
        device->GetPixelShaderConstantF(0, &oldConstants[0][0], 3);

        constexpr D3DRENDERSTATETYPE renderStates[] =
        {
            D3DRS_ZENABLE, D3DRS_ZWRITEENABLE, D3DRS_ALPHABLENDENABLE, D3DRS_ALPHATESTENABLE, D3DRS_STENCILENABLE,
            D3DRS_CULLMODE, D3DRS_COLORWRITEENABLE, D3DRS_SCISSORTESTENABLE, D3DRS_SRGBWRITEENABLE,
        };
        constexpr DWORD renderValues[] =
        {
            FALSE, FALSE, FALSE, FALSE, FALSE,
            D3DCULL_NONE, 0x0F, FALSE, FALSE,
        };
        DWORD oldRenderStates[std::size(renderStates)]{};
        for (size_t i = 0; i < std::size(renderStates); ++i)
        {
            device->GetRenderState(renderStates[i], &oldRenderStates[i]);
            device->SetRenderState(renderStates[i], renderValues[i]);
        }

        constexpr D3DSAMPLERSTATETYPE samplerStates[] =
        {
            D3DSAMP_ADDRESSU, D3DSAMP_ADDRESSV, D3DSAMP_MINFILTER, D3DSAMP_MAGFILTER, D3DSAMP_MIPFILTER, D3DSAMP_SRGBTEXTURE,
        };
        constexpr DWORD samplerValues[] =
        {
            D3DTADDRESS_CLAMP, D3DTADDRESS_CLAMP, D3DTEXF_POINT, D3DTEXF_POINT, D3DTEXF_NONE, FALSE,
        };
        DWORD oldSamplerStates[std::size(samplerStates)]{};
        for (size_t i = 0; i < std::size(samplerStates); ++i)
        {
            device->GetSamplerState(0, samplerStates[i], &oldSamplerStates[i]);
            device->SetSamplerState(0, samplerStates[i], samplerValues[i]);
        }

        auto paperWhite = GetPaperWhite();
        auto peak = std::max(GetPeak(), paperWhite);
        // DirectX 12, HDR off with float buffers until the next Reset: SDR white at scRGB 1.0, no highlights above
        if (OnDriver() && !DriverWanted())
            paperWhite = peak = 80.0f;
        float constants[4] =
        {
            paperWhite / 80.0f,
            peak / paperWhite,
            fRollOffStart,
            1.0f,
        };

        D3DVIEWPORT9 viewport = { 0, 0, desc.Width, desc.Height, 0.0f, 1.0f };
        device->SetViewport(&viewport);
        device->SetTexture(0, CompositeRT->mD3DTexture);
        device->SetPixelShader(OutputPS);
        device->SetPixelShaderConstantF(0, constants, 1);
        device->SetVertexShader(nullptr);
        device->SetFVF(D3DFVF_XYZRHW | D3DFVF_TEX1);

        struct ScreenVertex { float x, y, z, rhw, u, v; };
        auto width = static_cast<float>(desc.Width);
        auto height = static_cast<float>(desc.Height);
        ScreenVertex vertices[4] =
        {
            { -0.5f,         -0.5f,          0.0f, 1.0f, 0.0f, 0.0f },
            { -0.5f,          height - 0.5f, 0.0f, 1.0f, 0.0f, 1.0f },
            { width - 0.5f,  -0.5f,          0.0f, 1.0f, 1.0f, 0.0f },
            { width - 0.5f,   height - 0.5f, 0.0f, 1.0f, 1.0f, 1.0f },
        };
        device->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, vertices, sizeof(ScreenVertex));

        if (pageRow)
            DrawCalibration(device, desc, *pageRow);

        // Restore
        for (size_t i = 0; i < std::size(renderStates); ++i)
            device->SetRenderState(renderStates[i], oldRenderStates[i]);
        for (size_t i = 0; i < std::size(samplerStates); ++i)
            device->SetSamplerState(0, samplerStates[i], oldSamplerStates[i]);
        device->SetPixelShaderConstantF(0, &oldConstants[0][0], 3);
        device->SetTexture(0, oldTexture);
        // DrawPrimitiveUP clears stream zero, including the calibration draw above.
        device->SetStreamSource(0, oldStream, oldOffset, oldStride);
        device->SetViewport(&oldViewport);
        device->SetPixelShader(oldPS);
        device->SetVertexShader(oldVS);
        if (oldDecl)
            device->SetVertexDeclaration(oldDecl);
        else
            device->SetFVF(oldFVF);

        SAFE_RELEASE(oldPS);
        SAFE_RELEASE(oldVS);
        SAFE_RELEASE(oldDecl);
        SAFE_RELEASE(oldTexture);
        SAFE_RELEASE(oldStream);
        compositeSurface->Release();
        backBuffer->Release();
    }

public:
    HDR()
    {
        FusionFix::onInitEvent() += []()
        {
            InstallModeChangeGate();

            SettingsReader reader;
            fRollOffStart = std::clamp(reader.ReadFloat("HDR", "RollOffStart", 0.8f), 0.0f, 1.0f);

            // An HDR page of Display. Turning HDR on or off resets the device with the other back buffer format,
            // the brightness values take effect immediately.
            FusionFixSettings.RegisterPreference(PreferenceName, "HDR.HDR", OnOutputChanged);
            LoadBrightness();
            SettingsPlugin().OnChange("HDR.PeakBrightnessNits", LoadBrightness);
            SettingsPlugin().OnChange("HDR.GameBrightnessNits", LoadBrightness);

            // The HDR page of Display. The brightness sliders are shown while HDR is on.
            for (auto screen : FusionFixSettings.GetHdrScreens())
            {
                FusionFixSettings.AddSettingRow(screen, "HDR.HDR");
                FusionFixSettings.AddSettingRow(screen, "HDR.PeakBrightnessNits");
                FusionFixSettings.AddSettingRow(screen, "HDR.GameBrightnessNits");
            }

            // The rows' info text says how to set them with their calibration card, or that the cards need HDR output
            constexpr auto SdrNote = "The calibration patterns of the brightness settings appear here when HDR output is on.";
            FusionFixSettings.SetRowHint("HDR.HDR", []
            {
                return bOutputActive && bBackBufferFloat ? FusionFixSettings.GameText("FF_HDRCAL_HDR", "Select a brightness setting below to see its calibration pattern.") :
                    FusionFixSettings.GameText("FF_HDRCAL_SDR", SdrNote);
            });
            FusionFixSettings.SetRowHint("HDR.PeakBrightnessNits", []
            {
                return bOutputActive && bBackBufferFloat ? FusionFixSettings.GameText("FF_HDRCAL_PEAK", "Increase until the logo in the box is barely visible. If your TV has HGIG, turn it on first.") :
                    FusionFixSettings.GameText("FF_HDRCAL_SDR", SdrNote);
            });
            FusionFixSettings.SetRowHint("HDR.GameBrightnessNits", []
            {
                return bOutputActive && bBackBufferFloat ? FusionFixSettings.GameText("FF_HDRCAL_PAPER", "Set to a comfortable brightness for menus and the HUD. The logo in the box shows this brightness.") :
                    FusionFixSettings.GameText("FF_HDRCAL_SDR", SdrNote);
            });

            // HDR can be turned on with the Vulkan or DirectX 12 graphics API (DirectX 12: Fusion Fix's D3D9On12 driver
            // with HDR output, loaded before the plugin's init by graphicsapi.ixx) while Windows HDR is on, else the menu
            // offers only Off. The driver's window decision is asked at every device creation and Reset.
            // Vulkan: the driver presents DXVK's swap chains in HDR only when its instance is made for it, when the game
            // creates its Direct3D object: with HDR on, or a display in HDR mode (turning HDR on later then works too)
            FusionFixGraphicsApi::SetVulkanHdr([] { return IsEnabled() || FusionFixGraphicsApi::AnyDisplayHdr(); });
            Driver = FusionFixGraphicsApi::Running() == FusionFixGraphicsApi::Api::D3D9On12 ? FusionFixGraphicsApi::HdrOutput() : FusionFixGraphicsApi::HdrOutputFunctions{};
            if (OnDriver())
            {
                // A single presentation owner for the lifetime of the device. After exclusive focus
                // recovery, the D3D9 runtime can retain ownership of this HWND even after Reset
                // succeeds, preventing the driver from creating the HDR flip swap chain.
                FusionFixGraphicsApi::SetWindowedOnDriver([] { return true; });
                // The game's present parameters, pushed for its Reset call (dllmain.cpp's reset hooks are next to it)
                auto resetCall = find_pattern("A1 ? ? ? ? 68 ? ? ? ? 8B 08 50 FF 51 40", "8B 08 8B 51 40 68 ? ? ? ? 50 FF D2 8B");
                if (!resetCall.empty())
                    pResetParams = *resetCall.get_first<D3DPRESENT_PARAMETERS*>(6);
            }
            // The saved choice comes back once it's available, checked when the menu opens.
            FusionFixSettings.SetAvailability(PreferenceName, [](int32_t value) -> bool
            {
                return value == 0 || IsSupported();
            });
            FusionFix::onMenuEnterEvent() += []()
            {
                FusionFixSettings.RefreshAvailability(PreferenceName);
            };
            // On the game's thread: in gameplay, and while the menu is shown (the game is paused then)
            FusionFix::onGameProcessEvent() += []()
            {
                OnGameProcess();
            };
            FusionFix::onMenuDrawingEvent() += []()
            {
                OnGameProcess();
            };

            // Requested display mode: width, height and refresh rate, and the refresh rate of the current mode
            auto pattern = find_pattern("8B 15 ? ? ? ? 85 D2 74 ? A1 ? ? ? ? 8B 0D ? ? ? ? A3 ? ? ? ? A3 ? ? ? ? A1 ? ? ? ? 85 C9 0F 45 C1",
                "A1 ? ? ? ? 3B C3 74 ? 8B 0D ? ? ? ? 3B CB 8B 15 ? ? ? ? A3 ? ? ? ? 89 15 ? ? ? ? 89 0D ? ? ? ? A3 ? ? ? ? 89 15 ? ? ? ? 74 ? 89 0D");
            if (!pattern.empty())
            {
                bool legacy = *pattern.get_first<uint8_t>(0) == 0xA1;
                pRequestedWidth = *pattern.get_first<int32_t*>(legacy ? 1 : 2);
                pRequestedHeight = *pattern.get_first<int32_t*>(legacy ? 19 : 11);
                pRequestedRefreshRate = *pattern.get_first<int32_t*>(legacy ? 11 : 17);
                pRefreshRate = *pattern.get_first<int32_t*>(legacy ? 55 : 32);
            }

            // Unlock the float back buffer in DXVK as soon as the game has its IDirect3D9
            pattern = hook::pattern("C6 05 ? ? ? ? 00 FF 15 ? ? ? ? 8B C8 89 0D");
            if (!pattern.empty())
            {
                static auto Direct3DCreated = safetyhook::create_mid(pattern.get_first(13), [](SafetyHookContext& regs)
                {
                    OnDirect3DCreated(reinterpret_cast<IDirect3D9*>(regs.eax));
                });
            }
            else
            {
                pattern = hook::pattern("C6 05 ? ? ? ? 00 E8 ? ? ? ? 3B C6 A3");
                if (!pattern.empty())
                {
                    static auto Direct3DCreated = safetyhook::create_mid(pattern.get_first(12), [](SafetyHookContext& regs)
                    {
                        OnDirect3DCreated(reinterpret_cast<IDirect3D9*>(regs.eax));
                    });
                }
            }

            FusionFix::onBeforeReset() += []()
            {
                ReleaseResources();
                if (OnDriver())
                {
                    // Reset the buffers while preserving the game's fullscreen choice on windowed presentation
                    bResetPending = false;
                    SendDriverOutput();
                    ApplyResetWindowMode();
                    bBackBufferFloat = DriverWanted();
                }
                else
                    ApplyBackBufferFormat(pPresentParams);
                nAppliedState = -1;
            };

            FusionFix::onAfterEndScene() += []()
            {
                RenderOutput();
            };
        };
    }
} HDR;

export namespace HDROutput
{
    // The frame goes to the display in HDR: SDR tone mapping has to stay off
    bool IsActive()
    {
        return HDR::bOutputActive && HDR::bBackBufferFloat;
    }

    // The back buffer is 16-bit float, intermediate targets that are copied into it should be as well
    bool IsBackBufferFloat()
    {
        return HDR::bBackBufferFloat;
    }
}
