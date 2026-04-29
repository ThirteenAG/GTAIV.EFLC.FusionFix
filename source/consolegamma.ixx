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

    static inline IDirect3DVertexBuffer9* mQuadVertexBuffer;
    static inline IDirect3DVertexDeclaration9* mQuadVertexDecl;

    // DX9 textures/surfaces
    static inline IDirect3DTexture9* pSceneTex = nullptr;
    static inline IDirect3DSurface9* pSceneSurf = nullptr;

    // Shaders
    static inline IDirect3DVertexShader9* VS_BlitGamma = nullptr;
    static inline IDirect3DPixelShader9* PS_BlitGamma = nullptr;

    static void __fastcall OnDeviceLost()
    {
        SAFE_RELEASE(pSceneTex);
        SAFE_RELEASE(pSceneSurf);

        SAFE_RELEASE(mQuadVertexBuffer);
        SAFE_RELEASE(mQuadVertexDecl);
    }

    static void __fastcall OnDeviceReset()
    {
        auto pDevice = rage::grcDevice::GetD3DDevice();
        if (!pDevice)
            return;

        IDirect3DSurface9* pBackBuffer = GetRealBackBuffer(pDevice);
        if (!pBackBuffer)
            return;

        D3DSURFACE_DESC desc{};
        pBackBuffer->GetDesc(&desc);
        auto nScreenWidth = desc.Width;
        auto nScreenHeight = desc.Height;
        SAFE_RELEASE(pBackBuffer);

        if (FAILED(pDevice->CreateTexture(nScreenWidth, nScreenHeight, 1, D3DUSAGE_RENDERTARGET, desc.Format, D3DPOOL_DEFAULT, &pSceneTex, nullptr)))
            return;

        pSceneTex->GetSurfaceLevel(0, &pSceneSurf);

        // Create vertex declaration
        if (!mQuadVertexDecl)
        {
            D3DVERTEXELEMENT9 VertexDeclElements[] =
            {
                {0, 0,  D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0},
                {0, 12, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0},
                D3DDECL_END()
            };

            if (FAILED(pDevice->CreateVertexDeclaration(VertexDeclElements, &mQuadVertexDecl)))
            {
                mQuadVertexDecl = nullptr;
                return;
            }
        }

        // Create and fill the fullscreen-quad VB
        if (!mQuadVertexBuffer)
        {
            if (FAILED(pDevice->CreateVertexBuffer(6 * sizeof(VertexFormat), 0, 0, D3DPOOL_DEFAULT, &mQuadVertexBuffer, nullptr)))
            {
                mQuadVertexBuffer = nullptr;
                return;
            }

            VertexFormat* VertexData = nullptr;
            if (FAILED(mQuadVertexBuffer->Lock(0, 0, reinterpret_cast<void**>(&VertexData), 0)))
            {
                mQuadVertexBuffer->Release();
                mQuadVertexBuffer = nullptr;
                return;
            }

            VertexData[0] = { -1.0f, 1.0f, 0.0f, 0.0f + (0.5f / (float)nScreenWidth), 0.0f + (0.5f / (float)nScreenHeight) };
            VertexData[1] = { -1.0f, -1.0f, 0.0f, 0.0f + (0.5f / (float)nScreenWidth), 1.0f + (0.5f / (float)nScreenHeight) };
            VertexData[2] = { 1.0f, 1.0f, 0.0f, 1.0f + (0.5f / (float)nScreenWidth), 0.0f + (0.5f / (float)nScreenHeight) };
            VertexData[3] = { -1.0f, -1.0f, 0.0f, 0.0f + (0.5f / (float)nScreenWidth), 1.0f + (0.5f / (float)nScreenHeight) };
            VertexData[4] = { 1.0f, -1.0f, 0.0f, 1.0f + (0.5f / (float)nScreenWidth), 1.0f + (0.5f / (float)nScreenHeight) };
            VertexData[5] = { 1.0f, 1.0f, 0.0f, 1.0f + (0.5f / (float)nScreenWidth), 0.0f + (0.5f / (float)nScreenHeight) };

            mQuadVertexBuffer->Unlock();
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
        if (!cg->get())
            return;

        auto pDevice = rage::grcDevice::GetD3DDevice();
        if (!pDevice || !pSceneTex || !pSceneSurf || !VS_BlitGamma || !PS_BlitGamma || !mQuadVertexDecl || !mQuadVertexBuffer)
            return;

        IDirect3DSurface9* pRealBB = GetRealBackBuffer(pDevice);
        if (!pRealBB)
            return;

        IDirect3DSurface9* pCurrentRT = nullptr;
        pDevice->GetRenderTarget(0, &pCurrentRT);
        if (pCurrentRT)
        {
            pDevice->StretchRect(pCurrentRT, nullptr, pSceneSurf, nullptr, D3DTEXF_POINT);
            SAFE_RELEASE(pCurrentRT);
        }

        DWORD prevAddressV = 0;
        DWORD prevAddressU = 0;
        DWORD prevAddressW = 0;
        DWORD prevMinFilter = 0;
        DWORD prevMagFilter = 0;
        DWORD prevMipFilter = 0;

        DWORD prevCullMode = 0;
        DWORD prevZEnable = 0;
        DWORD prevZWriteEnable = 0;
        DWORD prevZFunc = 0;
        DWORD prevAlphaBlendEnable = 0;
        DWORD prevAlphaTestEnable = 0;
        DWORD prevScissorTestEnable = 0;
        DWORD prevLighting = 0;
        DWORD prevFogEnable = 0;

        // Previous surfaces, depth/stencil, textures, shaders
        IDirect3DSurface9* prevSurface = nullptr;
        IDirect3DSurface9* prevDepthStencilSurface = nullptr;

        IDirect3DVertexDeclaration9* prevVertexDecl = nullptr;
        IDirect3DVertexBuffer9* prevVertexBuffer = nullptr;
        UINT prevOffset = 0;
        UINT prevStride = 0;

        IDirect3DBaseTexture9* prevTex = nullptr;

        IDirect3DVertexShader9* prevVS = nullptr;
        IDirect3DPixelShader9* prevPS = nullptr;

        // Store previous sampler states, renderstates, surfaces, textures, shaders
        pDevice->GetRenderState(D3DRS_CULLMODE, &prevCullMode);
        pDevice->GetRenderState(D3DRS_ZENABLE, &prevZEnable);
        pDevice->GetRenderState(D3DRS_ZWRITEENABLE, &prevZWriteEnable);
        pDevice->GetRenderState(D3DRS_ZFUNC, &prevZFunc);
        pDevice->GetRenderState(D3DRS_ALPHABLENDENABLE, &prevAlphaBlendEnable);
        pDevice->GetRenderState(D3DRS_ALPHATESTENABLE, &prevAlphaTestEnable);
        pDevice->GetRenderState(D3DRS_SCISSORTESTENABLE, &prevScissorTestEnable);
        pDevice->GetRenderState(D3DRS_LIGHTING, &prevLighting);
        pDevice->GetRenderState(D3DRS_FOGENABLE, &prevFogEnable);

        pDevice->GetSamplerState(0, D3DSAMP_ADDRESSU, &prevAddressU);
        pDevice->GetSamplerState(0, D3DSAMP_ADDRESSV, &prevAddressV);
        pDevice->GetSamplerState(0, D3DSAMP_ADDRESSW, &prevAddressW);
        pDevice->GetSamplerState(0, D3DSAMP_MINFILTER, &prevMinFilter);
        pDevice->GetSamplerState(0, D3DSAMP_MAGFILTER, &prevMagFilter);
        pDevice->GetSamplerState(0, D3DSAMP_MIPFILTER, &prevMipFilter);

        pDevice->GetRenderTarget(0, &prevSurface);
        pDevice->GetDepthStencilSurface(&prevDepthStencilSurface);

        pDevice->GetVertexDeclaration(&prevVertexDecl);
        pDevice->GetStreamSource(0, &prevVertexBuffer, &prevOffset, &prevStride);

        pDevice->GetTexture(0, &prevTex);

        pDevice->GetVertexShader(&prevVS);
        pDevice->GetPixelShader(&prevPS);

        {
            pDevice->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
            pDevice->SetRenderState(D3DRS_ZENABLE, D3DZB_FALSE);
            pDevice->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
            pDevice->SetRenderState(D3DRS_ZFUNC, D3DCMP_ALWAYS);
            pDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
            pDevice->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
            pDevice->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
            pDevice->SetRenderState(D3DRS_LIGHTING, FALSE);
            pDevice->SetRenderState(D3DRS_FOGENABLE, FALSE);

            pDevice->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
            pDevice->SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
            pDevice->SetSamplerState(0, D3DSAMP_ADDRESSW, D3DTADDRESS_WRAP);
            pDevice->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
            pDevice->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
            pDevice->SetSamplerState(0, D3DSAMP_MIPFILTER, D3DTEXF_LINEAR);

            pDevice->SetRenderTarget(0, nullptr);
            pDevice->SetDepthStencilSurface(nullptr);

            pDevice->SetRenderTarget(0, pRealBB);

            pDevice->SetVertexDeclaration(nullptr);
            pDevice->SetStreamSource(0, nullptr, 0, 0);

            pDevice->SetVertexDeclaration(mQuadVertexDecl);
            pDevice->SetStreamSource(0, mQuadVertexBuffer, 0, sizeof(VertexFormat));

            pDevice->SetTexture(0, nullptr);

            pDevice->SetTexture(0, pSceneTex);

            pDevice->SetVertexShader(VS_BlitGamma);
            pDevice->SetPixelShader(PS_BlitGamma);

            pDevice->DrawPrimitive(D3DPT_TRIANGLELIST, 0, 2);

            SAFE_RELEASE(pRealBB);
        }

        pDevice->SetRenderState(D3DRS_CULLMODE, prevCullMode);
        pDevice->SetRenderState(D3DRS_ZENABLE, prevZEnable);
        pDevice->SetRenderState(D3DRS_ZWRITEENABLE, prevZWriteEnable);
        pDevice->SetRenderState(D3DRS_ZFUNC, prevZFunc);
        pDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, prevAlphaBlendEnable);
        pDevice->SetRenderState(D3DRS_ALPHATESTENABLE, prevAlphaTestEnable);
        pDevice->SetRenderState(D3DRS_SCISSORTESTENABLE, prevScissorTestEnable);
        pDevice->SetRenderState(D3DRS_LIGHTING, prevLighting);
        pDevice->SetRenderState(D3DRS_FOGENABLE, prevFogEnable);

        pDevice->SetSamplerState(0, D3DSAMP_ADDRESSU, prevAddressU);
        pDevice->SetSamplerState(0, D3DSAMP_ADDRESSV, prevAddressV);
        pDevice->SetSamplerState(0, D3DSAMP_ADDRESSW, prevAddressW);
        pDevice->SetSamplerState(0, D3DSAMP_MINFILTER, prevMinFilter);
        pDevice->SetSamplerState(0, D3DSAMP_MAGFILTER, prevMagFilter);
        pDevice->SetSamplerState(0, D3DSAMP_MIPFILTER, prevMipFilter);

        pDevice->SetRenderTarget(0, prevSurface);
        pDevice->SetDepthStencilSurface(prevDepthStencilSurface);

        pDevice->SetVertexDeclaration(prevVertexDecl);
        pDevice->SetStreamSource(0, prevVertexBuffer, prevOffset, prevStride);

        pDevice->SetTexture(0, prevTex);

        pDevice->SetVertexShader(prevVS);
        pDevice->SetPixelShader(prevPS);

        // Release
        SAFE_RELEASE(prevSurface);
        SAFE_RELEASE(prevDepthStencilSurface);

        SAFE_RELEASE(prevVertexDecl);
        SAFE_RELEASE(prevVertexBuffer);

        SAFE_RELEASE(prevTex);

        SAFE_RELEASE(prevVS);
        SAFE_RELEASE(prevPS);
    }

    static void Initalize()
    {
        static bool bInitialized = false;
        if (bInitialized)
            return;

        auto pDevice = rage::grcDevice::GetD3DDevice();
        if (!pDevice)
            return;

        auto OnLostCB = rage::grcDevice::Functor0(NULL, OnDeviceLost, NULL, 0);
        auto OnResetCB = rage::grcDevice::Functor0(NULL, OnDeviceReset, NULL, 0);
        rage::grcDevice::RegisterDeviceCallbacks(OnLostCB, OnResetCB);

        HMODULE hModule = NULL;
        GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCWSTR)&Initalize, &hModule);

        ID3DXBuffer* ppShader = nullptr;
        ID3DXBuffer* ppErrorMsgs = nullptr;

        //Shader ASM
        if (!VS_BlitGamma)
        {
            if (SUCCEEDED(D3DXAssembleShaderFromResourceW(hModule, MAKEINTRESOURCEW(IDR_VS_BlitGamma), NULL, NULL, 0, &ppShader, &ppErrorMsgs)))
            {
                pDevice->CreateVertexShader((DWORD*)ppShader->GetBufferPointer(), &VS_BlitGamma);
                SAFE_RELEASE(ppShader);
                SAFE_RELEASE(ppErrorMsgs);
            }
        }

        if (!PS_BlitGamma)
        {
            if (SUCCEEDED(D3DXAssembleShaderFromResourceW(hModule, MAKEINTRESOURCEW(IDR_PS_BlitGamma), NULL, NULL, 0, &ppShader, &ppErrorMsgs)))
            {
                pDevice->CreatePixelShader((DWORD*)ppShader->GetBufferPointer(), &PS_BlitGamma);
                SAFE_RELEASE(ppShader);
                SAFE_RELEASE(ppErrorMsgs);
            }
        }

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
                    Initalize();
                    RenderConsoleGamma();
                };
            }
        };
    }
} ConsoleGamma;