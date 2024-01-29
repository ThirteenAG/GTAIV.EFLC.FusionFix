module;

#include <common.hxx>
#include <d3d9.h>
#include <D3DX9Mesh.h>
#include <d3dx9.h>

export module antialiasing;

import common;
import comvars;
import shaders;
import fusiondxhook;
import settings;

#define IDR_FXAA        101
#define IDR_SMAA        102
#define IDR_AreaTex     103
#define IDR_SearchTex   104

#ifndef SAFE_RELEASE
#define SAFE_RELEASE(p)      { if (p) { (p)->Release(); (p)=NULL; } }
#endif

bool doPostFxAA = false;
bool bUseSSAA = false;

class ShadersAA
{
public:
    static inline IDirect3DTexture9* pHDRTex2 = nullptr;
    static inline IDirect3DTexture9* areaTex = nullptr;
    static inline IDirect3DTexture9* searchTex = nullptr;
    static inline IDirect3DTexture9* edgesTex = nullptr;
    static inline IDirect3DTexture9* blendTex = nullptr;
    static inline IDirect3DPixelShader9* FxaaPS = nullptr;
    static inline IDirect3DPixelShader9* SMAA_EdgeDetectionPS = nullptr;
    static inline IDirect3DPixelShader9* SMAA_BlendingWeightsCalculationPS = nullptr;
    static inline IDirect3DPixelShader9* SMAA_NeighborhoodBlendingPS = nullptr;
    static inline IDirect3DVertexShader9* SMAA_EdgeDetectionVS = nullptr;
    static inline IDirect3DVertexShader9* SMAA_BlendingWeightsCalculationVS = nullptr;
    static inline IDirect3DVertexShader9* SMAA_NeighborhoodBlendingVS = nullptr;

    static inline bool resourcesFinishedLoading()
    {
        return SMAA_EdgeDetectionPS && SMAA_BlendingWeightsCalculationPS && SMAA_NeighborhoodBlendingPS &&
               SMAA_EdgeDetectionVS && SMAA_BlendingWeightsCalculationVS && SMAA_NeighborhoodBlendingVS &&
               areaTex && searchTex && edgesTex && blendTex;
    }
};

// smaa postfx vertex array
struct VERTEX {
    D3DXVECTOR3 pos;
    D3DXVECTOR2 tex1;
};
VERTEX SmaaVertexArray[4] = { D3DXVECTOR3(0,0,0),  D3DXVECTOR2(0,0) };

// SMAA vertex array
void CreateSmaaVertexArray()
{
    D3DXVECTOR2 pixelSize = D3DXVECTOR2(1.0f / float(getWindowWidth()), 1.0f / float(getWindowHeight()));
    SmaaVertexArray[0].pos = D3DXVECTOR3(-1.0f - pixelSize.x, 1.0f + pixelSize.y, 0.5f);
    SmaaVertexArray[0].tex1 = D3DXVECTOR2(0.0f, 0.0f);
    SmaaVertexArray[1].pos = D3DXVECTOR3(1.0f - pixelSize.x, 1.0f + pixelSize.y, 0.5f);
    SmaaVertexArray[1].tex1 = D3DXVECTOR2(1.0f, 0.0f);
    SmaaVertexArray[2].pos = D3DXVECTOR3(-1.0f - pixelSize.x, -1.0f + pixelSize.y, 0.5f);
    SmaaVertexArray[2].tex1 = D3DXVECTOR2(0.0f, 1.0f);
    SmaaVertexArray[3].pos = D3DXVECTOR3(1.0f - pixelSize.x, -1.0f + pixelSize.y, 0.5f);
    SmaaVertexArray[3].tex1 = D3DXVECTOR2(1.0f, 1.0f);
}

class Antialiasing
{
public:
    Antialiasing()
    {
        FusionFix::onInitEvent() += []()
        {
            static HMODULE hm = NULL;
            GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCWSTR)&FusionFix::onInitEvent, &hm);
            bUseSSAA = FusionFixSettings.Get("PREF_SSAA") != 0;

