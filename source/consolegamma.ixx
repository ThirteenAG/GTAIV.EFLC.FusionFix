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

#define IDR_VS_BlitXenonGamma_Dither 134
#define IDR_PS_BlitXenonGamma_Dither 135

#define IDR_VS_BlitCellGamma_Dither 136
#define IDR_PS_BlitCellGamma_Dither 137

class ConsoleGamma
{
private:
    struct VertexFormat
    {
        float Pos[4];
        float TexCoord[2];
    };

    struct ShaderProgram
    {
        int vsResourceId;
        int psResourceId;
        IDirect3DVertexShader9** vs;
        IDirect3DPixelShader9** ps;
    };

    struct PipelineState
    {
        DWORD cullMode = 0;
        DWORD zEnable = 0;
        DWORD zWriteEnable = 0;
        DWORD zFunc = 0;
        DWORD alphaBlendEnable = 0;
        DWORD alphaTestEnable = 0;
        DWORD scissorTestEnable = 0;
        DWORD lighting = 0;
        DWORD fogEnable = 0;

        DWORD addressU = 0;
        DWORD addressV = 0;
        DWORD addressW = 0;
        DWORD minFilter = 0;
        DWORD magFilter = 0;
        DWORD mipFilter = 0;

        IDirect3DSurface9* renderTarget = nullptr;
        IDirect3DSurface9* depthStencil = nullptr;

        IDirect3DBaseTexture9* texture = nullptr;

        IDirect3DVertexShader9* vertexShader = nullptr;
        IDirect3DPixelShader9* pixelShader = nullptr;

        IDirect3DVertexBuffer9* vertexBuffer = nullptr;
        IDirect3DVertexDeclaration9* vertexDecl = nullptr;
        UINT vertexOffset = 0;
        UINT vertexStride = 0;
    };

    static inline bool g_initialized = false;
    static inline UINT g_width = 0;
    static inline UINT g_height = 0;

    // Very ugly
    static inline int g_lastGammaSetting = -1;

    static inline IDirect3DVertexShader9* g_vertexShader = nullptr;
    static inline IDirect3DPixelShader9* g_pixelShader = nullptr;

    static inline IDirect3DVertexBuffer9* g_vertexBuffer;
    static inline IDirect3DVertexDeclaration9* g_vertexDecl;

    static inline rage::grcRenderTargetPC* pSceneRT = nullptr;
    static inline IDirect3DSurface9* pSceneSurf = nullptr;

    static void SaveState(IDirect3DDevice9* device, PipelineState& st)
    {
        device->GetRenderState(D3DRS_CULLMODE, &st.cullMode);
        device->GetRenderState(D3DRS_ZENABLE, &st.zEnable);
        device->GetRenderState(D3DRS_ZWRITEENABLE, &st.zWriteEnable);
        device->GetRenderState(D3DRS_ZFUNC, &st.zFunc);
        device->GetRenderState(D3DRS_ALPHABLENDENABLE, &st.alphaBlendEnable);
        device->GetRenderState(D3DRS_ALPHATESTENABLE, &st.alphaTestEnable);
        device->GetRenderState(D3DRS_SCISSORTESTENABLE, &st.scissorTestEnable);
        device->GetRenderState(D3DRS_LIGHTING, &st.lighting);
        device->GetRenderState(D3DRS_FOGENABLE, &st.fogEnable);

        device->GetSamplerState(0, D3DSAMP_ADDRESSU, &st.addressU);
        device->GetSamplerState(0, D3DSAMP_ADDRESSV, &st.addressV);
        device->GetSamplerState(0, D3DSAMP_ADDRESSW, &st.addressW);
        device->GetSamplerState(0, D3DSAMP_MINFILTER, &st.minFilter);
        device->GetSamplerState(0, D3DSAMP_MAGFILTER, &st.magFilter);
        device->GetSamplerState(0, D3DSAMP_MIPFILTER, &st.mipFilter);

        device->GetRenderTarget(0, &st.renderTarget);
        device->GetDepthStencilSurface(&st.depthStencil);
        device->GetTexture(0, &st.texture);
        device->GetVertexShader(&st.vertexShader);
        device->GetPixelShader(&st.pixelShader);
        device->GetVertexDeclaration(&st.vertexDecl);
        device->GetStreamSource(0, &st.vertexBuffer, &st.vertexOffset, &st.vertexStride);
    }

