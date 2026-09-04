module;

#include <common.hxx>

export module consolegamma;

import common;
import comvars;
import d3dx9_43;
import settings;

#ifndef SAFE_RELEASE
#define SAFE_RELEASE(p) { if (p) { (p)->Release(); (p)=NULL; } }
#endif

#define IDR_VS_BlitXenonGamma 134
#define IDR_PS_BlitXenonGamma 135

#define IDR_VS_BlitCellGamma 136
#define IDR_PS_BlitCellGamma 137

class ConsoleGamma
{
private:
    struct VertexFormat
    {
        float Pos[3];
        float TexCoord[2];
    };

    struct ShaderProgram
    {
        int vsResourceId;
        int psResourceId;
        IDirect3DVertexShader9** vs;
        IDirect3DPixelShader9** ps;
    };

    static inline bool g_initialized = false;
    static inline int g_lastGammaSetting = -1; // Very ugly

    static inline IDirect3DVertexBuffer9* mQuadVertexBuffer;
    static inline IDirect3DVertexDeclaration9* mQuadVertexDecl;

    static inline rage::grcRenderTargetPC* pSceneRT = nullptr;
    static inline IDirect3DSurface9* pSceneSurf = nullptr;

    static inline IDirect3DVertexShader9* g_vertexShader = nullptr;
    static inline IDirect3DPixelShader9* g_pixelShader = nullptr;

    static inline IDirect3DVertexShader9* VS_BlitXenonGamma = nullptr;
    static inline IDirect3DPixelShader9* PS_BlitXenonGamma = nullptr;

    static inline IDirect3DVertexShader9* VS_BlitCellGamma = nullptr;
    static inline IDirect3DPixelShader9* PS_BlitCellGamma = nullptr;

    // Size is not needed for dx9. Maybe it can be removed? In general all of this would look nicer if it would be closer to api specifications etc, this is just a lame port from dx11 for now.
    static bool LoadCompiledShaderResource(HMODULE hModule, int resourceId, const void** data, UINT* size)
    {
        HRSRC hRes = FindResourceW(hModule, MAKEINTRESOURCEW(resourceId), RT_RCDATA);
        if (!hRes)
            return false;

        HGLOBAL hGlob = LoadResource(hModule, hRes);
        if (!hGlob)
            return false;

        *data = LockResource(hGlob);
        if (!*data)
            return false;

        *size = SizeofResource(hModule, hRes);
        return *size != 0;
    }

    static ShaderProgram GetShaderProgram(int ConsoleGamma)
    {
        if (ConsoleGamma == 1) // Xenon gamma
        {
            return { IDR_VS_BlitXenonGamma, IDR_PS_BlitXenonGamma, &VS_BlitXenonGamma, &PS_BlitXenonGamma };
        }
        else if (ConsoleGamma == 2) // Cell gamma
        {
            return { IDR_VS_BlitCellGamma, IDR_PS_BlitCellGamma, &VS_BlitCellGamma, &PS_BlitCellGamma };
        }

        return { 0, 0, nullptr, nullptr };
    };

    // It would be nice to create an onMenuOptionChanged event for this, to just call it each time the option is changed
    static void ReloadShaders()
    {
        // Force reinitialization
        g_initialized = false;

        // Release current shaders and vertex stuff (Is this needed?)
        SAFE_RELEASE(g_vertexShader);
        SAFE_RELEASE(g_pixelShader);
        SAFE_RELEASE(mQuadVertexBuffer);
        SAFE_RELEASE(mQuadVertexDecl);

        // Release all cached shaders so they get recreated
        SAFE_RELEASE(VS_BlitXenonGamma);
        SAFE_RELEASE(PS_BlitXenonGamma);
        SAFE_RELEASE(VS_BlitCellGamma);
        SAFE_RELEASE(PS_BlitCellGamma);
    }