            FusionFix::D3D9::onBeforeCreateTexture() += [](LPDIRECT3DDEVICE9& pDevice, UINT& Width, UINT& Height, UINT& Levels, DWORD& Usage, D3DFORMAT& Format, D3DPOOL& Pool, IDirect3DTexture9**& ppTexture, HANDLE*& pSharedHandle)
            {
                auto gWindowWidth = (gRect.right - gRect.left);
                auto gWindowHeight = (gRect.bottom - gRect.top);

                if (bUseSSAA && Width == getWindowWidth() && Height == getWindowHeight())
                {
                    Width *= 2;
                    Height *= 2;
                }
            };

            FusionFix::D3D9::onAfterCreateDevice() += [](LPDIRECT3D9& pDirect3D9, UINT& Adapter, D3DDEVTYPE& DeviceType, HWND& hFocusWindow, DWORD& BehaviorFlags, D3DPRESENT_PARAMETERS*& pPresentationParameters, IDirect3DDevice9**& ppReturnedDeviceInterface)
            {
                CreateSmaaVertexArray();
            };
            
            FusionFix::D3D9::onAfterReset() += [](LPDIRECT3DDEVICE9& pDevice, D3DPRESENT_PARAMETERS*& pPresentationParameters)
            {
                CreateSmaaVertexArray();
            };
            
            FusionFix::D3D9::onBeforeReset() += [](LPDIRECT3DDEVICE9& pDevice, D3DPRESENT_PARAMETERS*& pPresentationParameters)
            {            
                SAFE_RELEASE(ShadersAA::pHDRTex2);
                SAFE_RELEASE(ShadersAA::areaTex);
                SAFE_RELEASE(ShadersAA::searchTex);
                SAFE_RELEASE(ShadersAA::edgesTex);
                SAFE_RELEASE(ShadersAA::blendTex);
            };
            