    static void RestoreState(IDirect3DDevice9* device, PipelineState& st)
    {
        device->SetRenderState(D3DRS_CULLMODE, st.cullMode);
        device->SetRenderState(D3DRS_ZENABLE, st.zEnable);
        device->SetRenderState(D3DRS_ZWRITEENABLE, st.zWriteEnable);
        device->SetRenderState(D3DRS_ZFUNC, st.zFunc);
        device->SetRenderState(D3DRS_ALPHABLENDENABLE, st.alphaBlendEnable);
        device->SetRenderState(D3DRS_ALPHATESTENABLE, st.alphaTestEnable);
        device->SetRenderState(D3DRS_SCISSORTESTENABLE, st.scissorTestEnable);
        device->SetRenderState(D3DRS_LIGHTING, st.lighting);
        device->SetRenderState(D3DRS_FOGENABLE, st.fogEnable);

        device->SetSamplerState(0, D3DSAMP_ADDRESSU, st.addressU);
        device->SetSamplerState(0, D3DSAMP_ADDRESSV, st.addressV);
        device->SetSamplerState(0, D3DSAMP_ADDRESSW, st.addressW);
        device->SetSamplerState(0, D3DSAMP_MINFILTER, st.minFilter);
        device->SetSamplerState(0, D3DSAMP_MAGFILTER, st.magFilter);
        device->SetSamplerState(0, D3DSAMP_MIPFILTER, st.mipFilter);

        device->SetRenderTarget(0, st.renderTarget);
        device->SetDepthStencilSurface(st.depthStencil);
        device->SetTexture(0, st.texture);
        device->SetVertexShader(st.vertexShader);
        device->SetPixelShader(st.pixelShader);
        device->SetVertexDeclaration(st.vertexDecl);
        device->SetStreamSource(0, st.vertexBuffer, st.vertexOffset, st.vertexStride);

        // Release the references we got from Get* calls
        SAFE_RELEASE(st.renderTarget);
        SAFE_RELEASE(st.depthStencil);
        SAFE_RELEASE(st.texture);
        SAFE_RELEASE(st.vertexShader);
        SAFE_RELEASE(st.pixelShader);
        SAFE_RELEASE(st.vertexDecl);
        SAFE_RELEASE(st.vertexBuffer);
    }

    // Shader identifiers
    static inline IDirect3DVertexShader9* VS_BlitXenonGamma_Dither = nullptr;
    static inline IDirect3DPixelShader9* PS_BlitXenonGamma_Dither = nullptr;

    static inline IDirect3DVertexShader9* VS_BlitCellGamma_Dither = nullptr;
    static inline IDirect3DPixelShader9* PS_BlitCellGamma_Dither = nullptr;

    static const DWORD* LoadCompiledShaderResource(HMODULE hModule, int resourceId)
    {
        HRSRC hRes = FindResourceW(hModule, MAKEINTRESOURCEW(resourceId), RT_RCDATA);
        if (!hRes)
            return nullptr;

        HGLOBAL hGlob = LoadResource(hModule, hRes);
        if (!hGlob)
            return nullptr;

        return reinterpret_cast<const DWORD*>(LockResource(hGlob));
    }

    static ShaderProgram GetShaderProgram(int ConsoleGamma)
    {
        if (ConsoleGamma == 1) // Xenon gamma
        {
            return { IDR_VS_BlitXenonGamma_Dither, IDR_PS_BlitXenonGamma_Dither, &VS_BlitXenonGamma_Dither, &PS_BlitXenonGamma_Dither };
        }
        else if (ConsoleGamma == 2) // Cell gamma
        {
            return { IDR_VS_BlitCellGamma_Dither, IDR_PS_BlitCellGamma_Dither, &VS_BlitCellGamma_Dither, &PS_BlitCellGamma_Dither };
        }

        return { 0, 0, nullptr, nullptr };
    };

    // It would be nice to create an onMenuOptionChanged event for this, to just call it each time the option is changed
    static void ReloadShaders()
    {
        // Force reinitialization
        g_initialized = false;

        // Set current shaders to nullptr
        g_vertexShader = nullptr;
        g_pixelShader = nullptr;

        SAFE_RELEASE(g_vertexBuffer);
        SAFE_RELEASE(g_vertexDecl);

        // Release all cached shaders so they get recreated
        SAFE_RELEASE(VS_BlitXenonGamma_Dither);
        SAFE_RELEASE(PS_BlitXenonGamma_Dither);
        SAFE_RELEASE(VS_BlitCellGamma_Dither);
        SAFE_RELEASE(PS_BlitCellGamma_Dither);
    }

