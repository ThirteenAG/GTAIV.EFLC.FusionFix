module;

#include <common.hxx>
#include <dxvk_interop.hpp>

export module hdr;

import common;
import comvars;
import settings;

#define IDR_HDR_OUTPUT_PS 3101

#ifndef SAFE_RELEASE
#define SAFE_RELEASE(p) { if (p) { (p)->Release(); (p)=NULL; } }
#endif

// HDR output through DXVK.
//
// With HDR enabled the device is created with a 16-bit float back buffer (DXVK has to unlock that format).
// The frame is composited exactly like in SDR, only nothing clips at 1.0 any more: the game's post
// processing writes gamma encoded color with highlights above 1.0 and the interface is drawn on top of it.
// The last pass of the frame converts the back buffer to scRGB, with paper white and a roll-off towards
// the peak brightness of the display, and the swap chain is switched to the extended sRGB color space.

// Brightness sliders of the HDR category of Display, in nits
namespace HDRBrightness
{
    constexpr float Step = 50.0f;
    constexpr float PeakMinimum = 50.0f;
    constexpr float PeakMaximum = 4000.0f;
    constexpr float PeakDefault = 1000.0f;
    constexpr float PaperWhiteMinimum = 50.0f;    // "Game/UI Brightness": brightness of the SDR white of the game and the interface
    constexpr float PaperWhiteMaximum = 500.0f;
    constexpr float PaperWhiteDefault = 200.0f;   // close to 203, the BT.2408 reference white
}

class HDR
{
public:
    static inline bool bFormatsUnlocked = false;
    static inline bool bBackBufferFloat = false;
    static inline bool bOutputActive = false;         // swap chain is in the extended sRGB color space
    static inline int32_t nAppliedState = -1;
    static inline float fPeak = HDRBrightness::PeakDefault;
    static inline float fPaperWhite = HDRBrightness::PaperWhiteDefault;
    static inline float fRollOffStart = 0.8f;
    static inline D3DPRESENT_PARAMETERS* pPresentParams = nullptr;
    static inline D3DFORMAT OriginalBackBufferFormat = D3DFMT_UNKNOWN;

    static inline rage::grcRenderTargetPC* CompositeRT = nullptr;
    static inline IDirect3DPixelShader9* OutputPS = nullptr;

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

