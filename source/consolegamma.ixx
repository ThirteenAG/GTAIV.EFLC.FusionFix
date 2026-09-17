module;

#include <common.hxx>
#include <d3dx9.h>

export module consolegamma;

import common;
import comvars;
import d3dx9_43;
import settings;

#define IDR_CONSOLEGAMMA 134

export class ConsoleGamma
{
public:
    static inline int nConsoleGammaMode = 0;

private:
    template <typename T>
    static void SafeRelease(T*& p)
    {
        if (p)
        {
            p->Release();

            p = nullptr;
        }
    }

    struct BackBufferInfo
    {
        D3DFORMAT format = D3DFMT_UNKNOWN;
        UINT width = 0;
        UINT height = 0;
    };

    static inline BackBufferInfo backBufferInfo{};

    static inline rage::grcRenderTargetPC* pSceneRT = nullptr;
    static inline IDirect3DTexture9* pSceneTex = nullptr;
    static inline IDirect3DSurface9* pSceneSurf = nullptr;
    static inline ID3DXEffect* pEffect = nullptr;

    static inline D3DXHANDLE hInputTex2D = nullptr;
    static inline D3DXHANDLE hGammaTechnique = nullptr;
    static inline D3DXHANDLE hTechniqueBlitXenonGamma = nullptr;
    static inline D3DXHANDLE hTechniqueBlitCellGamma = nullptr;

    static IDirect3DSurface9* GetRealBackBuffer(IDirect3DDevice9* device)
    {
        IDirect3DSurface9* backBuffer = nullptr;
        if (!device)
            return backBuffer;

        IDirect3DSwapChain9* swapChain = nullptr;
        if (SUCCEEDED(device->GetSwapChain(0, &swapChain)) && swapChain)
        {
            swapChain->GetBackBuffer(0, D3DBACKBUFFER_TYPE_MONO, &backBuffer);

            swapChain->Release();
        }

        return backBuffer;
    }