    static void __fastcall OnDeviceLost()
    {
        SAFE_RELEASE(pSceneSurf);

        if (pSceneRT)
        {
            pSceneRT->Destroy();

            pSceneRT = nullptr;
        }

        SAFE_RELEASE(g_vertexBuffer);
        SAFE_RELEASE(g_vertexDecl);
    }

    static void __fastcall OnDeviceReset()
    {
        auto device = rage::grcDevice::GetD3DDevice();
        if (!device)
            return;

        IDirect3DSurface9* backBuffer = GetRealBackBuffer(device);
        if (!backBuffer)
            return;

        D3DSURFACE_DESC backBufferDesc{};
        backBuffer->GetDesc(&backBufferDesc);

        g_width  = backBufferDesc.Width;
        g_height = backBufferDesc.Height;

        SAFE_RELEASE(backBuffer);

        SAFE_RELEASE(pSceneSurf);

        if (pSceneRT)
        {
            pSceneRT->Destroy();

            pSceneRT = nullptr;
        }

        rage::grcRenderTargetDesc renderTargetDesc{};
        renderTargetDesc.mMultisampleCount = 0;
        renderTargetDesc.field_0 = 1;
        renderTargetDesc.field_12 = 1;
        renderTargetDesc.mDepthRT = nullptr;
        renderTargetDesc.field_8 = 1;
        renderTargetDesc.field_10 = 1;
        renderTargetDesc.field_11 = 1;
        renderTargetDesc.field_24 = false;
        renderTargetDesc.mFormat = rage::getEngineTextureFormat(backBufferDesc.Format);

        auto renderTarget = rage::grcTextureFactory::GetInstance()->CreateRenderTarget("ConsoleGammaScene", 3, g_width, g_height, 32, &renderTargetDesc);

        rage::grcDevice::grcResolveFlags resolveFlags{};
        rage::grcTextureFactoryPC::GetInstance()->LockRenderTarget(0, renderTarget, nullptr);
        rage::grcTextureFactoryPC::GetInstance()->UnlockRenderTarget(0, &resolveFlags);

        pSceneRT = renderTarget;
        if (!pSceneRT || !pSceneRT->mD3DTexture)
            return;

        pSceneRT->mD3DTexture->GetSurfaceLevel(0, &pSceneSurf);

        if (!g_vertexDecl)
        {
            D3DVERTEXELEMENT9 vertexElements[] =
            {
                { 0,  0, D3DDECLTYPE_FLOAT4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
                { 0, 16, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0 },
                D3DDECL_END()
            };

            HRESULT hResult = device->CreateVertexDeclaration(vertexElements, &g_vertexDecl);
            if (FAILED(hResult))
                return;
        }

        if (!g_vertexBuffer)
        {
            HRESULT hResult = device->CreateVertexBuffer(6 * sizeof(VertexFormat), 0, 0, D3DPOOL_DEFAULT, &g_vertexBuffer, nullptr);
            if (FAILED(hResult))
                return;
        }

        if (g_vertexBuffer)
        {
            VertexFormat* vertices = nullptr;
            if (SUCCEEDED(g_vertexBuffer->Lock(0, 0, reinterpret_cast<void**>(&vertices), 0)))
            {
                const float HalfPixelOffsetX = 0.5f / (float)g_width;
                const float HalfPixelOffsetY = 0.5f / (float)g_height;

                vertices[0] = { -1.0f,  1.0f, 0.0f, 1.0f, 0.0f + HalfPixelOffsetX, 0.0f + HalfPixelOffsetY };
                vertices[1] = { -1.0f, -1.0f, 0.0f, 1.0f, 0.0f + HalfPixelOffsetX, 1.0f + HalfPixelOffsetY };
                vertices[2] = {  1.0f,  1.0f, 0.0f, 1.0f, 1.0f + HalfPixelOffsetX, 0.0f + HalfPixelOffsetY };
                vertices[3] = { -1.0f, -1.0f, 0.0f, 1.0f, 0.0f + HalfPixelOffsetX, 1.0f + HalfPixelOffsetY };
                vertices[4] = {  1.0f, -1.0f, 0.0f, 1.0f, 1.0f + HalfPixelOffsetX, 1.0f + HalfPixelOffsetY };
                vertices[5] = {  1.0f,  1.0f, 0.0f, 1.0f, 1.0f + HalfPixelOffsetX, 0.0f + HalfPixelOffsetY };

                g_vertexBuffer->Unlock();
            }
        }
    }

    static IDirect3DSurface9* GetRealBackBuffer(IDirect3DDevice9* device)
    {
        IDirect3DSwapChain9* swapChain = nullptr;
        IDirect3DSurface9* backBuffer = nullptr;

        if (SUCCEEDED(device->GetSwapChain(0, &swapChain)) && swapChain)
        {
            swapChain->GetBackBuffer(0, D3DBACKBUFFER_TYPE_MONO, &backBuffer);

            swapChain->Release();
        }

        return backBuffer;
    }

    static bool Initialize(IDirect3DDevice9* device)
    {
        if (g_initialized || !device)
            return g_initialized;

        static bool deviceCallbacksRegistered = false;
        if (!deviceCallbacksRegistered)
        {
            auto onDeviceLostCB = rage::grcDevice::Functor0(NULL, OnDeviceLost, NULL, 0);
            auto onDeviceResetCB = rage::grcDevice::Functor0(NULL, OnDeviceReset, NULL, 0);
            rage::grcDevice::RegisterDeviceCallbacks(onDeviceLostCB, onDeviceResetCB);

            deviceCallbacksRegistered = true;
        }

        static auto ConsoleGamma = FusionFixSettings.GetRef("PREF_CONSOLE_GAMMA");
        if (ConsoleGamma->get() != 1 && ConsoleGamma->get() != 2)
            return false;

        HMODULE hModule = NULL;
        GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCWSTR)&Initialize, &hModule);