            FusionFix::D3D9::onBeforeCreatePixelShader() += [](LPDIRECT3DDEVICE9& pDevice, DWORD*& pFunction, IDirect3DPixelShader9**& ppShader)
            {
                ID3DXBuffer* bf1 = nullptr;
                ID3DXBuffer* bf2 = nullptr;
                ID3DXConstantTable* ppConstantTable = nullptr;

                if (!ShadersAA::FxaaPS && D3DXAssembleShaderFromResourceW(hm, MAKEINTRESOURCEW(IDR_FXAA), NULL, NULL, 0, &bf1, &bf2) == S_OK)
                {
                    if (CreatePixelShaderOriginal.unsafe_stdcall<HRESULT>(pDevice, (DWORD*)bf1->GetBufferPointer(), &ShadersAA::FxaaPS) != S_OK || !ShadersAA::FxaaPS)
                        SAFE_RELEASE(ShadersAA::FxaaPS);
                    SAFE_RELEASE(bf1);
                    SAFE_RELEASE(bf2);
                }

                if (!ShadersAA::SMAA_EdgeDetectionPS && D3DXCompileShaderFromResourceW(hm, MAKEINTRESOURCEW(IDR_SMAA), NULL, NULL, "SMAAColorEdgeDetectionPS", "ps_3_0", 0, &bf1, &bf2, &ppConstantTable) == S_OK)
                {
                    if (CreatePixelShaderOriginal.unsafe_stdcall<HRESULT>(pDevice, (DWORD*)bf1->GetBufferPointer(), &ShadersAA::SMAA_EdgeDetectionPS) != S_OK || !ShadersAA::SMAA_EdgeDetectionPS)
                        SAFE_RELEASE(ShadersAA::SMAA_EdgeDetectionPS);
                    SAFE_RELEASE(bf1);
                    SAFE_RELEASE(bf2);
                    SAFE_RELEASE(ppConstantTable);
                }

                if (!ShadersAA::SMAA_EdgeDetectionVS && D3DXCompileShaderFromResourceW(hm, MAKEINTRESOURCEW(IDR_SMAA), NULL, NULL, "SMAAEdgeDetectionVS", "vs_3_0", 0, &bf1, &bf2, &ppConstantTable) == S_OK) {
                    if (pDevice->CreateVertexShader((DWORD*)bf1->GetBufferPointer(), &ShadersAA::SMAA_EdgeDetectionVS) != S_OK || !ShadersAA::SMAA_EdgeDetectionVS)
                        SAFE_RELEASE(ShadersAA::SMAA_EdgeDetectionVS);
                    SAFE_RELEASE(bf1);
                    SAFE_RELEASE(bf2);
                    SAFE_RELEASE(ppConstantTable);
                }
                
                if (!ShadersAA::SMAA_BlendingWeightsCalculationPS && D3DXCompileShaderFromResourceW(hm, MAKEINTRESOURCEW(IDR_SMAA), NULL, NULL, "SMAABlendingWeightCalculationPS", "ps_3_0", 0, &bf1, &bf2, &ppConstantTable) == S_OK) {
                    if (CreatePixelShaderOriginal.unsafe_stdcall<HRESULT>(pDevice, (DWORD*)bf1->GetBufferPointer(), &ShadersAA::SMAA_BlendingWeightsCalculationPS) != S_OK || !ShadersAA::SMAA_BlendingWeightsCalculationPS)
                        SAFE_RELEASE(ShadersAA::SMAA_BlendingWeightsCalculationPS);
                    SAFE_RELEASE(bf1);
                    SAFE_RELEASE(bf2);
                    SAFE_RELEASE(ppConstantTable);
                }

                if (!ShadersAA::SMAA_BlendingWeightsCalculationVS && D3DXCompileShaderFromResourceW(hm, MAKEINTRESOURCEW(IDR_SMAA), NULL, NULL, "SMAABlendingWeightCalculationVS", "vs_3_0", 0, &bf1, &bf2, &ppConstantTable) == S_OK) {
                    if (pDevice->CreateVertexShader((DWORD*)bf1->GetBufferPointer(), &ShadersAA::SMAA_BlendingWeightsCalculationVS) != S_OK || !ShadersAA::SMAA_BlendingWeightsCalculationVS)
                        SAFE_RELEASE(ShadersAA::SMAA_BlendingWeightsCalculationVS);
                    SAFE_RELEASE(bf1);
                    SAFE_RELEASE(bf2);
                    SAFE_RELEASE(ppConstantTable);
                }

                if (!ShadersAA::SMAA_NeighborhoodBlendingPS && D3DXCompileShaderFromResourceW(hm, MAKEINTRESOURCEW(IDR_SMAA), NULL, NULL, "SMAANeighborhoodBlendingPS", "ps_3_0", 0, &bf1, &bf2, &ppConstantTable) == S_OK) {
                    if (CreatePixelShaderOriginal.unsafe_stdcall<HRESULT>(pDevice, (DWORD*)bf1->GetBufferPointer(), &ShadersAA::SMAA_NeighborhoodBlendingPS) != S_OK || !ShadersAA::SMAA_NeighborhoodBlendingPS)
                        SAFE_RELEASE(ShadersAA::SMAA_NeighborhoodBlendingPS);
                    SAFE_RELEASE(bf1);
                    SAFE_RELEASE(bf2);
                    SAFE_RELEASE(ppConstantTable);
                }

                if (!ShadersAA::SMAA_NeighborhoodBlendingVS && D3DXCompileShaderFromResourceW(hm, MAKEINTRESOURCEW(IDR_SMAA), NULL, NULL, "SMAANeighborhoodBlendingVS", "vs_3_0", 0, &bf1, &bf2, &ppConstantTable) == S_OK) {
                    if (pDevice->CreateVertexShader((DWORD*)bf1->GetBufferPointer(), &ShadersAA::SMAA_NeighborhoodBlendingVS) != S_OK || !ShadersAA::SMAA_NeighborhoodBlendingVS)
                        SAFE_RELEASE(ShadersAA::SMAA_NeighborhoodBlendingVS);
                    SAFE_RELEASE(bf1);
                    SAFE_RELEASE(bf2);
                    SAFE_RELEASE(ppConstantTable);
                }
            };
            
