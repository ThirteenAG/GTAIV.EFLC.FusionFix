module;

#include <common.hxx>
#include <d3dx9tex.h>

export module consolegamma;

import common;
import comvars;
import d3dx9_43;
import settings;

#ifndef SAFE_RELEASE
#define SAFE_RELEASE(p) { if (p) { (p)->Release(); (p)=NULL; } }
#endif

#define IDR_VS_BlitGamma 134
#define IDR_PS_BlitGamma 135

class ConsoleGamma
{
private:
    struct VertexFormat
    {
        float Pos[3];
        float TexCoord[2];
    };

    static inline IDirect3DVertexBuffer9* mQuadVertexBuffer = nullptr;
    static inline IDirect3DVertexDeclaration9* mQuadVertexDecl = nullptr;

    // grc render target
    static inline rage::grcRenderTargetPC* pSceneRT = nullptr;
    static inline IDirect3DTexture9* pSceneTex = nullptr;
    static inline IDirect3DSurface9* pSceneSurf = nullptr;

    // Shaders
    static inline IDirect3DVertexShader9* VS_BlitGamma = nullptr;
    static inline IDirect3DPixelShader9* PS_BlitGamma = nullptr;

    static void __fastcall OnDeviceLost()
    {
        SAFE_RELEASE(pSceneSurf);
        pSceneTex = nullptr;

        if (pSceneRT)
        {
            pSceneRT->Destroy();
            pSceneRT = nullptr;
        }

        SAFE_RELEASE(mQuadVertexBuffer);
        SAFE_RELEASE(mQuadVertexDecl);

        SAFE_RELEASE(VS_BlitGamma);
        SAFE_RELEASE(PS_BlitGamma);
    }

    static void __fastcall OnDeviceReset()
    {
        auto pDevice = rage::grcDevice::GetD3DDevice();
        if (!pDevice) return;

        // Get backbuffer resolution
        IDirect3DSurface9* pBackBuffer = GetRealBackBuffer(pDevice);
        if (!pBackBuffer) return;

        D3DSURFACE_DESC desc{};
        pBackBuffer->GetDesc(&desc);
        UINT Width = desc.Width;
        UINT Height = desc.Height;
        SAFE_RELEASE(pBackBuffer);

        // ==================== CreateEmptyRT Workaround ====================
        if (!pSceneRT)
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
            rtDesc.mFormat = rage::GRCFMT_A8R8G8B8;

            auto CreateEmptyRT = [&](const char* name, uint32_t w, uint32_t h) -> rage::grcRenderTargetPC*
            {
                auto rt = rage::grcTextureFactory::GetInstance()->CreateRenderTarget(name, 3, w, h, 32, &rtDesc);

                rage::grcDevice::grcResolveFlags resolveFlags{};
                rage::grcTextureFactoryPC::GetInstance()->LockRenderTarget(0, rt, nullptr);
                rage::grcTextureFactoryPC::GetInstance()->UnlockRenderTarget(0, &resolveFlags);

                return rt;
            };

            pSceneRT = CreateEmptyRT("ConsoleGammaScene", Width, Height);
        }

        if (pSceneRT)
        {
            pSceneTex = pSceneRT->mD3DTexture;
            pSceneRT->mD3DTexture->GetSurfaceLevel(0, &pSceneSurf);
        }

        // Vertex Declaration
        if (!mQuadVertexDecl)
        {
            D3DVERTEXELEMENT9 VertexDeclElements[] =
            {
                {0, 0,  D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0},
                {0, 12, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0},
                D3DDECL_END()
            };

            pDevice->CreateVertexDeclaration(VertexDeclElements, &mQuadVertexDecl);
        }

        // Fullscreen Quad
        if (!mQuadVertexBuffer)
        {
            if (FAILED(pDevice->CreateVertexBuffer(6 * sizeof(VertexFormat), 0, 0, D3DPOOL_DEFAULT, &mQuadVertexBuffer, nullptr)))
                return;

            VertexFormat* VertexData = nullptr;
            if (SUCCEEDED(mQuadVertexBuffer->Lock(0, 0, reinterpret_cast<void**>(&VertexData), 0)))
            {
                float hx = 0.5f / (float)Width;
                float hy = 0.5f / (float)Height;

                VertexData[0] = { -1.0f,  1.0f, 0.0f, 0.0f + hx, 0.0f + hy };
                VertexData[1] = { -1.0f, -1.0f, 0.0f, 0.0f + hx, 1.0f + hy };
                VertexData[2] = { 1.0f,  1.0f, 0.0f, 1.0f + hx, 0.0f + hy };
                VertexData[3] = { -1.0f, -1.0f, 0.0f, 0.0f + hx, 1.0f + hy };
                VertexData[4] = { 1.0f, -1.0f, 0.0f, 1.0f + hx, 1.0f + hy };
                VertexData[5] = { 1.0f,  1.0f, 0.0f, 1.0f + hx, 0.0f + hy };

                mQuadVertexBuffer->Unlock();
            }
        }

