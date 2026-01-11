module;

#include <common.hxx>
#include <d3dx9tex.h>

export module reflectionmsaa;

import common;
import comvars;
import d3dx9_43;
import settings;

#ifndef SAFE_RELEASE
#define SAFE_RELEASE(p) { if (p) { (p)->Release(); (p)=NULL; } }
#endif

#define IDR_RefMipBlur_VS 130
#define IDR_RefMipBlur_PS 131
#define IDR_RefMipBlurBlend_PS 132

bool bDrawReflections = false;

float* dwC3F9A4 = nullptr;
bool* dw1320FA8 = nullptr;

class ReflectionMSAA
{
private:
    struct VertexFormat
    {
        float Pos[3];
        float TexCoord[2];
    };

    static inline IDirect3DVertexBuffer9* mQuadVertexBuffer;
    static inline IDirect3DVertexDeclaration9* mQuadVertexDecl;

    // Game render targets
    static inline rage::grcRenderTargetPC* ReflectionMapColourRT = nullptr;

    // Shaders
    static inline IDirect3DVertexShader9* RefMipBlur_VS = nullptr;
    static inline IDirect3DPixelShader9* RefMipBlur_PS = nullptr;
    static inline IDirect3DPixelShader9* RefMipBlurBlend_PS = nullptr;

    static void __fastcall OnDeviceLost()
    {
        ReflectionMapColourRT = nullptr;

        if (mQuadVertexBuffer)
        {
            mQuadVertexBuffer->Release();
            mQuadVertexBuffer = nullptr;
        }

        if (mQuadVertexDecl)
        {
            mQuadVertexDecl->Release();
            mQuadVertexDecl = nullptr;
        }
    }