            FusionFix::D3D9::onAfterCreateTexture() += [](LPDIRECT3DDEVICE9& pDevice, UINT& Width, UINT& Height, UINT& Levels, DWORD& Usage, D3DFORMAT& Format, D3DPOOL& Pool, IDirect3DTexture9**& ppTexture, HANDLE*& pSharedHandle)
            {
                if (Format == D3DFMT_A16B16G16R16F && ppTexture && (getWindowWidth() * (bUseSSAA ? 2 : 1)) && Height == (getWindowHeight() * (bUseSSAA ? 2 : 1)))
                {
                    SAFE_RELEASE(ShadersAA::pHDRTex2);
                    SAFE_RELEASE(ShadersAA::areaTex);
                    SAFE_RELEASE(ShadersAA::searchTex);
                    SAFE_RELEASE(ShadersAA::edgesTex);
                    SAFE_RELEASE(ShadersAA::blendTex);

                    // create new texture to postfx
                    if (SUCCEEDED(CreateTextureOriginal.unsafe_stdcall<HRESULT>(pDevice, Width, Height, Levels, Usage, Format, Pool, &ShadersAA::pHDRTex2, 0)) && ShadersAA::pHDRTex2)
                    {
                        if (D3DXCreateTextureFromResourceExW(pDevice, hm, MAKEINTRESOURCEW(IDR_AreaTex), 0, 0, 0, 0, D3DFMT_UNKNOWN, D3DPOOL_MANAGED, D3DX_FILTER_LINEAR, D3DX_FILTER_LINEAR, D3DCOLOR_ARGB(150, 100, 100, 100), NULL, NULL, &ShadersAA::areaTex) == S_OK)
                            CreateTextureOriginal.unsafe_stdcall<HRESULT>(pDevice, Width, Height, Levels, Usage, D3DFMT_X8R8G8B8, Pool, &ShadersAA::edgesTex, 0);
                        if (D3DXCreateTextureFromResourceExW(pDevice, hm, MAKEINTRESOURCEW(IDR_SearchTex), 0, 0, 0, 0, D3DFMT_UNKNOWN, D3DPOOL_MANAGED, D3DX_FILTER_LINEAR, D3DX_FILTER_LINEAR, D3DCOLOR_ARGB(150, 100, 100, 100), NULL, NULL, &ShadersAA::searchTex) == S_OK)
                            CreateTextureOriginal.unsafe_stdcall<HRESULT>(pDevice, Width, Height, Levels, Usage, D3DFMT_A8R8G8B8, Pool, &ShadersAA::blendTex, 0);
                    }
                }
            };

            FusionFix::onBeforePostFX() += []()
            {
                doPostFxAA = true;
            };