        auto shaderProgram = GetShaderProgram(ConsoleGamma->get());

        if (shaderProgram.vsResourceId == 0)
            return false;

        // Load compiled vertex shader 
        if (!*shaderProgram.vs)
        {
            const DWORD* vsData = LoadCompiledShaderResource(hModule, shaderProgram.vsResourceId);
            if (!vsData)
                return false;

            HRESULT hResult = device->CreateVertexShader(vsData, shaderProgram.vs);
            if (FAILED(hResult))
                return false;
        }

        // Load compiled pixel shader
        if (!*shaderProgram.ps)
        {
            const DWORD* psData = LoadCompiledShaderResource(hModule, shaderProgram.psResourceId);
            if (!psData)
                return false;

            HRESULT hResult = device->CreatePixelShader(psData, shaderProgram.ps);
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

    static void Render(IDirect3DDevice9* device)
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

        if (!g_initialized && !Initialize(device))
            return;

        if (!device || !pSceneRT || !pSceneRT->mD3DTexture || !pSceneSurf || !g_vertexShader || !g_pixelShader || !g_vertexDecl || !g_vertexBuffer)
            return;

        IDirect3DSurface9* backBuffer = GetRealBackBuffer(device);
        if (!backBuffer)
            return;

        IDirect3DSurface9* currentRenderTarget = nullptr;
        device->GetRenderTarget(0, &currentRenderTarget);
        if (currentRenderTarget)
        {
            HRESULT hResult = device->StretchRect(currentRenderTarget, nullptr, pSceneSurf, nullptr, D3DTEXF_POINT);
            if (FAILED(hResult))
            {
                SAFE_RELEASE(backBuffer);

                return;
            }

            SAFE_RELEASE(currentRenderTarget);
        }

        PipelineState old = {};

        SaveState(device, old);

        device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
        device->SetRenderState(D3DRS_ZENABLE, D3DZB_FALSE);
        device->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
        device->SetRenderState(D3DRS_ZFUNC, D3DCMP_ALWAYS);
        device->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
        device->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
        device->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
        device->SetRenderState(D3DRS_LIGHTING, FALSE);
        device->SetRenderState(D3DRS_FOGENABLE, FALSE);

        device->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
        device->SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
        device->SetSamplerState(0, D3DSAMP_ADDRESSW, D3DTADDRESS_CLAMP);
        device->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
        device->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
        device->SetSamplerState(0, D3DSAMP_MIPFILTER, D3DTEXF_LINEAR);

        device->SetRenderTarget(0, backBuffer);
        device->SetDepthStencilSurface(nullptr);

        device->SetVertexDeclaration(g_vertexDecl);
        device->SetStreamSource(0, g_vertexBuffer, 0, sizeof(VertexFormat));
        device->SetTexture(0, pSceneRT->mD3DTexture);
        device->SetVertexShader(g_vertexShader);
        device->SetPixelShader(g_pixelShader);

        device->DrawPrimitive(D3DPT_TRIANGLELIST, 0, 2);

        device->SetTexture(0, nullptr);

        RestoreState(device, old);

        SAFE_RELEASE(backBuffer);
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
                    IDirect3DDevice9* device = rage::grcDevice::GetD3DDevice();

                    ConsoleGamma::Render(device);
                };
            }
        };
    }
} ConsoleGamma;