    // DXVK with a display in HDR mode, the extended sRGB color space is also available through HDR10
    static bool IsSupported()
    {
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

    // The back buffer format only changes with a device reset: the game resets the device in the current mode
    static void RequestReset()
    {
        if (!pRequestedWidth || !pPresentParams || *pRequestedWidth != 0 || !pPresentParams->BackBufferWidth || !pPresentParams->BackBufferHeight)
            return;
        *pRequestedHeight = static_cast<int32_t>(pPresentParams->BackBufferHeight);
        *pRequestedRefreshRate = *pRefreshRate;
        *pRequestedWidth = static_cast<int32_t>(pPresentParams->BackBufferWidth);
    }

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

    // Saved in nits in the [HDR] section of the cfg, with the other menu settings
    static void LoadBrightness()
    {
        CIniReader cfg(CSettings::GetConfigPath());
        auto snap = [](int32_t value, float minimum, float maximum)
        {
            return std::clamp(std::round(value / HDRBrightness::Step) * HDRBrightness::Step, minimum, maximum);
        };
        fPeak = snap(cfg.ReadInteger("HDR", "PeakBrightnessNits", static_cast<int32_t>(HDRBrightness::PeakDefault)),
            HDRBrightness::PeakMinimum, HDRBrightness::PeakMaximum);
        fPaperWhite = snap(cfg.ReadInteger("HDR", "GameBrightnessNits", static_cast<int32_t>(HDRBrightness::PaperWhiteDefault)),
            HDRBrightness::PaperWhiteMinimum, HDRBrightness::PaperWhiteMaximum);
    }

    static void SetBrightness(float& target, float value, const char* key)
    {
        target = value;
        CIniReader cfg(CSettings::GetConfigPath());
        cfg.WriteInteger("HDR", key, static_cast<int32_t>(value), true);
        // The HDR metadata of the swap chain follows
        nAppliedState = -1;
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

        auto hr = RealCreateDevice(d3d, adapter, type, window, flags, pp, device);
        if (FAILED(hr) && pp && pp->BackBufferFormat == D3DFMT_A16B16G16R16F && OriginalBackBufferFormat != D3DFMT_UNKNOWN)
        {
            // HDR back buffer refused, keep the game running in SDR
            pp->BackBufferFormat = OriginalBackBufferFormat;
            hr = RealCreateDevice(d3d, adapter, type, window, flags, pp, device);
        }

        bBackBufferFloat = SUCCEEDED(hr) && pp && pp->BackBufferFormat == D3DFMT_A16B16G16R16F;
        nAppliedState = -1;
        return hr;
    }

    static void OnDirect3DCreated(IDirect3D9* d3d)
    {
        if (!d3d)
            return;

        ID3D9VkExtInterface* ext = nullptr;
        if (FAILED(d3d->QueryInterface(__uuidof(ID3D9VkExtInterface), reinterpret_cast<void**>(&ext))) || !ext)
            return;

        ext->UnlockAdditionalFormats();
        ext->Release();
        bFormatsUnlocked = true;

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

    static bool CreateResources(IDirect3DDevice9* device, const D3DSURFACE_DESC& desc)
    {
        if (!OutputPS)
        {
            HMODULE hm = nullptr;
            GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCWSTR)&CreateResources, &hm);
            if (auto hRes = FindResourceW(hm, MAKEINTRESOURCEW(IDR_HDR_OUTPUT_PS), RT_RCDATA))
                if (auto hGlob = LoadResource(hm, hRes))
                    if (auto buffer = LockResource(hGlob))
                        device->CreatePixelShader((DWORD*)buffer, &OutputPS);
        }

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

    static void RenderOutput()
    {
        auto device = rage::grcDevice::GetD3DDevice();
        if (!device)
            return;

        IDirect3DSurface9* backBuffer = nullptr;
        if (FAILED(device->GetRenderTarget(0, &backBuffer)) || !backBuffer)
            return;

        D3DSURFACE_DESC desc{};
        backBuffer->GetDesc(&desc);
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
        DWORD oldFVF = 0;
        D3DVIEWPORT9 oldViewport{};
        float oldConstant[4]{};
        device->GetPixelShader(&oldPS);
        device->GetVertexShader(&oldVS);
        device->GetVertexDeclaration(&oldDecl);
        device->GetFVF(&oldFVF);
        device->GetTexture(0, &oldTexture);
        device->GetViewport(&oldViewport);
        device->GetPixelShaderConstantF(0, oldConstant, 1);

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

        // Restore
        for (size_t i = 0; i < std::size(renderStates); ++i)
            device->SetRenderState(renderStates[i], oldRenderStates[i]);
        for (size_t i = 0; i < std::size(samplerStates); ++i)
            device->SetSamplerState(0, samplerStates[i], oldSamplerStates[i]);
        device->SetPixelShaderConstantF(0, oldConstant, 1);
        device->SetTexture(0, oldTexture);
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
        compositeSurface->Release();
        backBuffer->Release();
    }

public:
    HDR()
    {
        FusionFix::onInitEvent() += []()
        {
            CIniReader iniReader("");
            fRollOffStart = std::clamp(iniReader.ReadFloat("HDR", "RollOffStart", 0.8f), 0.0f, 1.0f);

            // An HDR category of Display. Turning HDR on or off resets the device with the other back buffer format,
            // the brightness values take effect immediately.
            FusionFixSettings.RegisterPreference(PreferenceName, 1, 0, "HDR", "HDR", OnOutputChanged);
            LoadBrightness();

            for (auto screen : { CSettings::MenuScreen::Display, CSettings::MenuScreen::TitleDisplay })
            {
                auto category = FusionFixSettings.AddCategory(screen, "HDR", "MO_DEF");
                if (category == CSettings::MenuScreen::Invalid)
                    continue;
                FusionFixSettings.AddToggle(category, "HDR", PreferenceName);
                FusionFixSettings.AddSlider(category, "Peak Brightness", HDRBrightness::PeakMinimum, HDRBrightness::PeakMaximum, HDRBrightness::Step,
                    [] { return fPeak; }, [](float value) { SetBrightness(fPeak, value, "PeakBrightnessNits"); });
                FusionFixSettings.AddSlider(category, "Game/UI Brightness", HDRBrightness::PaperWhiteMinimum, HDRBrightness::PaperWhiteMaximum,
                    HDRBrightness::Step, [] { return fPaperWhite; }, [](float value) { SetBrightness(fPaperWhite, value, "GameBrightnessNits"); });
            }

            // HDR can be turned on with the Vulkan graphics API while Windows HDR is on. The saved choice comes back
            // once it's available, checked when the menu opens.
            FusionFixSettings.SetAvailability(PreferenceName, [](int32_t value) -> bool
            {
                return value == 0 || IsSupported();
            });
            FusionFix::onMenuEnterEvent() += []()
            {
                FusionFixSettings.RefreshAvailability(PreferenceName);
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