    static void __fastcall OnDeviceReset()
    {
        auto pDevice = rage::grcDevice::GetD3DDevice();
        if (!pDevice)
            return;

        ReflectionMapColourRT = rage::grcTextureFactoryPC::GetRTByName("REFLECTION_MAP_COLOUR");

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
            if (FAILED(pDevice->CreateVertexBuffer(6 * sizeof(VertexFormat), 0, 0, D3DPOOL_DEFAULT, &mQuadVertexBuffer, nullptr)) || !mQuadVertexBuffer)
            {
                mQuadVertexBuffer = nullptr;
                return;
            }

            VertexFormat* VertexData = nullptr;
            if (FAILED(mQuadVertexBuffer->Lock(0, 0, reinterpret_cast<void**>(&VertexData), 0)) || !VertexData)
            {
                mQuadVertexBuffer->Release();
                mQuadVertexBuffer = nullptr;
                return;
            }

            VertexData[0] = { 0.0f, 1.0f, 0.0f, 0.0f, 0.0f };
            VertexData[1] = { 0.0f, 0.0f, 0.0f, 0.0f, 1.0f };
            VertexData[2] = { 1.0f, 1.0f, 0.0f, 1.0f, 0.0f };
            VertexData[3] = { 0.0f, 0.0f, 0.0f, 0.0f, 1.0f };
            VertexData[4] = { 1.0f, 0.0f, 0.0f, 1.0f, 1.0f };
            VertexData[5] = { 1.0f, 1.0f, 0.0f, 1.0f, 0.0f };

            mQuadVertexBuffer->Unlock();
        }
    }

    static void OnBeforeLighting()
    {
        auto pDevice = rage::grcDevice::GetD3DDevice();
        if (!pDevice)
            return;

        if (!ReflectionMapColourRT || !RefMipBlur_VS || !RefMipBlur_PS || !RefMipBlurBlend_PS || !mQuadVertexDecl || !mQuadVertexBuffer)
            return;

        DWORD prevAddressV = 0;
        DWORD prevAddressU = 0;
        DWORD prevAddressW = 0;
        DWORD prevMinFilter = 0;
        DWORD prevMagFilter = 0;
        DWORD prevMipFilter = 0;

        DWORD prevCullMode = 0;
        DWORD prevZEnable = 0;
        DWORD prevZWriteEnable = 0;
        DWORD prevAlphaBlendEnable = 0;
        DWORD prevAlphaTestEnable = 0;
        DWORD prevSrcBlend = 0;
        DWORD prevDestBlend = 0;

        // Previous surfaces, depth/stencil, textures, shaders
        IDirect3DSurface9* prevSurface = nullptr;
        IDirect3DSurface9* prevDepthStencilSurface = nullptr;

        IDirect3DBaseTexture9* prevTex = nullptr;

        IDirect3DVertexShader9* prevVS = nullptr;
        IDirect3DPixelShader9* prevPS = nullptr;

        IDirect3DSurface9* ReflectionMapColourSurface = nullptr;

        // Store previous sampler states, renderstates, surfaces, textures, shaders
        pDevice->GetRenderState(D3DRS_CULLMODE, &prevCullMode);
        pDevice->GetRenderState(D3DRS_ZENABLE, &prevZEnable);
        pDevice->GetRenderState(D3DRS_ZWRITEENABLE, &prevZWriteEnable);
        pDevice->GetRenderState(D3DRS_ALPHABLENDENABLE, &prevAlphaBlendEnable);
        pDevice->GetRenderState(D3DRS_ALPHATESTENABLE, &prevAlphaTestEnable);
        pDevice->GetRenderState(D3DRS_SRCBLEND, &prevSrcBlend);
        pDevice->GetRenderState(D3DRS_DESTBLEND, &prevDestBlend);

        pDevice->GetSamplerState(0, D3DSAMP_ADDRESSU, &prevAddressU);
        pDevice->GetSamplerState(0, D3DSAMP_ADDRESSV, &prevAddressV);
        pDevice->GetSamplerState(0, D3DSAMP_ADDRESSW, &prevAddressW);
        pDevice->GetSamplerState(0, D3DSAMP_MINFILTER, &prevMinFilter);
        pDevice->GetSamplerState(0, D3DSAMP_MAGFILTER, &prevMagFilter);
        pDevice->GetSamplerState(0, D3DSAMP_MIPFILTER, &prevMipFilter);

        pDevice->GetRenderTarget(0, &prevSurface);
        pDevice->GetDepthStencilSurface(&prevDepthStencilSurface);

        pDevice->GetVertexShader(&prevVS);
        pDevice->GetPixelShader(&prevPS);

        static auto rq = FusionFixSettings.GetRef("PREF_REFLECTION_RESOLUTION");
        if (((rq && rq->get()) && bDrawReflections) || (dw1320FA8 && *dw1320FA8))
        {
            pDevice->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
            pDevice->SetRenderState(D3DRS_ZENABLE, D3DZB_FALSE);
            pDevice->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);

            pDevice->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
            pDevice->SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
            pDevice->SetSamplerState(0, D3DSAMP_ADDRESSW, D3DTADDRESS_WRAP);
            pDevice->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
            pDevice->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
            pDevice->SetSamplerState(0, D3DSAMP_MIPFILTER, D3DTEXF_LINEAR);

            rage::Vector4 RefMipBlurParams{ 1.0f, 1.0f, 0.0f, 0.0f};
            rage::Vector4 PixelOffset{};

            pDevice->SetVertexDeclaration(mQuadVertexDecl);
            pDevice->SetStreamSource(0, mQuadVertexBuffer, 0, sizeof(VertexFormat));

            pDevice->SetVertexShader(RefMipBlur_VS);

            for (uint32_t levels = 1; levels < ReflectionMapColourRT->mLevels; levels++)
            {
                ReflectionMapColourRT->mD3DTexture->GetSurfaceLevel(levels, &ReflectionMapColourSurface);

                if (ReflectionMapColourSurface)
                {
                    pDevice->SetRenderTarget(0, ReflectionMapColourSurface);
                    pDevice->SetDepthStencilSurface(nullptr);

                    if (*dw1320FA8)
                    {
                        pDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
                        pDevice->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
                        pDevice->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
                        pDevice->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);

                        PixelOffset.x = (0.5f / (float)(ReflectionMapColourRT->mWidth >> levels));
                        PixelOffset.y = (0.5f / (float)(ReflectionMapColourRT->mHeight >> levels));
                        RefMipBlurParams.z = 0.0f;

                        float blend = 1.0f;
                        if ((1.0f - *dwC3F9A4) >= 0.0f)
                        {
                            if ((1.0f - *dwC3F9A4) <= 1.0f)
                                blend = 1.0f - *dwC3F9A4;
                            else
                                blend = 1.0f;
                        }
                        else
                        {
                            blend = 0.0f;
                        }
                        RefMipBlurParams.w = blend;

                        pDevice->SetVertexShaderConstantF(238, &PixelOffset.x, 1);
                        pDevice->SetPixelShaderConstantF(206, &RefMipBlurParams.x, 1);

                        pDevice->SetTexture(0, ReflectionMapColourRT->mD3DTexture);

                        pDevice->SetPixelShader(RefMipBlurBlend_PS);

                        pDevice->DrawPrimitive(D3DPT_TRIANGLELIST, 0, 2);
                    }

                    pDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
                    pDevice->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);

                    PixelOffset.x = (0.5f / (float)(ReflectionMapColourRT->mWidth >> levels));
                    PixelOffset.y = (0.5f / (float)(ReflectionMapColourRT->mHeight >> levels));
                    RefMipBlurParams.z = ((float)levels - 1.0f);
                    RefMipBlurParams.w = (1.0f / ((float)levels + 1.0f));
                    pDevice->SetVertexShaderConstantF(238, &PixelOffset.x, 1);
                    pDevice->SetPixelShaderConstantF(206, &RefMipBlurParams.x, 1);

                    pDevice->SetTexture(0, ReflectionMapColourRT->mD3DTexture);

                    pDevice->SetPixelShader(RefMipBlur_PS);

                    pDevice->DrawPrimitive(D3DPT_TRIANGLELIST, 0, 2);

                    SAFE_RELEASE(ReflectionMapColourSurface);
                }
            }
        }

        pDevice->SetRenderState(D3DRS_CULLMODE, prevCullMode);
        pDevice->SetRenderState(D3DRS_ZENABLE, prevZEnable);
        pDevice->SetRenderState(D3DRS_ZWRITEENABLE, prevZWriteEnable);
        pDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, prevAlphaBlendEnable);
        pDevice->SetRenderState(D3DRS_ALPHATESTENABLE, prevAlphaTestEnable);
        pDevice->SetRenderState(D3DRS_SRCBLEND, prevSrcBlend);
        pDevice->SetRenderState(D3DRS_DESTBLEND, prevDestBlend);

        pDevice->SetSamplerState(0, D3DSAMP_ADDRESSU, prevAddressU);
        pDevice->SetSamplerState(0, D3DSAMP_ADDRESSV, prevAddressV);
        pDevice->SetSamplerState(0, D3DSAMP_ADDRESSW, prevAddressW);
        pDevice->SetSamplerState(0, D3DSAMP_MINFILTER, prevMinFilter);
        pDevice->SetSamplerState(0, D3DSAMP_MAGFILTER, prevMagFilter);
        pDevice->SetSamplerState(0, D3DSAMP_MIPFILTER, prevMipFilter);

        pDevice->SetRenderTarget(0, prevSurface);
        pDevice->SetDepthStencilSurface(prevDepthStencilSurface);

        pDevice->SetTexture(0, prevTex);

        pDevice->SetVertexShader(prevVS);
        pDevice->SetPixelShader(prevPS);

        // Release
        SAFE_RELEASE(prevSurface);
        SAFE_RELEASE(prevDepthStencilSurface);
        SAFE_RELEASE(prevTex);
        SAFE_RELEASE(prevVS);
        SAFE_RELEASE(prevPS);
    }

    static void Init()
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

        HMODULE hm = NULL;
        GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCWSTR)&Init, &hm);

        ID3DXBuffer* bf1 = nullptr;
        ID3DXBuffer* bf2 = nullptr;

        //Shader ASM
        if (!RefMipBlur_VS)
        {
            if (D3DXAssembleShaderFromResourceW(hm, MAKEINTRESOURCEW(IDR_RefMipBlur_VS), NULL, NULL, 0, &bf1, &bf2) == S_OK)
            {
                if (pDevice->CreateVertexShader((DWORD*)bf1->GetBufferPointer(), &RefMipBlur_VS) != S_OK || !RefMipBlur_VS)
                    SAFE_RELEASE(RefMipBlur_VS);
                SAFE_RELEASE(bf1);
                SAFE_RELEASE(bf2);
            }
        }

        if (!RefMipBlur_PS)
        {
            if (D3DXAssembleShaderFromResourceW(hm, MAKEINTRESOURCEW(IDR_RefMipBlur_PS), NULL, NULL, 0, &bf1, &bf2) == S_OK)
            {
                if (pDevice->CreatePixelShader((DWORD*)bf1->GetBufferPointer(), &RefMipBlur_PS) != S_OK || !RefMipBlur_PS)
                    SAFE_RELEASE(RefMipBlur_PS);
                SAFE_RELEASE(bf1);
                SAFE_RELEASE(bf2);
            }
        }

        if (!RefMipBlurBlend_PS)
        {
            if (D3DXAssembleShaderFromResourceW(hm, MAKEINTRESOURCEW(IDR_RefMipBlurBlend_PS), NULL, NULL, 0, &bf1, &bf2) == S_OK)
            {
                if (pDevice->CreatePixelShader((DWORD*)bf1->GetBufferPointer(), &RefMipBlurBlend_PS) != S_OK || !RefMipBlurBlend_PS)
                    SAFE_RELEASE(RefMipBlurBlend_PS);
                SAFE_RELEASE(bf1);
                SAFE_RELEASE(bf2);
            }
        }

        OnDeviceReset();

        bInitialized = true;
    }