        // Load Shaders
        HMODULE hModule = NULL;
        GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           (LPCWSTR)&Initialize, &hModule);

        ID3DXBuffer* ppShader = nullptr;
        ID3DXBuffer* ppErrorMsgs = nullptr;

        if (!VS_BlitGamma)
        {
            if (SUCCEEDED(D3DXAssembleShaderFromResourceW(hModule, MAKEINTRESOURCEW(IDR_VS_BlitGamma), NULL, NULL, 0, &ppShader, &ppErrorMsgs)))
            {
                pDevice->CreateVertexShader((DWORD*)ppShader->GetBufferPointer(), &VS_BlitGamma);
            }
            SAFE_RELEASE(ppShader);
            SAFE_RELEASE(ppErrorMsgs);
        }

        if (!PS_BlitGamma)
        {
            if (SUCCEEDED(D3DXAssembleShaderFromResourceW(hModule, MAKEINTRESOURCEW(IDR_PS_BlitGamma), NULL, NULL, 0, &ppShader, &ppErrorMsgs)))
            {
                pDevice->CreatePixelShader((DWORD*)ppShader->GetBufferPointer(), &PS_BlitGamma);
            }
            SAFE_RELEASE(ppShader);
            SAFE_RELEASE(ppErrorMsgs);
        }
    }

    static IDirect3DSurface9* GetRealBackBuffer(IDirect3DDevice9* pDevice)
    {
        IDirect3DSwapChain9* pSwap = nullptr;
        IDirect3DSurface9* pBB = nullptr;
        if (SUCCEEDED(pDevice->GetSwapChain(0, &pSwap)) && pSwap)
        {
            pSwap->GetBackBuffer(0, D3DBACKBUFFER_TYPE_MONO, &pBB);
            pSwap->Release();
        }
        return pBB;
    }

    static void RenderConsoleGamma()
    {
        static auto cg = FusionFixSettings.GetRef("PREF_CONSOLE_GAMMA");
        if (!cg->get()) return;

        auto pDevice = rage::grcDevice::GetD3DDevice();
        if (!pDevice || !pSceneRT || !pSceneSurf || !VS_BlitGamma || !PS_BlitGamma || !mQuadVertexDecl || !mQuadVertexBuffer)
            return;

        IDirect3DSurface9* pRealBB = GetRealBackBuffer(pDevice);
        if (!pRealBB) return;

        // Capture current frame
        IDirect3DSurface9* pCurrentRT = nullptr;
        pDevice->GetRenderTarget(0, &pCurrentRT);
        if (pCurrentRT)
        {
            pDevice->StretchRect(pCurrentRT, nullptr, pSceneSurf, nullptr, D3DTEXF_POINT);
            SAFE_RELEASE(pCurrentRT);
        }

        // Save previous state
        DWORD prevCullMode, prevZEnable, prevZWrite, prevZFunc, prevAlphaBlend, prevAlphaTest, prevScissor, prevLighting, prevFog;
        pDevice->GetRenderState(D3DRS_CULLMODE, &prevCullMode);
        pDevice->GetRenderState(D3DRS_ZENABLE, &prevZEnable);
        pDevice->GetRenderState(D3DRS_ZWRITEENABLE, &prevZWrite);
        pDevice->GetRenderState(D3DRS_ZFUNC, &prevZFunc);
        pDevice->GetRenderState(D3DRS_ALPHABLENDENABLE, &prevAlphaBlend);
        pDevice->GetRenderState(D3DRS_ALPHATESTENABLE, &prevAlphaTest);
        pDevice->GetRenderState(D3DRS_SCISSORTESTENABLE, &prevScissor);
        pDevice->GetRenderState(D3DRS_LIGHTING, &prevLighting);
        pDevice->GetRenderState(D3DRS_FOGENABLE, &prevFog);

        DWORD prevAddressU, prevAddressV, prevAddressW, prevMin, prevMag, prevMip;
        pDevice->GetSamplerState(0, D3DSAMP_ADDRESSU, &prevAddressU);
        pDevice->GetSamplerState(0, D3DSAMP_ADDRESSV, &prevAddressV);
        pDevice->GetSamplerState(0, D3DSAMP_ADDRESSW, &prevAddressW);
        pDevice->GetSamplerState(0, D3DSAMP_MINFILTER, &prevMin);
        pDevice->GetSamplerState(0, D3DSAMP_MAGFILTER, &prevMag);
        pDevice->GetSamplerState(0, D3DSAMP_MIPFILTER, &prevMip);

        IDirect3DSurface9* prevSurface = nullptr;
        IDirect3DSurface9* prevDS = nullptr;
        pDevice->GetRenderTarget(0, &prevSurface);
        pDevice->GetDepthStencilSurface(&prevDS);

        IDirect3DVertexDeclaration9* prevDecl = nullptr;
        IDirect3DVertexBuffer9* prevVB = nullptr;
        UINT prevOffset = 0, prevStride = 0;
        pDevice->GetVertexDeclaration(&prevDecl);
        pDevice->GetStreamSource(0, &prevVB, &prevOffset, &prevStride);

        IDirect3DBaseTexture9* prevTex = nullptr;
        pDevice->GetTexture(0, &prevTex);

        IDirect3DVertexShader9* prevVS = nullptr;
        IDirect3DPixelShader9* prevPS = nullptr;
        pDevice->GetVertexShader(&prevVS);
        pDevice->GetPixelShader(&prevPS);

        // Draw gamma corrected image
        {
            pDevice->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
            pDevice->SetRenderState(D3DRS_ZENABLE, D3DZB_FALSE);
            pDevice->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
            pDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
            pDevice->SetRenderState(D3DRS_LIGHTING, FALSE);
            pDevice->SetRenderState(D3DRS_FOGENABLE, FALSE);

            pDevice->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
            pDevice->SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
            pDevice->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
            pDevice->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);

            pDevice->SetRenderTarget(0, pRealBB);
            pDevice->SetDepthStencilSurface(nullptr);

            pDevice->SetVertexDeclaration(mQuadVertexDecl);
            pDevice->SetStreamSource(0, mQuadVertexBuffer, 0, sizeof(VertexFormat));
            pDevice->SetTexture(0, pSceneTex);

            pDevice->SetVertexShader(VS_BlitGamma);
            pDevice->SetPixelShader(PS_BlitGamma);

            pDevice->DrawPrimitive(D3DPT_TRIANGLELIST, 0, 2);
        }

        // Restore previous state
        pDevice->SetRenderState(D3DRS_CULLMODE, prevCullMode);
        pDevice->SetRenderState(D3DRS_ZENABLE, prevZEnable);
        pDevice->SetRenderState(D3DRS_ZWRITEENABLE, prevZWrite);
        pDevice->SetRenderState(D3DRS_ZFUNC, prevZFunc);
        pDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, prevAlphaBlend);
        pDevice->SetRenderState(D3DRS_ALPHATESTENABLE, prevAlphaTest);
        pDevice->SetRenderState(D3DRS_SCISSORTESTENABLE, prevScissor);
        pDevice->SetRenderState(D3DRS_LIGHTING, prevLighting);
        pDevice->SetRenderState(D3DRS_FOGENABLE, prevFog);

        pDevice->SetSamplerState(0, D3DSAMP_ADDRESSU, prevAddressU);
        pDevice->SetSamplerState(0, D3DSAMP_ADDRESSV, prevAddressV);
        pDevice->SetSamplerState(0, D3DSAMP_ADDRESSW, prevAddressW);
        pDevice->SetSamplerState(0, D3DSAMP_MINFILTER, prevMin);
        pDevice->SetSamplerState(0, D3DSAMP_MAGFILTER, prevMag);
        pDevice->SetSamplerState(0, D3DSAMP_MIPFILTER, prevMip);

        pDevice->SetRenderTarget(0, prevSurface);
        pDevice->SetDepthStencilSurface(prevDS);
        pDevice->SetVertexDeclaration(prevDecl);
        pDevice->SetStreamSource(0, prevVB, prevOffset, prevStride);
        pDevice->SetTexture(0, prevTex);
        pDevice->SetVertexShader(prevVS);
        pDevice->SetPixelShader(prevPS);

        // Cleanup
        SAFE_RELEASE(pRealBB);
        SAFE_RELEASE(prevSurface);
        SAFE_RELEASE(prevDS);
        SAFE_RELEASE(prevDecl);
        SAFE_RELEASE(prevVB);
        SAFE_RELEASE(prevTex);
        SAFE_RELEASE(prevVS);
        SAFE_RELEASE(prevPS);
    }

    static void Initialize()
    {
        static bool bInitialized = false;
        if (bInitialized) return;

        auto OnLostCB = rage::grcDevice::Functor0(NULL, OnDeviceLost, NULL, 0);
        auto OnResetCB = rage::grcDevice::Functor0(NULL, OnDeviceReset, NULL, 0);
        rage::grcDevice::RegisterDeviceCallbacks(OnLostCB, OnResetCB);

        OnDeviceReset();

        bInitialized = true;
    }

public:
    ConsoleGamma()
    {
        FusionFix::onInitEventAsync() += []()
        {
            if (GetD3DX9_43DLL())
            {
                FusionFix::onEndScene() += []()
                {
                    Initialize();
                    RenderConsoleGamma();
                };
            }
        };
    }
} ConsoleGamma;