    static bool LoadEffectFile(IDirect3DDevice9* device)
    {
        if (pEffect)
            return true;

        HMODULE hModule = nullptr;
        GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCWSTR)&LoadEffectFile, &hModule);

        HRSRC hRes = FindResourceW(hModule, MAKEINTRESOURCEW(IDR_CONSOLEGAMMA), RT_RCDATA);
        if (!hRes)
        {
            OutputDebugStringA("ConsoleGamma: Failed to find effect file\n");

            return false;
        }

        HGLOBAL hGlob = LoadResource(hModule, hRes);
        if (!hGlob)
        {
            OutputDebugStringA("ConsoleGamma: Failed to load effect file\n");

            return false;
        }

        const void* pData = LockResource(hGlob);
        DWORD dataSize = SizeofResource(hModule, hRes);
        if (!pData || !dataSize)
        {
            OutputDebugStringA("ConsoleGamma: Effect file is empty\n");

            return false;
        }

        ID3DXBuffer* errors = nullptr;
        HRESULT hResult = D3DXCreateEffect(device, pData, dataSize, nullptr, nullptr, 0, nullptr, &pEffect, &errors);
        if (FAILED(hResult) || !pEffect)
        {
            if (errors)
            {
                OutputDebugStringA((const char*)errors->GetBufferPointer());

                errors->Release();
            }

            OutputDebugStringA("ConsoleGamma: Failed to load effect\n");

            return false;
        }

        hInputTex2D = pEffect->GetParameterByName(nullptr, "InputTex2D");
        hTechniqueBlitXenonGamma = pEffect->GetTechniqueByName("BlitXenonGamma");
        hTechniqueBlitCellGamma = pEffect->GetTechniqueByName("BlitCellGamma");
        if (!hInputTex2D || !hTechniqueBlitXenonGamma || !hTechniqueBlitCellGamma)
        {
            SafeRelease(pEffect);

            return false;
        }

        pEffect->ValidateTechnique(hTechniqueBlitXenonGamma);
        pEffect->ValidateTechnique(hTechniqueBlitCellGamma);

        return true;
    }

    static bool CreateSceneTexture(IDirect3DDevice9* device)
    {
        if (pSceneRT)
            return true;

        IDirect3DSurface9* backBuffer = GetRealBackBuffer(device);
        if (!backBuffer)
            return false;

        D3DSURFACE_DESC backBufferDesc{};
        if (FAILED(backBuffer->GetDesc(&backBufferDesc)))
        {
            backBuffer->Release();

            return false;
        }

        backBufferInfo.width = backBufferDesc.Width;
        backBufferInfo.height = backBufferDesc.Height;
        backBufferInfo.format = backBufferDesc.Format;

        backBuffer->Release();

        rage::grcRenderTargetDesc renderTargetDesc{};
        renderTargetDesc.mMultisampleCount = 0;
        renderTargetDesc.field_0 = 1;
        renderTargetDesc.field_12 = 1;
        renderTargetDesc.mDepthRT = nullptr;
        renderTargetDesc.field_8 = 1;
        renderTargetDesc.field_10 = 1;
        renderTargetDesc.field_11 = 1;
        renderTargetDesc.field_24 = false;
        renderTargetDesc.mFormat = rage::getEngineTextureFormat(backBufferInfo.format);

        auto* renderTarget = rage::grcTextureFactory::GetInstance()->CreateRenderTarget("ConsoleGammaScene", 3, backBufferInfo.width, backBufferInfo.height, 32, &renderTargetDesc);

        if (!renderTarget)
            return false;

        rage::grcDevice::grcResolveFlags resolveFlags{};
        rage::grcTextureFactoryPC::GetInstance()->LockRenderTarget(0, renderTarget, nullptr);
        rage::grcTextureFactoryPC::GetInstance()->UnlockRenderTarget(0, &resolveFlags);

        pSceneRT = renderTarget;

        if (!pSceneRT->mD3DTexture)
        {
            pSceneRT->Destroy();

            pSceneRT = nullptr;

            return false;
        }

        pSceneTex = pSceneRT->mD3DTexture;

        if (FAILED(pSceneTex->GetSurfaceLevel(0, &pSceneSurf)))
        {
            pSceneRT->Destroy();

            pSceneRT = nullptr;
            pSceneTex = nullptr;

            return false;
        }

        return true;
    }

    static void ReleaseSceneTexture()
    {
        SafeRelease(pSceneSurf);

        pSceneTex = nullptr;

        if (pSceneRT)
        {
            pSceneRT->Destroy();

            pSceneRT = nullptr;
        }
    }

    static void DrawScreenQuad(IDirect3DDevice9* device)
    {
        struct ScreenVertex { float x, y, z, rhw, u, v; };
        ScreenVertex vertices[4] =
        {
            { -0.5f,                              -0.5f,                               0.0f, 1.0f, 0.0f, 0.0f },
            { -0.5f,                              float(backBufferInfo.height) - 0.5f, 0.0f, 1.0f, 0.0f, 1.0f },
            { float(backBufferInfo.width) - 0.5f, -0.5f,                               0.0f, 1.0f, 1.0f, 0.0f },
            { float(backBufferInfo.width) - 0.5f, float(backBufferInfo.height) - 0.5f, 0.0f, 1.0f, 1.0f, 1.0f }
        };

        device->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, vertices, sizeof(ScreenVertex));
    }

    static void Render(IDirect3DDevice9* device)
    {
        if (nConsoleGammaMode == 0 || !device)
            return;

        if (!LoadEffectFile(device) || !CreateSceneTexture(device))
            return;

        hGammaTechnique = (nConsoleGammaMode == 1) ? hTechniqueBlitXenonGamma : hTechniqueBlitCellGamma;
        if (!hGammaTechnique)
            return;

        IDirect3DSurface9* currentRenderTarget = nullptr;
        if (FAILED(device->GetRenderTarget(0, &currentRenderTarget)) || !currentRenderTarget)
            return;

        if (FAILED(device->StretchRect(currentRenderTarget, nullptr, pSceneSurf, nullptr, D3DTEXF_POINT)))
        {
            currentRenderTarget->Release();

            return;
        }

        IDirect3DVertexBuffer9* oldVertexBuffer = nullptr;
        IDirect3DVertexDeclaration9* oldVertexDecl = nullptr;

        UINT oldOffset = 0, oldStride = 0;
        DWORD oldFVF = 0;

        device->GetStreamSource(0, &oldVertexBuffer, &oldOffset, &oldStride);
        device->GetVertexDeclaration(&oldVertexDecl);
        device->GetFVF(&oldFVF);

        device->SetStreamSource(0, nullptr, 0, 0);
        device->SetVertexDeclaration(nullptr);
        device->SetFVF(D3DFVF_XYZRHW | D3DFVF_TEX1);

        pEffect->SetTexture(hInputTex2D, pSceneTex);
        pEffect->SetTechnique(hGammaTechnique);
        pEffect->CommitChanges();

        UINT passes = 0;
        if (SUCCEEDED(pEffect->Begin(&passes, 0)))
        {
            if (SUCCEEDED(pEffect->BeginPass(0)))
            {
                DrawScreenQuad(device);

                pEffect->EndPass();
            }

            pEffect->End();
        }

        device->SetStreamSource(0, oldVertexBuffer, oldOffset, oldStride);
        device->SetVertexDeclaration(oldVertexDecl);
        device->SetFVF(oldFVF);

        SafeRelease(oldVertexBuffer);
        SafeRelease(oldVertexDecl);

        currentRenderTarget->Release();
    }

    static void Shutdown()
    {
        SafeRelease(pEffect);
        ReleaseSceneTexture();

        hInputTex2D = nullptr;
        hGammaTechnique = nullptr;
        hTechniqueBlitXenonGamma = nullptr;
        hTechniqueBlitCellGamma = nullptr;
    }

public:
    ConsoleGamma()
    {
        FusionFix::onInitEventAsync() += []()
        {
            auto ConsoleGamma = FusionFixSettings.GetRef("PREF_CONSOLE_GAMMA");
            nConsoleGammaMode = ConsoleGamma->get();

            FusionFixSettings.SetCallback("PREF_CONSOLE_GAMMA", [](int32_t value)
            {
                nConsoleGammaMode = value;
            });

            FusionFix::onEndScene() += []()
            {
                ConsoleGamma::Render(rage::grcDevice::GetD3DDevice());
            };

            FusionFix::onBeforeReset() += []()
            {
                ConsoleGamma::Shutdown();
            };
        };
    }
} ConsoleGamma;