    static void __fastcall OnDeviceLost()
    {
        SAFE_RELEASE(pSceneSurf);

        if (pSceneRT)
        {
            pSceneRT->Destroy();
            pSceneRT = nullptr;
        }

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

        SAFE_RELEASE(pSceneSurf);
        if (pSceneRT)
        {
            pSceneRT->Destroy();
            pSceneRT = nullptr;
        }

        rage::grcRenderTargetDesc rtDesc{};
        rtDesc.mMultisampleCount = 0;
        rtDesc.field_0 = 1;
        rtDesc.field_12 = 1;
        rtDesc.mDepthRT = nullptr;
        rtDesc.field_8 = 1;
        rtDesc.field_10 = 1;
        rtDesc.field_11 = 1;
        rtDesc.field_24 = false;
        rtDesc.mFormat = rage::getEngineTextureFormat(desc.Format);

        auto rt = rage::grcTextureFactory::GetInstance()->CreateRenderTarget("ConsoleGammaScene", 3, nScreenWidth, nScreenHeight, desc.Width == nScreenWidth ? 32 : 32, &rtDesc);
        rage::grcDevice::grcResolveFlags resolveFlags{};
        rage::grcTextureFactoryPC::GetInstance()->LockRenderTarget(0, rt, nullptr);
        rage::grcTextureFactoryPC::GetInstance()->UnlockRenderTarget(0, &resolveFlags);
        pSceneRT = rt;

        if (!pSceneRT || !pSceneRT->mD3DTexture)
            return;

        pSceneRT->mD3DTexture->GetSurfaceLevel(0, &pSceneSurf);

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

    static bool Initialize()
    {
        if (g_initialized)
            return g_initialized;

        auto pDevice = rage::grcDevice::GetD3DDevice();
        if (!pDevice)
            return false;

        auto OnLostCB = rage::grcDevice::Functor0(NULL, OnDeviceLost, NULL, 0);
        auto OnResetCB = rage::grcDevice::Functor0(NULL, OnDeviceReset, NULL, 0);
        rage::grcDevice::RegisterDeviceCallbacks(OnLostCB, OnResetCB);

        static auto ConsoleGamma = FusionFixSettings.GetRef("PREF_CONSOLE_GAMMA");
        if (ConsoleGamma->get() != 1 && ConsoleGamma->get() != 2)
            return false;

        HMODULE hModule = NULL;
        GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCWSTR)&Initialize, &hModule);

        auto shaderProgram = GetShaderProgram(ConsoleGamma->get());

        if (shaderProgram.vsResourceId == 0)
            return false;

        const void* vsData = nullptr;
        UINT vsSize = 0;
        const void* psData = nullptr;
        UINT psSize = 0;

        HRESULT hResult;

        // Load compiled vertex shader 
        if (!*shaderProgram.vs)
        {
            if (!LoadCompiledShaderResource(hModule, shaderProgram.vsResourceId, &vsData, &vsSize))
                return false;

            hResult = pDevice->CreateVertexShader(reinterpret_cast<const DWORD*>(vsData), shaderProgram.vs);
            if (FAILED(hResult))
                return false;
        }

        // Load compiled pixel shader
        if (!*shaderProgram.ps)
        {
            if (!LoadCompiledShaderResource(hModule, shaderProgram.psResourceId, &psData, &psSize))
                return false;

            hResult = pDevice->CreatePixelShader(reinterpret_cast<const DWORD*>(psData), shaderProgram.ps);
            if (FAILED(hResult))
                return false;
        }

        g_vertexShader = *shaderProgram.vs;
        g_pixelShader = *shaderProgram.ps;

        OnDeviceReset();

        g_initialized = true;
        g_lastGammaSetting = FusionFixSettings.GetRef("PREF_CONSOLE_GAMMA")->get();
        return true;
    }

    static void Render()
    {
        static auto ConsoleGamma = FusionFixSettings.GetRef("PREF_CONSOLE_GAMMA");
        int current = ConsoleGamma->get();

        if (current != g_lastGammaSetting)
        {
            ReloadShaders();
            g_lastGammaSetting = current;
        }

        if (!current)
            return;

        if (!g_initialized && !Initialize())
            return;

        auto pDevice = rage::grcDevice::GetD3DDevice();
        if (!pDevice || !pSceneRT || !pSceneRT->mD3DTexture || !pSceneSurf || !g_vertexShader || !g_pixelShader || !mQuadVertexDecl || !mQuadVertexBuffer)
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

            pDevice->SetTexture(0, pSceneRT->mD3DTexture);

            pDevice->SetVertexShader(g_vertexShader);
            pDevice->SetPixelShader(g_pixelShader);

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
public:
    ConsoleGamma()
    {
        FusionFix::onInitEventAsync() += []()
        {
            if (GetD3DX9_43DLL())
            {
                FusionFix::onEndScene() += []()
                {
                    ConsoleGamma::Render();
                };
            }
        };
    }
} ConsoleGamma;