public:
    ReflectionMSAA()
    {
        FusionFix::onInitEventAsync() += []()
        {
            CIniReader iniReader("");

            static int nReflectionMSAAQuality = std::clamp(iniReader.ReadInteger("EXPERIMENTAL", "ReflectionMSAAQuality", 0), 0, 8);

            if (nReflectionMSAAQuality > 0 && nReflectionMSAAQuality != 6 && nReflectionMSAAQuality % 2 == 0)
            {
                if (GetD3DX9_43DLL())
                {                
                    auto pattern = hook::pattern("C7 44 24 ? ? ? ? ? BF ? ? ? ? C7 44 24 ? ? ? ? ? E8");
                    if (!pattern.empty())
                    {
                        injector::WriteMemory(pattern.count(2).get(0).get<void*>(4), nReflectionMSAAQuality, true);
                        injector::WriteMemory(pattern.count(2).get(1).get<void*>(4), nReflectionMSAAQuality, true);
                    }
                    else
                    {
                        pattern = hook::pattern("C7 44 24 ? ? ? ? ? BF ? ? ? ? C7 44 24 ? ? ? ? ? E8 ? ? ? ? 83 C4 ? 84 C0 74");
                        injector::WriteMemory(pattern.get_first(4), nReflectionMSAAQuality, true);

                        pattern = hook::pattern("89 5C 24 ? BF ? ? ? ? C7 44 24");
                        injector::MakeNOP(pattern.get_first(0), 4, true);
                        static auto CreateMirrorRTHook = safetyhook::create_mid(pattern.get_first(0), [](SafetyHookContext& regs)
                        {
                            *(int*)(regs.esp + 0x54 - 0x2C) = nReflectionMSAAQuality;
                        });
                    }

                    pattern = hook::pattern("C7 44 24 ? ? ? ? ? C7 44 24 ? ? ? ? ? 8B 01 6A ? 68 ? ? ? ? FF 50 ? 8B 0D ? ? ? ? 89 44 24");
                    if (!pattern.empty())
                        injector::WriteMemory(pattern.get_first(4), nReflectionMSAAQuality, true);
                    else
                    {
                        pattern = hook::pattern("89 5C 24 ? C7 44 24 ? ? ? ? ? 8B 11 8B 52 ? 6A ? 68 ? ? ? ? FF D2 8B 0D ? ? ? ? 8D 54 24");
                        injector::MakeNOP(pattern.get_first(0), 4, true);
                        static auto WaterReflectionRTDescriptionHook = safetyhook::create_mid(pattern.get_first(0), [](SafetyHookContext& regs)
                        {
                            *(int*)(regs.esp + 0xB8 - 0x80) = nReflectionMSAAQuality;
                        });
                    }

                    pattern = hook::pattern("C7 44 24 ? ? ? ? ? 78");
                    if (!pattern.empty())
                        injector::WriteMemory(pattern.get_first(4), nReflectionMSAAQuality, true);
                    else
                    {
                        pattern = hook::pattern("89 5C 24 ? 7C ? BE");
                        injector::MakeNOP(pattern.get_first(0), 4, true);
                        static auto CreateReflectionMapColourRTHook = safetyhook::create_mid(pattern.get_first(0), [](SafetyHookContext& regs)
                        {
                            *(int*)(regs.esp + 0xA8 - 0x80) = nReflectionMSAAQuality;
                        });
                    }

                    pattern = find_pattern("80 3D ? ? ? ? ? 57 8B F9 75 ? 80 3D", "80 3D ? ? ? ? ? 55 8B E9 75 ? 80 3D");
                    static auto CRenderPhaseReflection__BuildDrawListHook = safetyhook::create_mid(pattern.get_first(0), [](SafetyHookContext& regs)
                    {
                        bDrawReflections = false;
                    });

                    pattern = find_pattern("C3 53 55 56 6A", "C3 56 57 6A ? 6A ? 89 2D");
                    static auto CRenderPhaseReflection__BuildRenderListHook2 = safetyhook::create_mid(pattern.get_first(1), [](SafetyHookContext& regs)
                    {
                        bDrawReflections = true;
                    });
                    
                    pattern = find_pattern("C6 05 ? ? ? ? ? 8B 97", "C6 05 ? ? ? ? ? 8B 8E ? ? ? ? 85 C9");
                    dw1320FA8 = *pattern.get_first<bool*>(2);

                    pattern = hook::pattern("F3 0F 11 05 ? ? ? ? F3 0F 10 4F");
                    if (!pattern.empty())
                        dwC3F9A4 = *pattern.get_first<float*>(4);
                    else
                    {
                        pattern = hook::pattern("D9 05 ? ? ? ? F3 0F 10 43 ? 51");
                        dwC3F9A4 = *pattern.get_first<float*>(2);
                    }

                    // Skip game's reflection mip and blur pass (exterior & interior)
                    pattern = find_pattern("0F 8E ? ? ? ? 8D 49 ? 6A", "0F 8E ? ? ? ? EB ? 8D 49 ? 6A ? 6A");
                    injector::WriteMemory<uint16_t>(pattern.get_first(0), 0xE990, true);

                    pattern = find_pattern("0F 8E ? ? ? ? C7 44 24 ? ? ? ? ? 6A", "0F 8E ? ? ? ? C7 44 24 ? ? ? ? ? 6A");
                    injector::WriteMemory<uint16_t>(pattern.get_first(0), 0xE990, true);

                    // Our custom reflection mip and blur pass to work around MSAA/mip problem
                    CRenderPhaseDeferredLighting_LightsToScreen::OnBuildRenderList() += []()
                    {
                        Init();
                        auto mBeforeLightingCB = new T_CB_Generic_NoArgs(OnBeforeLighting);
                        if (mBeforeLightingCB)
                            mBeforeLightingCB->Append();
                    };
                }
            }
        };
    }
} ReflectionMSAA;