            FusionFix::D3D9::onBeforeDrawPrimitive() += [](LPDIRECT3DDEVICE9& pDevice, D3DPRIMITIVETYPE& PrimitiveType, UINT& StartVertex, UINT& PrimitiveCount)
            {
                static float vec4[4] = { 0.0f };
                IDirect3DPixelShader9* pShader = nullptr;
                IDirect3DSurface9* pHDRSurface = nullptr;
                IDirect3DSurface9* pHDRSurface2 = nullptr;
                IDirect3DSurface9* backBuffer = nullptr;
                IDirect3DSurface9* edgesSurf = nullptr;
                IDirect3DSurface9* blendSurf = nullptr;
                DWORD OldSRGB = 0;
                DWORD OldSampler = 0;
                #define PostfxTextureCount 5
                IDirect3DBaseTexture9* prePostFx[PostfxTextureCount] = { 0 };
                DWORD Samplers[PostfxTextureCount] = { D3DTEXF_LINEAR };
              
                if (doPostFxAA)
                {
                    if (bUseSSAA) {
                        pDevice->GetSamplerState(0, D3DSAMP_MAGFILTER, &OldSampler);
                        pDevice->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
                    }

                    pDevice->GetRenderTarget(0, &backBuffer);
                    if (backBuffer && ShadersAA::pHDRTex2)
                    {
                        // save sampler state
                        for (int i = 0; i < PostfxTextureCount; i++) {
                            pDevice->GetTexture(i, &prePostFx[i]);
                            pDevice->GetSamplerState(i, D3DSAMP_MAGFILTER, &Samplers[i]);
                        }

                        ShadersAA::pHDRTex2->GetSurfaceLevel(0, &pHDRSurface2);

                        static auto prefAA = FusionFixSettings.GetRef("PREF_ANTIALIASING");

                        // FXAA: Works normally as the game's post-processing shader
                        if (ShadersAA::FxaaPS && prefAA->get() == FusionFixSettings.AntialiasingText.eFXAA)
                        {
                            FusionFix::D3D9::setInsteadDrawPrimitive(true);
                            pDevice->SetRenderTarget(0, pHDRSurface2);
                            pDevice->Clear(0, 0, D3DCLEAR_TARGET, 0, 0, 0);
                            DrawPrimitiveOriginal.unsafe_stdcall<HRESULT>(pDevice, PrimitiveType, StartVertex, PrimitiveCount);

                            pDevice->SetPixelShader(ShadersAA::FxaaPS);

                            pDevice->SetRenderTarget(0, backBuffer);
                            pDevice->SetTexture(2, ShadersAA::pHDRTex2);

                            DrawPrimitiveOriginal.unsafe_stdcall<HRESULT>(pDevice, PrimitiveType, StartVertex, PrimitiveCount);

                            pDevice->SetTexture(2, 0);
                            pDevice->SetPixelShader(pShader);
                        }

                        // SMAA
                        // need to use the drawprimitiveup function to correct texture coordinates, 
                        // which is different from the game's post-processing.
                        else if (ShadersAA::resourcesFinishedLoading() && pHDRSurface2 && prefAA->get() == FusionFixSettings.AntialiasingText.eSMAA)
                        {
                            FusionFix::D3D9::setInsteadDrawPrimitive(true);
                            vec4[0] = 1.0f / float(getWindowWidth() * (bUseSSAA ? 2 : 1));
                            vec4[1] = 1.0f / float(getWindowHeight() * (bUseSSAA ? 2 : 1));
                            vec4[2] = float(getWindowWidth() * (bUseSSAA ? 2 : 1));
                            vec4[3] = float(getWindowHeight() * (bUseSSAA ? 2 : 1));
                            pDevice->SetPixelShaderConstantF(24, vec4, 1);
                            pDevice->SetVertexShaderConstantF(24, vec4, 1);
                            {
                                ShadersAA::edgesTex->GetSurfaceLevel(0, &edgesSurf);
                                ShadersAA::blendTex->GetSurfaceLevel(0, &blendSurf);

                                // game postfx
                                pDevice->SetRenderTarget(0, pHDRSurface2);
                                pDevice->Clear(0, 0, D3DCLEAR_TARGET, 0, 0, 0);
                                DrawPrimitiveOriginal.unsafe_stdcall<HRESULT>(pDevice, PrimitiveType, StartVertex, PrimitiveCount);

                                // SMAA_EdgeDetection
                                //constexpr DWORD VertexFVF = D3DFVF_XYZ | D3DFVF_TEX1;
                                //DWORD OldFVF = D3DFVF_XYZ | D3DFVF_TEX1;
                                pDevice->GetRenderState(D3DRS_SRGBWRITEENABLE, &OldSRGB); // save srgb state
                                pDevice->SetRenderState(D3DRS_SRGBWRITEENABLE, 0);
                                //pDevice->GetFVF(&OldFVF);
                                //pDevice->SetFVF(VertexFVF);
                                pDevice->SetPixelShader(ShadersAA::SMAA_EdgeDetectionPS);
                                pDevice->SetVertexShader(ShadersAA::SMAA_EdgeDetectionVS);
                                pDevice->SetPixelShaderConstantF(24, vec4, 1);
                                pDevice->SetVertexShaderConstantF(24, vec4, 1);
                                pDevice->SetRenderTarget(0, edgesSurf);
                                pDevice->SetTexture(0, ShadersAA::pHDRTex2);
                                pDevice->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
                                pDevice->SetSamplerState(1, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
                                pDevice->SetSamplerState(2, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
                                pDevice->SetSamplerState(3, D3DSAMP_MAGFILTER, D3DTEXF_POINT);
                                pDevice->SetSamplerState(4, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);

                                pDevice->Clear(0, 0, D3DCLEAR_TARGET, 0, 0, 0);
                                pDevice->SetSamplerState(0, D3DSAMP_SRGBTEXTURE, 1);
                                //pDevice->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, SmaaVertexArray, sizeof(VERTEX));
                                DrawPrimitiveOriginal.unsafe_stdcall<HRESULT>(pDevice, PrimitiveType, StartVertex, PrimitiveCount);

                                pDevice->SetSamplerState(0, D3DSAMP_SRGBTEXTURE, 0);

                                pDevice->SetTexture(0, NULL);

                                // SMAA_BlendingWeightsCalculation
                                pDevice->SetPixelShader(ShadersAA::SMAA_BlendingWeightsCalculationPS);
                                pDevice->SetVertexShader(ShadersAA::SMAA_BlendingWeightsCalculationVS);
                                pDevice->SetPixelShaderConstantF(24, vec4, 1);
                                pDevice->SetVertexShaderConstantF(24, vec4, 1);
                                pDevice->SetRenderTarget(0, blendSurf);
                                pDevice->SetTexture(1, ShadersAA::edgesTex);
                                pDevice->SetTexture(2, ShadersAA::areaTex);
                                pDevice->SetTexture(3, ShadersAA::searchTex);
                                pDevice->Clear(0, 0, D3DCLEAR_TARGET, 0, 0, 0);
                                //pDevice->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, SmaaVertexArray, sizeof(VERTEX));
                                DrawPrimitiveOriginal.unsafe_stdcall<HRESULT>(pDevice, PrimitiveType, StartVertex, PrimitiveCount);


                                pDevice->SetTexture(1, NULL);
                                pDevice->SetTexture(2, NULL);
                                pDevice->SetTexture(3, NULL);

                                pDevice->SetRenderState(D3DRS_SRGBWRITEENABLE, OldSRGB);// restore srgb state

                                // SMAA_NeighborhoodBlending
                                pDevice->SetPixelShader(ShadersAA::SMAA_NeighborhoodBlendingPS);
                                pDevice->SetVertexShader(ShadersAA::SMAA_NeighborhoodBlendingVS);
                                pDevice->SetPixelShaderConstantF(24, vec4, 1);
                                pDevice->SetVertexShaderConstantF(24, vec4, 1);

                                pDevice->SetRenderTarget(0, backBuffer);

                                pDevice->SetTexture(0, ShadersAA::pHDRTex2);
                                pDevice->SetTexture(1, ShadersAA::edgesTex);
                                pDevice->SetTexture(4, ShadersAA::blendTex);

                                pDevice->SetRenderState(D3DRS_SRGBWRITEENABLE, 0);

                                //pDevice->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, SmaaVertexArray, sizeof(VERTEX));
                                DrawPrimitiveOriginal.unsafe_stdcall<HRESULT>(pDevice, PrimitiveType, StartVertex, PrimitiveCount);


                                pDevice->SetTexture(1, NULL);
                                pDevice->SetTexture(4, NULL);

                                pDevice->SetPixelShader(pShader);
                                //pDevice->SetFVF(OldFVF);

                                SAFE_RELEASE(edgesSurf);
                                SAFE_RELEASE(blendSurf);
                            }
                        }

                        // restore sampler state
                        for (int i = 0; i < PostfxTextureCount; i++) {
                            if (prePostFx[i]) {
                                pDevice->SetTexture(i, prePostFx[i]);
                                SAFE_RELEASE(prePostFx[i]);
                            }
                            pDevice->SetSamplerState(i, D3DSAMP_MAGFILTER, Samplers[i]);
                        }

                        SAFE_RELEASE(backBuffer);
                        SAFE_RELEASE(pHDRSurface2);
                        if (bUseSSAA) {
                            pDevice->SetSamplerState(0, D3DSAMP_MAGFILTER, OldSampler);
                        }
                        doPostFxAA = false;
                    };
                    SAFE_RELEASE(backBuffer);
                }
            };

            FusionFix::D3D9::onSetPixelShaderConstantF() += [](LPDIRECT3DDEVICE9& pDevice, UINT& StartRegister, float*& pConstantData, UINT& Vector4fCount)
            {
                if (bUseSSAA && pConstantData[0] == 1.f / getWindowWidth() && pConstantData[1] == 1.f / getWindowHeight() && Vector4fCount == 1) {
                    pConstantData[0] *= 0.5f;
                    pConstantData[1] *= 0.5f;
                    pConstantData[2] *= 2.0f;
                    pConstantData[3] *= 2.0f;
                } else if (bUseSSAA && pConstantData[0] == getWindowWidth() && pConstantData[1] == getWindowHeight() && Vector4fCount == 1) {
                    pConstantData[0] *= 2.0f;
                    pConstantData[1] *= 2.0f;
                    pConstantData[2] *= 0.5f;
                    pConstantData[3] *= 0.5f;
                }
            };
        };
    }
} Antialiasing;