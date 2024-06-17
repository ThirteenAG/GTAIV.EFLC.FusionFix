module;

#include <common.hxx>
#include <d3dx9.h>
#include <map>

export module postfx;

import common;
import comvars;
import natives;
import shaders;
import fusiondxhook;
import settings;
import d3dx9_43;

#define IDR_FXAA                                 101
#define IDR_SMAA                                 102
#define IDR_AreaTex                              103
#define IDR_SearchTex                            104
#define IDR_bluenoisevolume                      105

#define IDR_dof_blur_ps                          110
#define IDR_dof_coc_ps                           111
#define IDR_depth_of_field_ps                    112
#define IDR_depth_of_field_tent_ps               113
#define IDR_depth_of_field_blur_ps               114
#define IDR_depth_of_field_coc_ps                115
#define IDR_stipple_filter_ps                    116
#define IDR_DeferredShadowGen_ps                 117
#define IDR_DeferredShadowUse1_ps                118
#define IDR_DeferredShadowUse2_ps                119
#define IDR_SSAO_ps                              120
#define IDR_SSAO_ps2                             121
#define IDR_SSAO_vs                              122

#define IDR_SSAO3_ps                             123
#define IDR_SMAA2                                124
#define IDR_DOF_ps                               125
#define IDR_DeferredShadowBlur                   126
#define IDR_SunShafts_PS                         127
#define IDR_CascadeAtlasGen                      128
#define IDR_SSDiffuseCloudsGen                   129

#ifndef SAFE_RELEASE
#define SAFE_RELEASE(p)      { if (p) { (p)->Release(); (p)=NULL; } }
#endif

bool doPostFx = false;
bool useSSAA = false;
std::optional<std::reference_wrapper<int32_t>> UsePostFxAA;

class PostFxResource {
public:
    IDirect3DTexture9* NormalTex;
    IDirect3DTexture9* DiffuseTex;
    IDirect3DTexture9* SpecularTex;
    IDirect3DTexture9* DepthTex;
    IDirect3DTexture9* StencilTex;
    IDirect3DTexture9* BloomTex;

    // postfx textures
    IDirect3DTexture9* pHDRTex = nullptr; // game hdr texture
    IDirect3DTexture9* pHDRTex2 = nullptr; // main temp texture
    IDirect3DTexture9* pHDRTex3 = nullptr; // main temp texture

    IDirect3DTexture9* pHalfHDRTex = nullptr; // game half res screen texture
    IDirect3DTexture9* pQuarterHDRTex = nullptr; //  game 1/4 res screen texture
    IDirect3DTexture9* pHDRDownsampleTex = nullptr; // main downsampled texture
    IDirect3DTexture9* pHDRDownsampleTex2 = nullptr; // main downsampled texture

    IDirect3DTexture9* pShadowBlurTex1 = nullptr; // main shadow temp texture
    IDirect3DTexture9* pShadowBlurTex2 = nullptr; // main shadow temp texture

    // smaa textures
    IDirect3DTexture9* areaTex = nullptr; // loaded from file
    IDirect3DTexture9* searchTex = nullptr; // loaded from file
    IDirect3DTexture9* edgesTex = nullptr; // smaa gen
    IDirect3DTexture9* blendTex = nullptr; // smaa gen

    IDirect3DVolumeTexture9* bluenoisevolume = nullptr; // loaded from file

    IDirect3DTexture9* aoTex = nullptr;
    // unused
    IDirect3DTexture9* aoHalfTex = nullptr;
    IDirect3DTexture9* halfDepthDsTex = nullptr;
    IDirect3DTexture9* stencilDownsampled = nullptr; // gen
    IDirect3DTexture9* mainDepthTex = nullptr; // gen
    IDirect3DTexture9* oldDepthTex = nullptr; // gen
    IDirect3DSurface9* mainDepth = nullptr; // gen
    IDirect3DSurface9* oldDepth = nullptr; // gen

    // temp set and used in postfx
    IDirect3DTexture9* renderTargetTex = nullptr;
    IDirect3DTexture9* textureRead = nullptr;
    IDirect3DSurface9* renderTargetSurf = nullptr;
    IDirect3DSurface9* surfaceRead = nullptr;

    // for bilinear interpolation on shadows
    IDirect3DTexture9* OldShadowAtlas = nullptr;
    IDirect3DTexture9* NewShadowAtlas = nullptr;
    IDirect3DSurface9* OldShadowAtlasSurf = nullptr;

    IDirect3DTexture9* depthStenciltex = nullptr; // gen

    IDirect3DPixelShader9* FxaaPS = nullptr;

    IDirect3DPixelShader9* SunShafts_PS = nullptr;
    IDirect3DPixelShader9* SunShafts2_PS = nullptr;
    IDirect3DPixelShader9* SunShafts3_PS = nullptr;
    IDirect3DPixelShader9* SunShafts4_PS = nullptr;
    IDirect3DPixelShader9* downsampler_ps = nullptr;
    IDirect3DPixelShader9* SSDownsampler_PS = nullptr;
    IDirect3DPixelShader9* SSDownsampler2_PS = nullptr;
    IDirect3DPixelShader9* SSDiffuseCloudsGen_PS = nullptr;
    IDirect3DPixelShader9* SSAO_ps = nullptr;
    IDirect3DPixelShader9* SSAO_ps2 = nullptr;
    IDirect3DVertexShader9* SSAO_vs = nullptr;

    IDirect3DPixelShader9* SSAdd_PS = nullptr;

    IDirect3DPixelShader9* DOF_ps = nullptr;
    IDirect3DPixelShader9* dof_blur_ps = nullptr;
    IDirect3DPixelShader9* dof_coc_ps = nullptr;

    IDirect3DPixelShader9* depth_of_field_ps = nullptr;
    IDirect3DPixelShader9* depth_of_field_tent_ps = nullptr;
    IDirect3DPixelShader9* depth_of_field_blur_ps = nullptr;
    IDirect3DPixelShader9* depth_of_field_coc_ps = nullptr;
    IDirect3DPixelShader9* stipple_filter_ps = nullptr;

    IDirect3DPixelShader9* SMAA_EdgeDetection = nullptr;
    IDirect3DPixelShader9* SMAA_BlendingWeightsCalculation = nullptr;
    IDirect3DPixelShader9* SMAA_NeighborhoodBlending = nullptr;
    IDirect3DVertexShader9* SMAA_EdgeDetectionVS = nullptr;
    IDirect3DVertexShader9* SMAA_BlendingWeightsCalculationVS = nullptr;
    IDirect3DVertexShader9* SMAA_NeighborhoodBlendingVS = nullptr;

    IDirect3DPixelShader9* DeferredShadowGen_ps = nullptr;
    IDirect3DPixelShader9* DeferredShadowBlur1_ps = nullptr;
    IDirect3DPixelShader9* DeferredShadowBlur2_ps = nullptr;
    IDirect3DPixelShader9* DeferredShadowBlur3_ps = nullptr;
    IDirect3DPixelShader9* DeferredShadowUse1_ps = nullptr;
    IDirect3DPixelShader9* DeferredShadowUse2_ps = nullptr;

    IDirect3DPixelShader9* CascadeAtlasGen = nullptr;

    std::map<IDirect3DPixelShader9*, int> ShaderListPS;
    std::map<IDirect3DVertexShader9*, int> ShaderListVS;

    float AoDistance = 100;
    float AOFocusPoint[4] = { 350, 0, 0, 0 };
    float AOFocusScale[4] = { 300, 0, 0, 0 };
    float AAparameters[4] = { 0.25f , 0.125f , 0.0f, 0.0f };
    float SunShafts_params1[4] = { 1.5f, 0.95f, 0.01f, 0.975f };
    float SunShafts_params2[4] = { 0.5f, 1.f, 2.f, 1.5f };

    float ResSSAA = 1.0f;

    // 0 off, 1 horizontal, 2 vertical, 3 horizontal e vertical.
    int useDefferedShadows = 3;
    int UseSunShafts = 2;
    int useDepthOfField = 3;
    int UseDebugTextures = 0;
    bool UseSSAO = false;
    bool useStippleFilter = false;
    bool useNewShadowAtlas = false;

    int SunShaftsSamples[4] = { 20, 20, 20, 20 };
    int DofSamples[4] = { 20 , 20, 20 ,20 };
    float NoiseSale[4] = { 1.f / 256, 0.3f, -0.5f, 0 };
    float BilateralDepthTreshold[4] = { 0.003f, 0.002f, 0.004f, 0.005f };

    bool fixDistantOutlineUsingDXVK = false;

    bool shadersLoaded = false;

    bool loadShaders(LPDIRECT3DDEVICE9 pDevice, HMODULE hm) {
        ID3DXBuffer* bf1 = nullptr;
        ID3DXBuffer* bf2 = nullptr;
        ID3DXConstantTable* ppConstantTable = nullptr;

        //asm
        if (!FxaaPS && D3DXAssembleShaderFromResourceW(hm, MAKEINTRESOURCEW(IDR_FXAA), NULL, NULL, 0, &bf1, &bf2) == S_OK) { if (CreatePixelShaderOriginal.unsafe_stdcall<HRESULT>(pDevice, (DWORD*)bf1->GetBufferPointer(), &FxaaPS) != S_OK || !FxaaPS) SAFE_RELEASE(FxaaPS); SAFE_RELEASE(bf1); SAFE_RELEASE(bf2); }
        if (!dof_blur_ps && D3DXAssembleShaderFromResourceW(hm, MAKEINTRESOURCEW(IDR_dof_blur_ps), NULL, NULL, 0, &bf1, &bf2) == S_OK) { if (CreatePixelShaderOriginal.unsafe_stdcall<HRESULT>(pDevice, (DWORD*)bf1->GetBufferPointer(), &dof_blur_ps) != S_OK || !dof_blur_ps) SAFE_RELEASE(dof_blur_ps); SAFE_RELEASE(bf1); SAFE_RELEASE(bf2); }
        if (!dof_coc_ps && D3DXAssembleShaderFromResourceW(hm, MAKEINTRESOURCEW(IDR_dof_coc_ps), NULL, NULL, 0, &bf1, &bf2) == S_OK) { if (CreatePixelShaderOriginal.unsafe_stdcall<HRESULT>(pDevice, (DWORD*)bf1->GetBufferPointer(), &dof_coc_ps) != S_OK || !dof_coc_ps) SAFE_RELEASE(dof_coc_ps); SAFE_RELEASE(bf1); SAFE_RELEASE(bf2); }
        if (!depth_of_field_ps && D3DXAssembleShaderFromResourceW(hm, MAKEINTRESOURCEW(IDR_depth_of_field_ps), NULL, NULL, 0, &bf1, &bf2) == S_OK) { if (CreatePixelShaderOriginal.unsafe_stdcall<HRESULT>(pDevice, (DWORD*)bf1->GetBufferPointer(), &depth_of_field_ps) != S_OK || !depth_of_field_ps) SAFE_RELEASE(depth_of_field_ps); SAFE_RELEASE(bf1); SAFE_RELEASE(bf2); }
        if (!depth_of_field_tent_ps && D3DXAssembleShaderFromResourceW(hm, MAKEINTRESOURCEW(IDR_depth_of_field_tent_ps), NULL, NULL, 0, &bf1, &bf2) == S_OK) { if (CreatePixelShaderOriginal.unsafe_stdcall<HRESULT>(pDevice, (DWORD*)bf1->GetBufferPointer(), &depth_of_field_tent_ps) != S_OK || !depth_of_field_tent_ps) SAFE_RELEASE(depth_of_field_tent_ps); SAFE_RELEASE(bf1); SAFE_RELEASE(bf2); }
        if (!depth_of_field_blur_ps && D3DXAssembleShaderFromResourceW(hm, MAKEINTRESOURCEW(IDR_depth_of_field_blur_ps), NULL, NULL, 0, &bf1, &bf2) == S_OK) { if (CreatePixelShaderOriginal.unsafe_stdcall<HRESULT>(pDevice, (DWORD*)bf1->GetBufferPointer(), &depth_of_field_blur_ps) != S_OK || !depth_of_field_blur_ps) SAFE_RELEASE(depth_of_field_blur_ps); SAFE_RELEASE(bf1); SAFE_RELEASE(bf2); }
        if (!depth_of_field_coc_ps && D3DXAssembleShaderFromResourceW(hm, MAKEINTRESOURCEW(IDR_depth_of_field_coc_ps), NULL, NULL, 0, &bf1, &bf2) == S_OK) { if (CreatePixelShaderOriginal.unsafe_stdcall<HRESULT>(pDevice, (DWORD*)bf1->GetBufferPointer(), &depth_of_field_coc_ps) != S_OK || !depth_of_field_coc_ps) SAFE_RELEASE(depth_of_field_coc_ps); SAFE_RELEASE(bf1); SAFE_RELEASE(bf2); }
        if (!stipple_filter_ps && D3DXAssembleShaderFromResourceW(hm, MAKEINTRESOURCEW(IDR_stipple_filter_ps), NULL, NULL, 0, &bf1, &bf2) == S_OK) { if (CreatePixelShaderOriginal.unsafe_stdcall<HRESULT>(pDevice, (DWORD*)bf1->GetBufferPointer(), &stipple_filter_ps) != S_OK || !stipple_filter_ps) SAFE_RELEASE(stipple_filter_ps); SAFE_RELEASE(bf1); SAFE_RELEASE(bf2); }
        if (!DeferredShadowGen_ps && D3DXAssembleShaderFromResourceW(hm, MAKEINTRESOURCEW(IDR_DeferredShadowGen_ps), NULL, NULL, 0, &bf1, &bf2) == S_OK) { if (CreatePixelShaderOriginal.unsafe_stdcall<HRESULT>(pDevice, (DWORD*)bf1->GetBufferPointer(), &DeferredShadowGen_ps) != S_OK || !DeferredShadowGen_ps) SAFE_RELEASE(DeferredShadowGen_ps); SAFE_RELEASE(bf1); SAFE_RELEASE(bf2); }
        if (!DeferredShadowUse1_ps && D3DXAssembleShaderFromResourceW(hm, MAKEINTRESOURCEW(IDR_DeferredShadowUse1_ps), NULL, NULL, 0, &bf1, &bf2) == S_OK) { if (CreatePixelShaderOriginal.unsafe_stdcall<HRESULT>(pDevice, (DWORD*)bf1->GetBufferPointer(), &DeferredShadowUse1_ps) != S_OK || !DeferredShadowUse1_ps) SAFE_RELEASE(DeferredShadowUse1_ps); SAFE_RELEASE(bf1); SAFE_RELEASE(bf2); }
        if (!DeferredShadowUse2_ps && D3DXAssembleShaderFromResourceW(hm, MAKEINTRESOURCEW(IDR_DeferredShadowUse2_ps), NULL, NULL, 0, &bf1, &bf2) == S_OK) { if (CreatePixelShaderOriginal.unsafe_stdcall<HRESULT>(pDevice, (DWORD*)bf1->GetBufferPointer(), &DeferredShadowUse2_ps) != S_OK || !DeferredShadowUse2_ps) SAFE_RELEASE(DeferredShadowUse2_ps); SAFE_RELEASE(bf1); SAFE_RELEASE(bf2); }
        if (!SSAO_ps && D3DXAssembleShaderFromResourceW(hm, MAKEINTRESOURCEW(IDR_SSAO_ps), NULL, NULL, 0, &bf1, &bf2) == S_OK) { if (CreatePixelShaderOriginal.unsafe_stdcall<HRESULT>(pDevice, (DWORD*)bf1->GetBufferPointer(), &SSAO_ps) != S_OK || !SSAO_ps) SAFE_RELEASE(SSAO_ps); SAFE_RELEASE(bf1); SAFE_RELEASE(bf2); }
        if (!SSAO_ps2 && D3DXAssembleShaderFromResourceW(hm, MAKEINTRESOURCEW(IDR_SSAO_ps2), NULL, NULL, 0, &bf1, &bf2) == S_OK) { if (CreatePixelShaderOriginal.unsafe_stdcall<HRESULT>(pDevice, (DWORD*)bf1->GetBufferPointer(), &SSAO_ps2) != S_OK || !SSAO_ps2) SAFE_RELEASE(SSAO_ps2); SAFE_RELEASE(bf1); SAFE_RELEASE(bf2); }
        if (!SSAO_vs && D3DXAssembleShaderFromResourceW(hm, MAKEINTRESOURCEW(IDR_SSAO_vs), NULL, NULL, 0, &bf1, &bf2) == S_OK) { if (CreateVertexShaderOriginal.unsafe_stdcall<HRESULT>(pDevice, (DWORD*)bf1->GetBufferPointer(), &SSAO_vs) != S_OK || !SSAO_vs) SAFE_RELEASE(SSAO_vs); SAFE_RELEASE(bf1); SAFE_RELEASE(bf2); }
        if (!CascadeAtlasGen && D3DXAssembleShaderFromResourceW(hm, MAKEINTRESOURCEW(IDR_CascadeAtlasGen), NULL, NULL, 0, &bf1, &bf2) == S_OK) { if (CreatePixelShaderOriginal.unsafe_stdcall<HRESULT>(pDevice, (DWORD*)bf1->GetBufferPointer(), &CascadeAtlasGen) != S_OK || !CascadeAtlasGen) SAFE_RELEASE(CascadeAtlasGen); SAFE_RELEASE(bf1); SAFE_RELEASE(bf2); }
        if (!SSDiffuseCloudsGen_PS && D3DXAssembleShaderFromResourceW(hm, MAKEINTRESOURCEW(IDR_SSDiffuseCloudsGen), NULL, NULL, 0, &bf1, &bf2) == S_OK) { if (CreatePixelShaderOriginal.unsafe_stdcall<HRESULT>(pDevice, (DWORD*)bf1->GetBufferPointer(), &SSDiffuseCloudsGen_PS) != S_OK || !SSDiffuseCloudsGen_PS) SAFE_RELEASE(SSDiffuseCloudsGen_PS); SAFE_RELEASE(bf1); SAFE_RELEASE(bf2); }

        //hlsl
        if (!downsampler_ps && D3DXCompileShaderFromResourceW(hm, MAKEINTRESOURCEW(IDR_SSAO3_ps), NULL, NULL, "mainDownsample", "ps_3_0", 0, &bf1, &bf2, &ppConstantTable) == S_OK) { if (CreatePixelShaderOriginal.unsafe_stdcall<HRESULT>(pDevice, (DWORD*)bf1->GetBufferPointer(), &downsampler_ps) != S_OK || !downsampler_ps) SAFE_RELEASE(downsampler_ps); SAFE_RELEASE(bf1); SAFE_RELEASE(bf2); SAFE_RELEASE(ppConstantTable); }
        if (!SunShafts_PS && D3DXCompileShaderFromResourceW(hm, MAKEINTRESOURCEW(IDR_SunShafts_PS), NULL, NULL, "SunShafts1", "ps_3_0", 0, &bf1, &bf2, &ppConstantTable) == S_OK) { if (CreatePixelShaderOriginal.unsafe_stdcall<HRESULT>(pDevice, (DWORD*)bf1->GetBufferPointer(), &SunShafts_PS) != S_OK || !SunShafts_PS) SAFE_RELEASE(SunShafts_PS); SAFE_RELEASE(bf1); SAFE_RELEASE(bf2); SAFE_RELEASE(ppConstantTable); }
        if (!SunShafts2_PS && D3DXCompileShaderFromResourceW(hm, MAKEINTRESOURCEW(IDR_SunShafts_PS), NULL, NULL, "SunShafts2", "ps_3_0", 0, &bf1, &bf2, &ppConstantTable) == S_OK) { if (CreatePixelShaderOriginal.unsafe_stdcall<HRESULT>(pDevice, (DWORD*)bf1->GetBufferPointer(), &SunShafts2_PS) != S_OK || !SunShafts2_PS) SAFE_RELEASE(SunShafts2_PS); SAFE_RELEASE(bf1); SAFE_RELEASE(bf2); SAFE_RELEASE(ppConstantTable); }
        if (!SunShafts3_PS && D3DXCompileShaderFromResourceW(hm, MAKEINTRESOURCEW(IDR_SunShafts_PS), NULL, NULL, "SunShafts3", "ps_3_0", 0, &bf1, &bf2, &ppConstantTable) == S_OK) { if (CreatePixelShaderOriginal.unsafe_stdcall<HRESULT>(pDevice, (DWORD*)bf1->GetBufferPointer(), &SunShafts3_PS) != S_OK || !SunShafts3_PS) SAFE_RELEASE(SunShafts3_PS); SAFE_RELEASE(bf1); SAFE_RELEASE(bf2); SAFE_RELEASE(ppConstantTable); }
        if (!SunShafts4_PS && D3DXCompileShaderFromResourceW(hm, MAKEINTRESOURCEW(IDR_SunShafts_PS), NULL, NULL, "SunShafts4", "ps_3_0", 0, &bf1, &bf2, &ppConstantTable) == S_OK) { if (CreatePixelShaderOriginal.unsafe_stdcall<HRESULT>(pDevice, (DWORD*)bf1->GetBufferPointer(), &SunShafts4_PS) != S_OK || !SunShafts4_PS) SAFE_RELEASE(SunShafts4_PS); SAFE_RELEASE(bf1); SAFE_RELEASE(bf2); SAFE_RELEASE(ppConstantTable); }
        if (!SSDownsampler_PS && D3DXCompileShaderFromResourceW(hm, MAKEINTRESOURCEW(IDR_SunShafts_PS), NULL, NULL, "SSDownsampler", "ps_3_0", 0, &bf1, &bf2, &ppConstantTable) == S_OK) { if (CreatePixelShaderOriginal.unsafe_stdcall<HRESULT>(pDevice, (DWORD*)bf1->GetBufferPointer(), &SSDownsampler_PS) != S_OK || !SSDownsampler_PS) SAFE_RELEASE(SSDownsampler_PS); SAFE_RELEASE(bf1); SAFE_RELEASE(bf2); SAFE_RELEASE(ppConstantTable); }
        if (!SSDownsampler2_PS && D3DXCompileShaderFromResourceW(hm, MAKEINTRESOURCEW(IDR_SunShafts_PS), NULL, NULL, "SSDownsampler2", "ps_3_0", 0, &bf1, &bf2, &ppConstantTable) == S_OK) { if (CreatePixelShaderOriginal.unsafe_stdcall<HRESULT>(pDevice, (DWORD*)bf1->GetBufferPointer(), &SSDownsampler2_PS) != S_OK || !SSDownsampler2_PS) SAFE_RELEASE(SSDownsampler2_PS); SAFE_RELEASE(bf1); SAFE_RELEASE(bf2); SAFE_RELEASE(ppConstantTable); }

        if (!SSAdd_PS && D3DXCompileShaderFromResourceW(hm, MAKEINTRESOURCEW(IDR_SunShafts_PS), NULL, NULL, "SSAdd", "ps_3_0", 0, &bf1, &bf2, &ppConstantTable) == S_OK) { if (CreatePixelShaderOriginal.unsafe_stdcall<HRESULT>(pDevice, (DWORD*)bf1->GetBufferPointer(), &SSAdd_PS) != S_OK || !SSAdd_PS) SAFE_RELEASE(SSAdd_PS); SAFE_RELEASE(bf1); SAFE_RELEASE(bf2); SAFE_RELEASE(ppConstantTable); }
        if (!DeferredShadowBlur1_ps && D3DXCompileShaderFromResourceW(hm, MAKEINTRESOURCEW(IDR_DeferredShadowBlur), NULL, NULL, "BlurHorizontal", "ps_3_0", 0, &bf1, &bf2, &ppConstantTable) == S_OK) { if (CreatePixelShaderOriginal.unsafe_stdcall<HRESULT>(pDevice, (DWORD*)bf1->GetBufferPointer(), &DeferredShadowBlur1_ps) != S_OK || !DeferredShadowBlur1_ps) SAFE_RELEASE(DeferredShadowBlur1_ps); SAFE_RELEASE(bf1); SAFE_RELEASE(bf2); SAFE_RELEASE(ppConstantTable); }
        if (!DeferredShadowBlur2_ps && D3DXCompileShaderFromResourceW(hm, MAKEINTRESOURCEW(IDR_DeferredShadowBlur), NULL, NULL, "BlurVertical", "ps_3_0", 0, &bf1, &bf2, &ppConstantTable) == S_OK) { if (CreatePixelShaderOriginal.unsafe_stdcall<HRESULT>(pDevice, (DWORD*)bf1->GetBufferPointer(), &DeferredShadowBlur2_ps) != S_OK || !DeferredShadowBlur2_ps) SAFE_RELEASE(DeferredShadowBlur2_ps); SAFE_RELEASE(bf1); SAFE_RELEASE(bf2); SAFE_RELEASE(ppConstantTable); }
        if (!DeferredShadowBlur3_ps && D3DXCompileShaderFromResourceW(hm, MAKEINTRESOURCEW(IDR_DeferredShadowBlur), NULL, NULL, "BlurOmini", "ps_3_0", 0, &bf1, &bf2, &ppConstantTable) == S_OK) { if (CreatePixelShaderOriginal.unsafe_stdcall<HRESULT>(pDevice, (DWORD*)bf1->GetBufferPointer(), &DeferredShadowBlur3_ps) != S_OK || !DeferredShadowBlur3_ps) SAFE_RELEASE(DeferredShadowBlur3_ps); SAFE_RELEASE(bf1); SAFE_RELEASE(bf2); SAFE_RELEASE(ppConstantTable); }

        if (!DOF_ps && D3DXCompileShaderFromResourceW(hm, MAKEINTRESOURCEW(IDR_DOF_ps), NULL, NULL, "main", "ps_3_0", 0, &bf1, &bf2, &ppConstantTable) == S_OK) { if (CreatePixelShaderOriginal.unsafe_stdcall<HRESULT>(pDevice, (DWORD*)bf1->GetBufferPointer(), &DOF_ps) != S_OK || !DOF_ps) SAFE_RELEASE(DOF_ps); SAFE_RELEASE(bf1); SAFE_RELEASE(bf2); SAFE_RELEASE(ppConstantTable); }
        if (!SMAA_EdgeDetection && D3DXCompileShaderFromResourceW(hm, MAKEINTRESOURCEW(IDR_SMAA), NULL, NULL, "SMAAColorEdgeDetectionPS", "ps_3_0", 0, &bf1, &bf2, &ppConstantTable) == S_OK) { if (CreatePixelShaderOriginal.unsafe_stdcall<HRESULT>(pDevice, (DWORD*)bf1->GetBufferPointer(), &SMAA_EdgeDetection) != S_OK || !SMAA_EdgeDetection) SAFE_RELEASE(SMAA_EdgeDetection); SAFE_RELEASE(bf1); SAFE_RELEASE(bf2); SAFE_RELEASE(ppConstantTable); }
        if (!SMAA_BlendingWeightsCalculation && D3DXCompileShaderFromResourceW(hm, MAKEINTRESOURCEW(IDR_SMAA), NULL, NULL, "SMAABlendingWeightCalculationPS", "ps_3_0", 0, &bf1, &bf2, &ppConstantTable) == S_OK) { if (CreatePixelShaderOriginal.unsafe_stdcall<HRESULT>(pDevice, (DWORD*)bf1->GetBufferPointer(), &SMAA_BlendingWeightsCalculation) != S_OK || !SMAA_BlendingWeightsCalculation) SAFE_RELEASE(SMAA_BlendingWeightsCalculation); SAFE_RELEASE(bf1); SAFE_RELEASE(bf2); SAFE_RELEASE(ppConstantTable); }
        if (!SMAA_NeighborhoodBlending && D3DXCompileShaderFromResourceW(hm, MAKEINTRESOURCEW(IDR_SMAA), NULL, NULL, "SMAANeighborhoodBlendingPS", "ps_3_0", 0, &bf1, &bf2, &ppConstantTable) == S_OK) { if (CreatePixelShaderOriginal.unsafe_stdcall<HRESULT>(pDevice, (DWORD*)bf1->GetBufferPointer(), &SMAA_NeighborhoodBlending) != S_OK || !SMAA_NeighborhoodBlending) SAFE_RELEASE(SMAA_NeighborhoodBlending); SAFE_RELEASE(bf1); SAFE_RELEASE(bf2); SAFE_RELEASE(ppConstantTable); }
        if (!SMAA_EdgeDetectionVS && D3DXCompileShaderFromResourceW(hm, MAKEINTRESOURCEW(IDR_SMAA), NULL, NULL, "SMAAEdgeDetectionVS", "vs_3_0", 0, &bf1, &bf2, &ppConstantTable) == S_OK) { if (CreateVertexShaderOriginal.unsafe_stdcall<HRESULT>(pDevice, (DWORD*)bf1->GetBufferPointer(), &SMAA_EdgeDetectionVS) != S_OK || !SMAA_EdgeDetectionVS) SAFE_RELEASE(SMAA_EdgeDetectionVS); SAFE_RELEASE(bf1); SAFE_RELEASE(bf2); SAFE_RELEASE(ppConstantTable); }
        if (!SMAA_BlendingWeightsCalculationVS && D3DXCompileShaderFromResourceW(hm, MAKEINTRESOURCEW(IDR_SMAA), NULL, NULL, "SMAABlendingWeightCalculationVS", "vs_3_0", 0, &bf1, &bf2, &ppConstantTable) == S_OK) { if (CreateVertexShaderOriginal.unsafe_stdcall<HRESULT>(pDevice, (DWORD*)bf1->GetBufferPointer(), &SMAA_BlendingWeightsCalculationVS) != S_OK || !SMAA_BlendingWeightsCalculationVS) SAFE_RELEASE(SMAA_BlendingWeightsCalculationVS); SAFE_RELEASE(bf1); SAFE_RELEASE(bf2); SAFE_RELEASE(ppConstantTable); }
        if (!SMAA_NeighborhoodBlendingVS && D3DXCompileShaderFromResourceW(hm, MAKEINTRESOURCEW(IDR_SMAA), NULL, NULL, "SMAANeighborhoodBlendingVS", "vs_3_0", 0, &bf1, &bf2, &ppConstantTable) == S_OK) { if (CreateVertexShaderOriginal.unsafe_stdcall<HRESULT>(pDevice, (DWORD*)bf1->GetBufferPointer(), &SMAA_NeighborhoodBlendingVS) != S_OK || !SMAA_NeighborhoodBlendingVS) SAFE_RELEASE(SMAA_NeighborhoodBlendingVS); SAFE_RELEASE(bf1); SAFE_RELEASE(bf2); SAFE_RELEASE(ppConstantTable); }
        return ShadersFinishedLoading();
    }

#define PostfxTextureCount 15
    IDirect3DBaseTexture9* prePostFx[PostfxTextureCount] = { 0 };
    DWORD Samplers[PostfxTextureCount] = { D3DTEXF_LINEAR };

    bool resourcesFinishedLoading() {
        return SMAA_EdgeDetection && SMAA_BlendingWeightsCalculation && SMAA_NeighborhoodBlending &&
            SMAA_EdgeDetectionVS && SMAA_BlendingWeightsCalculationVS && SMAA_NeighborhoodBlendingVS &&
            areaTex && searchTex && edgesTex && blendTex;
    }

    void swapbuffers() {
        auto temptex = renderTargetTex;
        renderTargetTex = textureRead;
        textureRead = temptex;

        auto tempsurf = renderTargetSurf;
        renderTargetSurf = surfaceRead;
        surfaceRead = tempsurf;
    };

    void ReleaseTextures() {
        NormalTex = nullptr;
        DiffuseTex = nullptr;
        SpecularTex = nullptr;
        StencilTex = nullptr;
        BloomTex = nullptr;
        DepthTex = nullptr;
        OldShadowAtlas = nullptr;
        OldShadowAtlasSurf = nullptr;

        pHalfHDRTex = nullptr;
        pQuarterHDRTex = nullptr;

        SAFE_RELEASE(pHDRTex2);
        SAFE_RELEASE(pHDRTex3);
        SAFE_RELEASE(areaTex);
        SAFE_RELEASE(searchTex);
        SAFE_RELEASE(edgesTex);
        SAFE_RELEASE(blendTex);
        SAFE_RELEASE(bluenoisevolume);

        SAFE_RELEASE(aoTex);
        SAFE_RELEASE(aoHalfTex);
        SAFE_RELEASE(halfDepthDsTex);
        SAFE_RELEASE(stencilDownsampled);
        SAFE_RELEASE(pHDRDownsampleTex);
        SAFE_RELEASE(pHDRDownsampleTex2);
        SAFE_RELEASE(depthStenciltex);

        SAFE_RELEASE(OldShadowAtlas);
        SAFE_RELEASE(OldShadowAtlasSurf);
        SAFE_RELEASE(NewShadowAtlas);
        SAFE_RELEASE(pShadowBlurTex1);
        SAFE_RELEASE(pShadowBlurTex2);
    }

    bool ShadersFinishedLoading() {
        if (FxaaPS && dof_blur_ps && dof_coc_ps && depth_of_field_ps && depth_of_field_tent_ps && depth_of_field_blur_ps
            && depth_of_field_coc_ps && stipple_filter_ps && DeferredShadowGen_ps && DeferredShadowUse1_ps
            && DeferredShadowUse2_ps && SSAO_ps && SSAO_ps2 && SSAO_vs && downsampler_ps && SunShafts_PS
            && SunShafts2_PS && SunShafts3_PS && SunShafts4_PS && SSDownsampler_PS && SSDownsampler2_PS
            && SSAdd_PS && DeferredShadowBlur1_ps && DeferredShadowBlur2_ps && DeferredShadowBlur3_ps
            && SMAA_EdgeDetection && SMAA_BlendingWeightsCalculation && SMAA_NeighborhoodBlending && DOF_ps
            && SMAA_EdgeDetectionVS && SMAA_BlendingWeightsCalculationVS && SMAA_NeighborhoodBlendingVS)
            return true;

        return false;
    }

    bool TexturesFinishedLoading() {
        if (areaTex && searchTex && edgesTex && blendTex)
            return true;
        return false;
    }

    void Readini()
    {
        CIniReader iniReader("");
        UseSunShafts = iniReader.ReadInteger("SUNSHAFTS", "SSType", 2);
        SunShaftsSamples[0] = iniReader.ReadInteger("SUNSHAFTS", "SSamples", 20);

        SunShafts_params1[0] = iniReader.ReadFloat("SUNSHAFTS", "SSWeight", 1.5f);
        SunShafts_params1[1] = iniReader.ReadFloat("SUNSHAFTS", "SSDensity", 0.95f);
        SunShafts_params1[2] = iniReader.ReadFloat("SUNSHAFTS", "SSExposure", 0.01f);
        SunShafts_params1[3] = iniReader.ReadFloat("SUNSHAFTS", "SSDecay", 0.975f);

        SunShafts_params2[0] = iniReader.ReadFloat("SUNSHAFTS", "SSSunSize", 0.5f);
        SunShafts_params2[1] = iniReader.ReadFloat("SUNSHAFTS", "SSPow", 1.f);
        SunShafts_params2[2] = iniReader.ReadFloat("SUNSHAFTS", "SSDepth", 2.f);
        SunShafts_params2[3] = iniReader.ReadFloat("SUNSHAFTS", "SSPower", 1.5f);

        // downscaled simple, downscaled two passes, full screen simple, fullscreen 2 passes
        useDepthOfField = iniReader.ReadInteger("SRF", "DepthOfFieldType", 3);
        UseSSAO = iniReader.ReadInteger("SRF", "UseSSAO", 0) != 0;
        useStippleFilter = iniReader.ReadInteger("SRF", "StippleFilter", 0) != 0;

        // 0 off, 1 horizontal, 2 vertical, 3 horizontal e vertical.
        useDefferedShadows = iniReader.ReadInteger("SRF", "ScreenSpaceShadowsBlur", 0);
        useNewShadowAtlas = iniReader.ReadInteger("SRF", "NewShadowAtlas", 0) != 0;

        DofSamples[0] = iniReader.ReadInteger("SRF", "DofSamples", 20);
        NoiseSale[0] = iniReader.ReadFloat("SRF", "NoiseSale", 1.f / 256.f);
        BilateralDepthTreshold[0] = iniReader.ReadFloat("SRF", "BilateralDepthTreshold", 0.003f);

        fixDistantOutlineUsingDXVK = iniReader.ReadInteger("SRF", "fixDXVKOutlines", 0) != 0;
    }
};

PostFxResource PostFxResources;

class PostFX {
public:
    PostFX() {
        FusionFix::onInitEventAsync() += []()
        {
            if (!GetD3DX9_43DLL())
                return;

            PostFxResources.Readini();

            // none, fxaa, smaa, blend, edge
            UsePostFxAA = FusionFixSettings.GetRef("PREF_ANTIALIASING");
            useSSAA = false;

            static HMODULE hm = NULL;
            GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCWSTR)&FusionFix::onInitEventAsync, &hm);

            FusionFix::D3D9::onAfterCreateVertexShader() += [](LPDIRECT3DDEVICE9& pDevice, DWORD*& pFunction, IDirect3DVertexShader9**& ppShader)
            {
                int id = GetFusionShaderID(*ppShader);
                if ((*ppShader) && id >= 0)
                    PostFxResources.ShaderListVS[*ppShader] = id;
            };

            FusionFix::D3D9::onAfterCreatePixelShader() += [](LPDIRECT3DDEVICE9& pDevice, DWORD*& pFunction, IDirect3DPixelShader9**& ppShader)
            {
                int id = GetFusionShaderID(*ppShader);
                if ((*ppShader) && id >= 0)
                    PostFxResources.ShaderListPS[*ppShader] = id;
            };

            if (useSSAA)
            {
                FusionFix::D3D9::onBeforeCreateTexture() += [](LPDIRECT3DDEVICE9& pDevice, UINT& Width, UINT& Height, UINT& Levels, DWORD& Usage, D3DFORMAT& Format, D3DPOOL& Pool, IDirect3DTexture9**& ppTexture, HANDLE*& pSharedHandle)
                {
                    auto gWindowWidth = (gRect.right - gRect.left);
                    auto gWindowHeight = (gRect.bottom - gRect.top);

                    if (useSSAA && Width == getWindowWidth() && Height == getWindowHeight())
                    {
                        Width *= 2;
                        Height *= 2;
                    }
                };
            }

            FusionFix::onBeforeReset() += []()
            {
                PostFxResources.ReleaseTextures();
                SAFE_RELEASE(PostFxResources.pHDRTex2);
                SAFE_RELEASE(PostFxResources.areaTex);
                SAFE_RELEASE(PostFxResources.searchTex);
                SAFE_RELEASE(PostFxResources.edgesTex);
                SAFE_RELEASE(PostFxResources.blendTex);
            };

            FusionFix::D3D9::onBeforeCreatePixelShader() += [](LPDIRECT3DDEVICE9& pDevice, DWORD*& pFunction, IDirect3DPixelShader9**& ppShader)
            {
                ID3DXBuffer* bf1 = nullptr;
                ID3DXBuffer* bf2 = nullptr;
                ID3DXConstantTable* ppConstantTable = nullptr;
                if (!PostFxResources.shadersLoaded)
                    PostFxResources.shadersLoaded = PostFxResources.loadShaders(pDevice, hm);
            };

            FusionFix::D3D9::onAfterCreateTexture() += [](LPDIRECT3DDEVICE9& pDevice, UINT& Width, UINT& Height, UINT& Levels, DWORD& Usage, D3DFORMAT& Format, D3DPOOL& Pool, IDirect3DTexture9**& ppTexture, HANDLE*& pSharedHandle)
            {
                if (*ppTexture != nullptr) {
                    if (*ppTexture != nullptr && (Format == D3DFMT_R16F || Format == D3DFMT_R32F) && Height >= 128 && Width == Height * 4 && Levels == 1) {
                        PostFxResources.OldShadowAtlas = *ppTexture;
                        SAFE_RELEASE(PostFxResources.NewShadowAtlas);
                        CreateTextureOriginal.unsafe_stdcall<HRESULT>(pDevice, Width, Height, Levels, D3DUSAGE_DEPTHSTENCIL, D3DFMT_D24X8, D3DPOOL_DEFAULT, &PostFxResources.NewShadowAtlas, 0);
                    }
                    if (Format == D3DFMT_A16B16G16R16F && Width == getWindowWidth() / 2 && Height == getWindowHeight() / 2) {
                        PostFxResources.pHalfHDRTex = (*ppTexture);
                    }
                }
                if (Format == D3DFMT_A16B16G16R16F && ppTexture && (getWindowWidth() * (useSSAA ? 2 : 1)) && Height == (getWindowHeight() * (useSSAA ? 2 : 1))) {
                    // Release before, just to be sure
                    SAFE_RELEASE(PostFxResources.pHDRTex2);
                    SAFE_RELEASE(PostFxResources.pHDRTex3);
                    SAFE_RELEASE(PostFxResources.edgesTex);
                    SAFE_RELEASE(PostFxResources.blendTex);
                    SAFE_RELEASE(PostFxResources.areaTex);
                    SAFE_RELEASE(PostFxResources.searchTex);
                    SAFE_RELEASE(PostFxResources.bluenoisevolume);

                    SAFE_RELEASE(PostFxResources.aoTex);
                    SAFE_RELEASE(PostFxResources.aoHalfTex);
                    SAFE_RELEASE(PostFxResources.halfDepthDsTex);
                    SAFE_RELEASE(PostFxResources.stencilDownsampled);
                    SAFE_RELEASE(PostFxResources.pHDRDownsampleTex);
                    SAFE_RELEASE(PostFxResources.pHDRDownsampleTex2);
                    SAFE_RELEASE(PostFxResources.depthStenciltex);
                    SAFE_RELEASE(PostFxResources.pShadowBlurTex1);
                    SAFE_RELEASE(PostFxResources.pShadowBlurTex2);

                    PostFxResources.pHDRTex = (*ppTexture);

                    // create new texture to postfx
                    if (SUCCEEDED(CreateTextureOriginal.unsafe_stdcall<HRESULT>(pDevice, Width, Height, Levels, Usage, Format, Pool, &PostFxResources.pHDRTex2, 0)) && PostFxResources.pHDRTex2) {
                        //if(SUCCEEDED(CreateTextureOriginal.unsafe_stdcall<HRESULT>(pDevice, Width, Height, Levels, Usage, Format, Pool, &PostFxResources.pHDRTex3, 0)) && PostFxResources.pHDRTex3) // Cause stuttering? Why? Low gpu memory? High bandwidth usage?
                        if (D3DXCreateTextureFromResourceExW(pDevice, hm, MAKEINTRESOURCEW(IDR_AreaTex), 0, 0, 0, 0, D3DFMT_UNKNOWN, D3DPOOL_MANAGED, D3DX_FILTER_LINEAR, D3DX_FILTER_LINEAR, D3DCOLOR_ARGB(150, 100, 100, 100), NULL, NULL, &PostFxResources.areaTex) == S_OK)
                            if (D3DXCreateTextureFromResourceExW(pDevice, hm, MAKEINTRESOURCEW(IDR_SearchTex), 0, 0, 0, 0, D3DFMT_UNKNOWN, D3DPOOL_MANAGED, D3DX_FILTER_LINEAR, D3DX_FILTER_LINEAR, D3DCOLOR_ARGB(150, 100, 100, 100), NULL, NULL, &PostFxResources.searchTex) == S_OK)
                                if (D3DXCreateVolumeTextureFromResourceExW(pDevice, hm, MAKEINTRESOURCEW(IDR_bluenoisevolume), 0, 0, 0, 0, 0, D3DFMT_UNKNOWN, D3DPOOL_MANAGED, D3DX_FILTER_LINEAR, D3DX_FILTER_LINEAR, D3DCOLOR_ARGB(150, 100, 100, 100), NULL, NULL, &PostFxResources.bluenoisevolume) == S_OK) {
                                    HRESULT hr = CreateTextureOriginal.unsafe_stdcall<HRESULT>(pDevice, Width, Height, Levels, Usage, D3DFMT_X8R8G8B8, Pool, &PostFxResources.edgesTex, 0);
                                    hr += CreateTextureOriginal.unsafe_stdcall<HRESULT>(pDevice, Width, Height, Levels, Usage, D3DFMT_A8R8G8B8, Pool, &PostFxResources.blendTex, 0);
                                    hr += CreateTextureOriginal.unsafe_stdcall<HRESULT>(pDevice, Width / 2, Height / 2, Levels, Usage, D3DFMT_X8R8G8B8, Pool, &PostFxResources.aoHalfTex, 0);
                                    hr += CreateTextureOriginal.unsafe_stdcall<HRESULT>(pDevice, Width, Height, Levels, Usage, D3DFMT_X8R8G8B8, Pool, &PostFxResources.aoTex, 0);
                                    hr += CreateTextureOriginal.unsafe_stdcall<HRESULT>(pDevice, Width / 2, Height / 2, Levels, Usage, D3DFMT_R16F, Pool, &PostFxResources.halfDepthDsTex, 0);
                                    hr += CreateTextureOriginal.unsafe_stdcall<HRESULT>(pDevice, Width, Height, Levels, Usage, D3DFMT_R16F, Pool, &PostFxResources.stencilDownsampled, 0);
                                    hr += CreateTextureOriginal.unsafe_stdcall<HRESULT>(pDevice, Width / 2, Height / 2, Levels /*1*/, Usage, D3DFMT_A32B32G32R32F, Pool, &PostFxResources.pHDRDownsampleTex, 0);
                                    hr += CreateTextureOriginal.unsafe_stdcall<HRESULT>(pDevice, Width / 2, Height / 2, Levels /*1*/, Usage, D3DFMT_A32B32G32R32F, Pool, &PostFxResources.pHDRDownsampleTex2, 0);
                                    hr += CreateTextureOriginal.unsafe_stdcall<HRESULT>(pDevice, Width, Height, Levels, Usage, D3DFMT_G16R16F, Pool, &PostFxResources.pShadowBlurTex1, 0);
                                    hr += CreateTextureOriginal.unsafe_stdcall<HRESULT>(pDevice, Width, Height, Levels, Usage, D3DFMT_G16R16F, Pool, &PostFxResources.pShadowBlurTex2, 0);
                                    hr += CreateTextureOriginal.unsafe_stdcall<HRESULT>(pDevice, Width / 2, Height / 2, Levels /*1*/, 0, D3DFMT_D24S8, Pool, &PostFxResources.depthStenciltex, 0);
                                }
                    }
                }
            };

            FusionFix::onBeforePostFX() += []()
            {
                doPostFx = true;
            };

            FusionFix::D3D9::onBeforeDrawPrimitive() += [](LPDIRECT3DDEVICE9& pDevice, D3DPRIMITIVETYPE& PrimitiveType, UINT& StartVertex, UINT& PrimitiveCount)
            {
                IDirect3DPixelShader9* pShader = nullptr;
                IDirect3DVertexShader9* vShader = nullptr;
                HRESULT hr = S_FALSE;
                pDevice->GetPixelShader(&pShader);
                pDevice->GetVertexShader(&vShader);
                int id = 0;
                if (pShader)
                    id = PostFxResources.ShaderListPS[pShader];

                //if(doPostFx) {
                if (id == 815 || id == 817 || id == 819 || id == 821 || id == 827 || id == 829 || id == 831) {
                    hr = PostFx3(pDevice, PrimitiveType, StartVertex, PrimitiveCount, pShader, vShader);
                    FusionFix::D3D9::setInsteadDrawPrimitive(true);
                }

                // atmoscat clouds
                else if ((id == 65 || id == 66) && PostFxResources.DiffuseTex != nullptr) {
                    IDirect3DSurface9* DiffuseSurf = nullptr;
                    PostFxResources.DiffuseTex->GetSurfaceLevel(0, &DiffuseSurf);
                    if (DiffuseSurf) {
                        IDirect3DSurface9* oldRenderTarget1 = 0;

                        pDevice->GetRenderTarget(1, &oldRenderTarget1);
                        pDevice->SetRenderTarget(1, DiffuseSurf);
                        if (id == 65)
                            pDevice->SetPixelShader(PostFxResources.SSDiffuseCloudsGen_PS);
                        hr = DrawPrimitiveOriginal.unsafe_stdcall<HRESULT>(pDevice, PrimitiveType, StartVertex, PrimitiveCount);

                        pDevice->SetPixelShader(pShader);
                        pDevice->SetRenderTarget(1, oldRenderTarget1);

                        SAFE_RELEASE(oldRenderTarget1);
                        SAFE_RELEASE(DiffuseSurf);
                        FusionFix::D3D9::setInsteadDrawPrimitive(true);
                        return hr;
                    }
                    SAFE_RELEASE(DiffuseSurf);
                }
                else if (PostFxResources.useNewShadowAtlas == true && id == 1 && PostFxResources.NewShadowAtlas) // cascade convert
                {
                    IDirect3DSurface9* oldDepth = nullptr;
                    IDirect3DSurface9* newDepth = nullptr;
                    PostFxResources.NewShadowAtlas->GetSurfaceLevel(0, &newDepth);
                    if (newDepth) {
                        D3DVIEWPORT9 newViewport = { 0 };
                        D3DVIEWPORT9 lastViewport = { 0 };
                        pDevice->GetViewport(&newViewport);
                        pDevice->GetViewport(&lastViewport);
                        pDevice->GetDepthStencilSurface(&oldDepth);
                        pDevice->SetDepthStencilSurface(newDepth);
                        DWORD last_ZENABLE = 0;
                        DWORD last_ZWRITEENABLE = 0;
                        DWORD last_ZFUNC = 0;
                        pDevice->GetRenderState(D3DRS_ZENABLE, &last_ZENABLE);
                        pDevice->GetRenderState(D3DRS_ZWRITEENABLE, &last_ZWRITEENABLE);
                        pDevice->GetRenderState(D3DRS_ZFUNC, &last_ZFUNC);

                        pDevice->SetRenderState(D3DRS_ZENABLE, D3DZB_TRUE);
                        pDevice->SetRenderState(D3DRS_ZWRITEENABLE, D3DZB_TRUE);
                        pDevice->SetRenderState(D3DRS_ZFUNC, D3DCMP_ALWAYS);
                        newViewport.MaxZ = 1000;
                        newViewport.MinZ = -1000;
                        pDevice->SetViewport(&newViewport);
                        pDevice->SetPixelShader(PostFxResources.CascadeAtlasGen);
                        hr = DrawPrimitiveOriginal.unsafe_stdcall<HRESULT>(pDevice, PrimitiveType, StartVertex, PrimitiveCount);
                        pDevice->SetPixelShader(pShader);
                        pDevice->SetViewport(&lastViewport);
                        pDevice->SetRenderState(D3DRS_ZENABLE, last_ZENABLE);
                        pDevice->SetRenderState(D3DRS_ZWRITEENABLE, last_ZWRITEENABLE);
                        pDevice->SetRenderState(D3DRS_ZFUNC, last_ZFUNC);
                        pDevice->SetDepthStencilSurface(oldDepth);
                        SAFE_RELEASE(newDepth);
                        SAFE_RELEASE(oldDepth);
                        FusionFix::D3D9::setInsteadDrawPrimitive(true);
                        return hr;
                    }
                    SAFE_RELEASE(newDepth);
                    SAFE_RELEASE(oldDepth);
                }
                else if (PostFxResources.fixDistantOutlineUsingDXVK && (id == 822 || id == 823)) // fog outline
                {
                    DWORD oldSampler = 0;
                    pDevice->GetSamplerState(0, D3DSAMP_MAGFILTER, &oldSampler);
                    pDevice->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_POINT);
                    hr = DrawPrimitiveOriginal.unsafe_stdcall<HRESULT>(pDevice, PrimitiveType, StartVertex, PrimitiveCount);
                    pDevice->SetSamplerState(0, D3DSAMP_MAGFILTER, oldSampler);
                    return hr;
                }
                else if (id == 6 || id == 7) // deferred 1 e 2 // breaks lights atm
                {
                    IDirect3DSurface9* pShadowBlurSurf1 = nullptr;
                    IDirect3DSurface9* pShadowBlurSurf2 = nullptr;
                    IDirect3DSurface9* OldRenderTarget = nullptr;
                
                    if (PostFxResources.useDefferedShadows > 0 && PostFxResources.pShadowBlurTex1 && PostFxResources.pShadowBlurTex2) {
                        PostFxResources.pShadowBlurTex1->GetSurfaceLevel(0, &pShadowBlurSurf1);
                        PostFxResources.pShadowBlurTex2->GetSurfaceLevel(0, &pShadowBlurSurf2);
                        pDevice->GetRenderTarget(0, &OldRenderTarget);
                        if (pShadowBlurSurf1 && pShadowBlurSurf2 && OldRenderTarget) {
                            pDevice->SetRenderTarget(0, pShadowBlurSurf1);
                            pDevice->SetPixelShader(PostFxResources.DeferredShadowGen_ps);
                            pDevice->SetPixelShaderConstantF(140, PostFxResources.BilateralDepthTreshold, 1);
                            DWORD old_SAMP_MAGFILTER = 0;
                            pDevice->GetSamplerState(14, D3DSAMP_MAGFILTER, &old_SAMP_MAGFILTER);
                            pDevice->SetSamplerState(14, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
                
                            if (PostFxResources.useNewShadowAtlas == true && PostFxResources.NewShadowAtlas) {
                                pDevice->SetTexture(15, PostFxResources.NewShadowAtlas);
                                //To enable Fetch - 4 on a texture sampler(sampler 0 in this example) :
                                #define FETCH4_ENABLE ((DWORD)MAKEFOURCC('G', 'E', 'T', '4'))
                                #define FETCH4_DISABLE ((DWORD)MAKEFOURCC('G', 'E', 'T', '1'))
                                DWORD old_SAMP_MIPMAPLODBIAS = 0;
                                DWORD old_SAMP_MAGFILTER = 0;
                                DWORD old_SAMP_MINFILTER = 0;
                
                                // Enable Fetch-4 on sampler 0 by overloading the MIPMAPLODBIAS render state
                                pDevice->GetSamplerState(15, D3DSAMP_MIPMAPLODBIAS, &old_SAMP_MIPMAPLODBIAS);
                                pDevice->GetSamplerState(15, D3DSAMP_MAGFILTER, &old_SAMP_MAGFILTER);
                                pDevice->GetSamplerState(15, D3DSAMP_MINFILTER, &old_SAMP_MINFILTER);
                
                                pDevice->SetSamplerState(15, D3DSAMP_MIPMAPLODBIAS, FETCH4_ENABLE);
                                // Set point sampling filtering (required for Fetch-4 to work)
                                pDevice->SetSamplerState(15, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
                                pDevice->SetSamplerState(15, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
                                pDevice->SetSamplerState(15, D3DSAMP_MIPFILTER, D3DTEXF_POINT);
                                hr = DrawPrimitiveOriginal.unsafe_stdcall<HRESULT>(pDevice, PrimitiveType, StartVertex, PrimitiveCount);
                
                                pDevice->SetSamplerState(15, D3DSAMP_MIPMAPLODBIAS, old_SAMP_MIPMAPLODBIAS);
                                pDevice->SetSamplerState(15, D3DSAMP_MAGFILTER, old_SAMP_MAGFILTER);
                                pDevice->SetSamplerState(15, D3DSAMP_MINFILTER, old_SAMP_MINFILTER);
                
                                pDevice->SetTexture(15, PostFxResources.OldShadowAtlas);
                            }
                            else
                                DrawPrimitiveOriginal.unsafe_stdcall<HRESULT>(pDevice, PrimitiveType, StartVertex, PrimitiveCount);
                
                            if (PostFxResources.useDefferedShadows == 1) {
                                pDevice->SetRenderTarget(0, pShadowBlurSurf2);
                                pDevice->SetTexture(14, PostFxResources.pShadowBlurTex1);
                                pDevice->SetPixelShader(PostFxResources.DeferredShadowBlur1_ps);
                                DrawPrimitiveOriginal.unsafe_stdcall<HRESULT>(pDevice, PrimitiveType, StartVertex, PrimitiveCount);
                            }
                            if (PostFxResources.useDefferedShadows == 2) {
                                pDevice->SetRenderTarget(0, pShadowBlurSurf2);
                                pDevice->SetTexture(14, PostFxResources.pShadowBlurTex1);
                                pDevice->SetPixelShader(PostFxResources.DeferredShadowBlur2_ps);
                                DrawPrimitiveOriginal.unsafe_stdcall<HRESULT>(pDevice, PrimitiveType, StartVertex, PrimitiveCount);
                            }
                            if (PostFxResources.useDefferedShadows == 3) {
                                pDevice->SetPixelShader(PostFxResources.DeferredShadowBlur1_ps);
                                pDevice->SetTexture(14, PostFxResources.pShadowBlurTex1);
                                pDevice->SetRenderTarget(0, pShadowBlurSurf2);
                                DrawPrimitiveOriginal.unsafe_stdcall<HRESULT>(pDevice, PrimitiveType, StartVertex, PrimitiveCount);
                
                                pDevice->SetPixelShader(PostFxResources.DeferredShadowBlur2_ps);
                                pDevice->SetTexture(14, PostFxResources.pShadowBlurTex2);
                                pDevice->SetRenderTarget(0, pShadowBlurSurf1);
                                DrawPrimitiveOriginal.unsafe_stdcall<HRESULT>(pDevice, PrimitiveType, StartVertex, PrimitiveCount);
                            }
                            if (PostFxResources.useDefferedShadows == 4) {
                                pDevice->SetRenderTarget(0, pShadowBlurSurf2);
                                pDevice->SetTexture(14, PostFxResources.pShadowBlurTex1);
                                pDevice->SetPixelShader(PostFxResources.DeferredShadowBlur3_ps);
                                DrawPrimitiveOriginal.unsafe_stdcall<HRESULT>(pDevice, PrimitiveType, StartVertex, PrimitiveCount);
                            }
                
                            pDevice->SetRenderTarget(0, OldRenderTarget);
                            if (PostFxResources.useDefferedShadows == 3)
                                pDevice->SetTexture(14, PostFxResources.pShadowBlurTex1);
                
                            else
                                pDevice->SetTexture(14, PostFxResources.pShadowBlurTex2);
                
                            if (id == 6)
                                pDevice->SetPixelShader(PostFxResources.DeferredShadowUse1_ps);
                            else
                                pDevice->SetPixelShader(PostFxResources.DeferredShadowUse2_ps);
                            DrawPrimitiveOriginal.unsafe_stdcall<HRESULT>(pDevice, PrimitiveType, StartVertex, PrimitiveCount);
                            pDevice->SetTexture(14, 0);
                            pDevice->SetPixelShader(pShader);
                            pDevice->SetSamplerState(14, D3DSAMP_MAGFILTER, old_SAMP_MAGFILTER);
                            SAFE_RELEASE(pShadowBlurSurf1);
                            SAFE_RELEASE(pShadowBlurSurf2);
                            SAFE_RELEASE(OldRenderTarget);
                            FusionFix::D3D9::setInsteadDrawPrimitive(true);
                            return hr;
                        }
                        SAFE_RELEASE(pShadowBlurSurf1);
                        SAFE_RELEASE(pShadowBlurSurf2);
                        SAFE_RELEASE(OldRenderTarget);
                        //SetPixelShader(pShader);
                        DrawPrimitiveOriginal.unsafe_stdcall<HRESULT>(pDevice, PrimitiveType, StartVertex, PrimitiveCount);
                        FusionFix::D3D9::setInsteadDrawPrimitive(true);
                        return hr;
                    }
                
                    if (PostFxResources.useNewShadowAtlas == true && PostFxResources.NewShadowAtlas) {
                        pDevice->SetTexture(15, PostFxResources.NewShadowAtlas);
                        //To enable Fetch - 4 on a texture sampler(sampler 0 in this example) :
                        #define FETCH4_ENABLE ((DWORD)MAKEFOURCC('G', 'E', 'T', '4'))
                        #define FETCH4_DISABLE ((DWORD)MAKEFOURCC('G', 'E', 'T', '1'))
                        DWORD old_SAMP_MIPMAPLODBIAS = 0;
                        DWORD old_SAMP_MAGFILTER = 0;
                        DWORD old_SAMP_MINFILTER = 0;
                
                        // Enable Fetch-4 on sampler 0 by overloading the MIPMAPLODBIAS render state
                        pDevice->GetSamplerState(15, D3DSAMP_MIPMAPLODBIAS, &old_SAMP_MIPMAPLODBIAS);
                        pDevice->GetSamplerState(15, D3DSAMP_MAGFILTER, &old_SAMP_MAGFILTER);
                        pDevice->GetSamplerState(15, D3DSAMP_MINFILTER, &old_SAMP_MINFILTER);
                
                        //pDevice->SetSamplerState(15, D3DSAMP_MIPMAPLODBIAS, FETCH4_ENABLE);
                        // Set point sampling filtering (required for Fetch-4 to work)
                        pDevice->SetSamplerState(15, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
                        pDevice->SetSamplerState(15, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
                        pDevice->SetSamplerState(15, D3DSAMP_MIPFILTER, D3DTEXF_POINT);
                        hr = DrawPrimitiveOriginal.unsafe_stdcall<HRESULT>(pDevice, PrimitiveType, StartVertex, PrimitiveCount);
                
                        pDevice->SetSamplerState(15, D3DSAMP_MIPMAPLODBIAS, old_SAMP_MIPMAPLODBIAS);
                        pDevice->SetSamplerState(15, D3DSAMP_MAGFILTER, old_SAMP_MAGFILTER);
                        pDevice->SetSamplerState(15, D3DSAMP_MINFILTER, old_SAMP_MINFILTER);
                
                        pDevice->SetTexture(15, PostFxResources.OldShadowAtlas);
                        FusionFix::D3D9::setInsteadDrawPrimitive(true);
                        return hr;
                    }
                
                    // use new deferred
                    pDevice->SetPixelShader(pShader);
                    DrawPrimitiveOriginal.unsafe_stdcall<HRESULT>(pDevice, PrimitiveType, StartVertex, PrimitiveCount);
                    return hr;
                }

                return hr;
            };

            if (useSSAA)
            {
                FusionFix::D3D9::onSetPixelShaderConstantF() += [](LPDIRECT3DDEVICE9& pDevice, UINT& StartRegister, float*& pConstantData, UINT& Vector4fCount)
                {
                    if (useSSAA && pConstantData[0] == 1.f / getWindowWidth() && pConstantData[1] == 1.f / getWindowHeight() && Vector4fCount == 1) {
                        pConstantData[0] *= 0.5f;
                        pConstantData[1] *= 0.5f;
                        pConstantData[2] *= 2.0f;
                        pConstantData[3] *= 2.0f;
                    }
                    else if (useSSAA && pConstantData[0] == getWindowWidth() && pConstantData[1] == getWindowHeight() && Vector4fCount == 1) {
                        pConstantData[0] *= 2.0f;
                        pConstantData[1] *= 2.0f;
                        pConstantData[2] *= 0.5f;
                        pConstantData[3] *= 0.5f;
                    }
                };
            }

            FusionFix::D3D9::onSetTexture() += [](LPDIRECT3DDEVICE9& pDevice, DWORD& Stage, IDirect3DBaseTexture9*& pTexture)
            {
                IDirect3DPixelShader9* pShader = nullptr;
                pDevice->GetPixelShader(&pShader);
                if (pShader)
                {
                    int id = PostFxResources.ShaderListPS[pShader];
                    // diffuse 0
                    if ((Stage == 0 && id == 5 + 1) ||
                        (Stage == 0 && id == 5 + 2) ||
                        (Stage == 0 && id == 5 + 3) ||
                        (Stage == 0 && id == 5 + 4) ||
                        (Stage == 0 && id == 5 + 6) ||
                        (Stage == 0 && id == 5 + 7) ||
                        (Stage == 0 && id == 5 + 8) ||
                        (Stage == 0 && id == 5 + 9) ||
                        (Stage == 0 && id == 5 + 10) ||
                        (Stage == 0 && id == 5 + 11) ||
                        (Stage == 0 && id == 5 + 12) ||
                        (Stage == 0 && id == 5 + 14) ||
                        (Stage == 0 && id == 5 + 15) ||
                        (Stage == 1 && id == 5 + 16) ||
                        (Stage == 1 && id == 5 + 17) ||
                        (Stage == 0 && id == 5 + 31)) {
                        PostFxResources.DiffuseTex = static_cast<IDirect3DTexture9*>(pTexture);
                    }
                    // normal 1
                    if ((Stage == 1 && id == 5 + 1) ||
                        (Stage == 1 && id == 5 + 2) ||
                        (Stage == 1 && id == 5 + 3) ||
                        (Stage == 1 && id == 5 + 4) ||
                        (Stage == 1 && id == 5 + 6) ||
                        (Stage == 1 && id == 5 + 7) ||
                        (Stage == 1 && id == 5 + 8) ||
                        (Stage == 1 && id == 5 + 9) ||
                        (Stage == 1 && id == 5 + 10) ||
                        (Stage == 1 && id == 5 + 11) ||
                        (Stage == 1 && id == 5 + 12) ||
                        (Stage == 1 && id == 5 + 14) ||
                        (Stage == 1 && id == 5 + 15) ||
                        (Stage == 2 && id == 5 + 16) ||
                        (Stage == 2 && id == 5 + 17) ||
                        (Stage == 1 && id == 5 + 29) ||
                        (Stage == 0 && id == 5 + 30) ||
                        (Stage == 0 && id == 5 + 32)) {
                        PostFxResources.NormalTex = static_cast<IDirect3DTexture9*>(pTexture);
                    }
                    // specular 2
                    if ((Stage == 2 && id == 5 + 1) ||
                        (Stage == 2 && id == 5 + 2) ||
                        (Stage == 2 && id == 5 + 3) ||
                        (Stage == 2 && id == 5 + 4) ||
                        (Stage == 2 && id == 5 + 6) ||
                        (Stage == 2 && id == 5 + 7) ||
                        (Stage == 2 && id == 5 + 8) ||
                        (Stage == 2 && id == 5 + 9) ||
                        (Stage == 2 && id == 5 + 14) ||
                        (Stage == 4 && id == 5 + 16) ||
                        (Stage == 4 && id == 5 + 17) ||
                        (Stage == 0 && id == 5 + 33)) {
                        PostFxResources.SpecularTex = static_cast<IDirect3DTexture9*>(pTexture);
                    }
                    // depth 3
                    if ((Stage == 4 && id == 5 + 1) ||
                        (Stage == 4 && id == 5 + 2) ||
                        (Stage == 4 && id == 5 + 3) ||
                        (Stage == 4 && id == 5 + 4) ||
                        (Stage == 4 && id == 5 + 6) ||
                        (Stage == 4 && id == 5 + 7) ||
                        (Stage == 4 && id == 5 + 8) ||
                        (Stage == 4 && id == 5 + 9) ||
                        (Stage == 2 && id == 5 + 10) ||
                        (Stage == 2 && id == 5 + 11) ||
                        (Stage == 2 && id == 5 + 12) ||
                        (Stage == 4 && id == 5 + 14) ||
                        (Stage == 2 && id == 5 + 15) ||
                        (Stage == 5 && id == 5 + 16) ||
                        (Stage == 5 && id == 5 + 17) ||
                        (Stage == 1 && id == 5 + 18) ||
                        (Stage == 1 && id == 5 + 19) ||
                        (Stage == 0 && id == 5 + 20) ||
                        (Stage == 0 && id == 5 + 21) ||
                        (Stage == 0 && id == 5 + 22) ||
                        (Stage == 0 && id == 5 + 23) ||
                        (Stage == 1 && id == 5 + 28) ||
                        (Stage == 2 && id == 5 + 29) ||
                        (Stage == 1 && id == 5 + 30) ||
                        (Stage == 1 && id == 5 + 35) ||
                        (Stage == 0 && id == 5 + 36) ||
                        (Stage == 0 && id == 5 + 11)) {
                        PostFxResources.DepthTex = static_cast<IDirect3DTexture9*>(pTexture);
                    }
                    // stencil
                    if ((Stage == 5 && id == 5 + 2) ||
                        (Stage == 5 && id == 5 + 4) ||
                        (Stage == 5 && id == 5 + 7) ||
                        (Stage == 5 && id == 5 + 8) ||
                        (Stage == 5 && id == 5 + 9) ||
                        (Stage == 4 && id == 5 + 11) ||
                        (Stage == 4 && id == 5 + 12) ||
                        (Stage == 5 && id == 5 + 14) ||
                        (Stage == 2 && id == 5 + 28)) {
                        PostFxResources.StencilTex = static_cast<IDirect3DTexture9*>(pTexture);
                    }
                }
            };
        };
    }

    static HRESULT PostFx3(LPDIRECT3DDEVICE9 pDevice, D3DPRIMITIVETYPE PrimitiveType, UINT StartVertex,
        UINT PrimitiveCount, IDirect3DPixelShader9* pShader, IDirect3DVertexShader9* vShader) {
        IDirect3DSurface9* pHDRSurface = nullptr;
        IDirect3DSurface9* pHDRSurface2 = nullptr;
        IDirect3DSurface9* pHDRSurface3 = nullptr;
        IDirect3DSurface9* pHDRDownsampleSurf = nullptr;
        IDirect3DSurface9* pHDRDownsampleSurf2 = nullptr;
        IDirect3DSurface9* backBuffer = nullptr;
        IDirect3DSurface9* edgesSurf = nullptr;
        IDirect3DSurface9* blendSurf = nullptr;
        IDirect3DSurface9* stencilDownsampledSurf = nullptr;
        IDirect3DSurface9* ppZStencilSurface = nullptr;
        IDirect3DSurface9* ppZStencilSurface2 = nullptr;
        IDirect3DSurface9* aoSurface = nullptr;

        D3DVOLUME_DESC volumeDescription;

        auto currGrcViewport = rage::GetCurrentViewport();
        auto SunColor = rage::grmShaderInfo::getParam("gta_atmoscatt_clouds.fxc", "SunColor");
        auto SunDirection = rage::grmShaderInfo::getParam("gta_atmoscatt_clouds.fxc", "SunDirection");
        auto gShadowMatrix = rage::grmShaderInfo::getGlobalParam("gShadowMatrix");

        HRESULT hr = S_FALSE;

        DWORD OldSRGB = 0;
        DWORD OldSampler = 0;

        static float vec4[4] = { 0.f };
        static float blueTimerVec4[4] = { 0.f };
        static int blueTimer = 0;

        // SetPixelShaderConstantI only set the first value, so I need 4 int4 to set 1 int4
        int SunShaftsSamplesa[4] = { PostFxResources.SunShaftsSamples[0], 0, 1, 0 };
        int SunShaftsSamplesb[4] = { PostFxResources.SunShaftsSamples[1], 0, 1, 0 };
        int SunShaftsSamplesc[4] = { PostFxResources.SunShaftsSamples[2], 0, 1, 0 };
        int SunShaftsSamplesd[4] = { PostFxResources.SunShaftsSamples[3], 0, 1, 0 };

        static float SunShaftsSamples2[4] = { 0.f };
        SunShaftsSamples2[0] = static_cast<float>(PostFxResources.SunShaftsSamples[0]);
        SunShaftsSamples2[1] = static_cast<float>(PostFxResources.SunShaftsSamples[1]);
        SunShaftsSamples2[2] = static_cast<float>(PostFxResources.SunShaftsSamples[2]);
        SunShaftsSamples2[3] = static_cast<float>(PostFxResources.SunShaftsSamples[3]);

        // save render state
        for (int i = 0; i < PostfxTextureCount; i++) {
            pDevice->GetTexture(i, &PostFxResources.prePostFx[i]);
            pDevice->GetSamplerState(i, D3DSAMP_MAGFILTER, &PostFxResources.Samplers[i]);
        }

        if (PostFxResources.bluenoisevolume) {
            PostFxResources.bluenoisevolume->GetLevelDesc(0, &volumeDescription);

            blueTimer++;
            blueTimer = blueTimer % (volumeDescription.Depth + 1);
            blueTimerVec4[0] = getWindowWidth() / float(volumeDescription.Width);
            blueTimerVec4[1] = getWindowHeight() / float(volumeDescription.Height);
            blueTimerVec4[2] = static_cast<float>(blueTimer) / float(volumeDescription.Depth);
            blueTimerVec4[3] = static_cast<float>(blueTimer);
        }

        pDevice->SetPixelShaderConstantF(218, blueTimerVec4, 1);
        pDevice->SetPixelShaderConstantF(144, PostFxResources.NoiseSale, 1);
        pDevice->SetTexture(8, PostFxResources.pHalfHDRTex);
        if (PostFxResources.bluenoisevolume)
            pDevice->SetTexture(9, PostFxResources.bluenoisevolume);

        if (useSSAA) {
            pDevice->GetSamplerState(0, D3DSAMP_MAGFILTER, &OldSampler);
            pDevice->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
        }

        // main postfx passes
        {
            IDirect3DTexture9* mainTex = 0;
            if (PostFxResources.pHDRTex2 && PostFxResources.pHDRTex && PostFxResources.aoTex) {
                PostFxResources.renderTargetTex = PostFxResources.pHDRTex2;
                pDevice->GetTexture(2, (IDirect3DBaseTexture9**)&mainTex);
                PostFxResources.textureRead = mainTex;

                pDevice->GetRenderTarget(0, &backBuffer);
                PostFxResources.renderTargetTex->GetSurfaceLevel(0, &PostFxResources.renderTargetSurf);
                PostFxResources.textureRead->GetSurfaceLevel(0, &PostFxResources.surfaceRead);
                PostFxResources.aoTex->GetSurfaceLevel(0, &aoSurface);
                //PostFxResources.pHDRTex3->GetSurfaceLevel(0, &pHDRSurface3);

                // ready for new post processing?
                if (backBuffer && PostFxResources.renderTargetSurf && PostFxResources.surfaceRead && aoSurface) {
                    if (PostFxResources.UseSSAO && PostFxResources.SSAO_ps) {
                        if (useSSAA) {
                            pDevice->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
                        }

                        pDevice->SetPixelShader(PostFxResources.SSAO_ps);
                        vec4[0] = ((getWindowWidth() * 1) * (useSSAA ? PostFxResources.ResSSAA : 1));
                        vec4[1] = ((getWindowHeight() * 1) * (useSSAA ? PostFxResources.ResSSAA : 1));
                        vec4[2] = 1.f / ((getWindowWidth() * 1) * (useSSAA ? PostFxResources.ResSSAA : 1));
                        vec4[3] = 1.f / ((getWindowHeight() * 1) * (useSSAA ? PostFxResources.ResSSAA : 1));
                        pDevice->SetPixelShaderConstantF(210, vec4, 1);
                        vec4[1] = PostFxResources.AoDistance;
                        pDevice->SetPixelShaderConstantF(211, vec4, 1);
                        //pDevice->SetTexture(2, 0);
                        pDevice->SetRenderTarget(0, aoSurface);
                        pDevice->SetTexture(2, PostFxResources.textureRead);
                        DrawPrimitiveOriginal.unsafe_stdcall<HRESULT>(pDevice, PrimitiveType, StartVertex, PrimitiveCount);
                        //pDevice->SetTexture(2, 0);
                        pDevice->SetPixelShader(PostFxResources.SSAO_ps2);

                        pDevice->SetRenderTarget(0, PostFxResources.renderTargetSurf);

                        switch (PostFxResources.UseDebugTextures) {
                            //"None",
                            //"Difuse",
                            //"normal",
                            //"specular",
                            //"depth",
                            //"SMAA edges",
                            //"stencil"
                        case 0:
                            pDevice->SetTexture(2, PostFxResources.textureRead);
                            break;
                        case 1:
                            pDevice->SetTexture(2, PostFxResources.DiffuseTex);
                            break;
                        case 2:
                            pDevice->SetTexture(2, PostFxResources.NormalTex);
                            break;
                        case 3:
                            pDevice->SetTexture(2, PostFxResources.BloomTex);
                            break;
                        case 4:
                            pDevice->SetTexture(2, PostFxResources.SpecularTex);
                            break;
                        case 5:
                            pDevice->SetTexture(2, PostFxResources.DepthTex);
                            break;
                        case 6:
                            pDevice->SetTexture(2, PostFxResources.StencilTex);
                            break;
                        default:
                            pDevice->SetTexture(2, PostFxResources.textureRead);
                            break;
                        }
                        pDevice->SetTexture(1, PostFxResources.aoTex);
                        DrawPrimitiveOriginal.unsafe_stdcall<HRESULT>(pDevice, PrimitiveType, StartVertex, PrimitiveCount);
                        PostFxResources.swapbuffers();
                        pDevice->SetTexture(1, PostFxResources.prePostFx[1]);
                        //pDevice->SetTexture(2, 0);
                        pDevice->SetPixelShader(pShader);
                    }

                    if (PostFxResources.useStippleFilter && PostFxResources.stipple_filter_ps) {
                        pDevice->SetPixelShader(PostFxResources.stipple_filter_ps);
                        pDevice->SetRenderTarget(0, PostFxResources.renderTargetSurf);
                        pDevice->SetTexture(2, PostFxResources.textureRead);
                        DrawPrimitiveOriginal.unsafe_stdcall<HRESULT>(pDevice, PrimitiveType, StartVertex, PrimitiveCount);
                        PostFxResources.swapbuffers();
                        pDevice->SetPixelShader(pShader);
                    }

                    static auto dof = FusionFixSettings.GetRef("PREF_TCYC_DOF");
                    if (dof->get() > FusionFixSettings.DofText.eCutscenesOnly || (dof->get() == FusionFixSettings.DofText.eCutscenesOnly && Natives::HasCutsceneFinished()))
                    {
                        if (PostFxResources.useDepthOfField == 1 && PostFxResources.depth_of_field_ps) {
                            pDevice->SetPixelShader(PostFxResources.depth_of_field_ps);
                            pDevice->SetRenderTarget(0, PostFxResources.renderTargetSurf);
                            pDevice->SetTexture(2, PostFxResources.textureRead);
                            DrawPrimitiveOriginal.unsafe_stdcall<HRESULT>(pDevice, PrimitiveType, StartVertex, PrimitiveCount);
                            PostFxResources.swapbuffers();
                            //pDevice->SetTexture(2, 0);
                            pDevice->SetPixelShader(pShader);
                        }

                        if (PostFxResources.useDepthOfField == 2 && PostFxResources.depth_of_field_blur_ps && PostFxResources.depth_of_field_tent_ps && PostFxResources.depth_of_field_coc_ps) {
                            pDevice->GetDepthStencilSurface(&ppZStencilSurface);
                            PostFxResources.depthStenciltex->GetSurfaceLevel(0, &ppZStencilSurface2);
                            PostFxResources.pHDRDownsampleTex->GetSurfaceLevel(0, &pHDRDownsampleSurf);
                            PostFxResources.pHDRDownsampleTex2->GetSurfaceLevel(0, &pHDRDownsampleSurf2);

                            if (ppZStencilSurface && ppZStencilSurface2 && pHDRDownsampleSurf && pHDRDownsampleSurf2) {
                                pDevice->SetSamplerState(8, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
                                pDevice->SetSamplerState(8, D3DSAMP_ADDRESSU, D3DTADDRESS_MIRROR);
                                pDevice->SetSamplerState(8, D3DSAMP_ADDRESSV, D3DTADDRESS_MIRROR);
                                pDevice->SetSamplerState(2, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);

                                pDevice->SetDepthStencilSurface(ppZStencilSurface2);

                                pDevice->SetPixelShader(PostFxResources.depth_of_field_blur_ps);
                                pDevice->SetRenderTarget(0, pHDRDownsampleSurf2);
                                pDevice->SetTexture(2, PostFxResources.textureRead);
                                DrawPrimitiveOriginal.unsafe_stdcall<HRESULT>(pDevice, PrimitiveType, StartVertex, PrimitiveCount);

                                pDevice->SetPixelShader(PostFxResources.depth_of_field_tent_ps);
                                pDevice->SetRenderTarget(0, pHDRDownsampleSurf);
                                pDevice->SetTexture(8, PostFxResources.pHDRDownsampleTex2);
                                DrawPrimitiveOriginal.unsafe_stdcall<HRESULT>(pDevice, PrimitiveType, StartVertex, PrimitiveCount);

                                pDevice->SetPixelShader(PostFxResources.depth_of_field_coc_ps);
                                pDevice->SetDepthStencilSurface(ppZStencilSurface);
                                pDevice->SetRenderTarget(0, PostFxResources.renderTargetSurf);
                                pDevice->SetTexture(8, PostFxResources.pHDRDownsampleTex);
                                DrawPrimitiveOriginal.unsafe_stdcall<HRESULT>(pDevice, PrimitiveType, StartVertex, PrimitiveCount);
                                PostFxResources.swapbuffers();

                                pDevice->SetPixelShader(pShader);
                                SAFE_RELEASE(pHDRDownsampleSurf);
                                SAFE_RELEASE(pHDRDownsampleSurf2);

                                SAFE_RELEASE(ppZStencilSurface);
                                SAFE_RELEASE(ppZStencilSurface2);
                            }
                        }

                        if (PostFxResources.useDepthOfField == 3 && PostFxResources.dof_blur_ps && PostFxResources.dof_coc_ps) {
                            pDevice->GetDepthStencilSurface(&ppZStencilSurface);
                            PostFxResources.depthStenciltex->GetSurfaceLevel(0, &ppZStencilSurface2);
                            PostFxResources.pHDRDownsampleTex->GetSurfaceLevel(0, &pHDRDownsampleSurf);
                            PostFxResources.pHDRDownsampleTex2->GetSurfaceLevel(0, &pHDRDownsampleSurf2);

                            if (ppZStencilSurface && ppZStencilSurface2 && pHDRDownsampleSurf && pHDRDownsampleSurf2) {
                                pDevice->SetSamplerState(8, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
                                pDevice->SetSamplerState(8, D3DSAMP_ADDRESSU, D3DTADDRESS_MIRROR);
                                pDevice->SetSamplerState(8, D3DSAMP_ADDRESSV, D3DTADDRESS_MIRROR);
                                pDevice->SetSamplerState(2, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);

                                pDevice->SetDepthStencilSurface(ppZStencilSurface2);

                                pDevice->SetPixelShader(PostFxResources.dof_blur_ps);
                                pDevice->SetRenderTarget(0, pHDRDownsampleSurf);
                                pDevice->SetTexture(2, PostFxResources.textureRead);
                                pDevice->SetTexture(8, PostFxResources.pHalfHDRTex);
                                DrawPrimitiveOriginal.unsafe_stdcall<HRESULT>(pDevice, PrimitiveType, StartVertex, PrimitiveCount);

                                pDevice->SetPixelShader(PostFxResources.dof_coc_ps);
                                pDevice->SetDepthStencilSurface(ppZStencilSurface);
                                pDevice->SetRenderTarget(0, PostFxResources.renderTargetSurf);
                                pDevice->SetTexture(2, PostFxResources.textureRead);
                                pDevice->SetTexture(8, PostFxResources.pHDRDownsampleTex);
                                DrawPrimitiveOriginal.unsafe_stdcall<HRESULT>(pDevice, PrimitiveType, StartVertex, PrimitiveCount);
                                PostFxResources.swapbuffers();
                                pDevice->SetTexture(8, PostFxResources.pHalfHDRTex);

                                pDevice->SetPixelShader(pShader);
                                SAFE_RELEASE(pHDRDownsampleSurf);
                                SAFE_RELEASE(pHDRDownsampleSurf2);

                                SAFE_RELEASE(ppZStencilSurface);
                                SAFE_RELEASE(ppZStencilSurface2);
                            }
                        }

                        if (PostFxResources.useDepthOfField == 4 && PostFxResources.DOF_ps) {
                            //numPasses--;
                            pDevice->SetPixelShader(PostFxResources.DOF_ps);
                            pDevice->SetPixelShaderConstantF(212, PostFxResources.AOFocusPoint, 1);
                            pDevice->SetPixelShaderConstantF(213, PostFxResources.AOFocusScale, 1);
                            pDevice->SetPixelShaderConstantI(4, PostFxResources.DofSamples, 1);
                            //pDevice->SetTexture(2, 0);

                            pDevice->SetRenderTarget(0, PostFxResources.renderTargetSurf);

                            pDevice->SetTexture(2, PostFxResources.textureRead);
                            DrawPrimitiveOriginal.unsafe_stdcall<HRESULT>(pDevice, PrimitiveType, StartVertex, PrimitiveCount);
                            PostFxResources.swapbuffers();
                            //pDevice->SetTexture(2, 0);
                            pDevice->SetPixelShader(pShader);
                        }
                    }

                    // fxaa / smaa
                    if (UsePostFxAA->get() > FusionFixSettings.AntialiasingText.eMO_OFF) {
                        //numPasses--;
                        if (useSSAA) {
                            pDevice->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
                        }

                        // FXAA
                        if ((UsePostFxAA->get() == FusionFixSettings.AntialiasingText.eFXAA) && PostFxResources.FxaaPS) {
                            pDevice->GetRenderState(D3DRS_SRGBWRITEENABLE, &OldSRGB); // save srgb state
                            pDevice->SetPixelShader(PostFxResources.FxaaPS);
                            pDevice->SetPixelShaderConstantF(214, PostFxResources.AAparameters, 1); // need to edit FXAA shader to be set

                            pDevice->SetRenderTarget(0, PostFxResources.renderTargetSurf);

                            pDevice->SetTexture(2, PostFxResources.textureRead);
                            hr = DrawPrimitiveOriginal.unsafe_stdcall<HRESULT>(pDevice, PrimitiveType, StartVertex, PrimitiveCount);
                            PostFxResources.swapbuffers();
                            pDevice->SetSamplerState(2, D3DSAMP_SRGBTEXTURE, 0);
                            //pDevice->SetTexture(2, 0);
                            pDevice->SetPixelShader(pShader);
                            pDevice->SetRenderState(D3DRS_SRGBWRITEENABLE, OldSRGB);// restore srgb state
                        }

                        // SMAA
                        if (UsePostFxAA->get() >= FusionFixSettings.AntialiasingText.eSMAA &&
                            PostFxResources.SMAA_EdgeDetection && PostFxResources.SMAA_BlendingWeightsCalculation && PostFxResources.SMAA_NeighborhoodBlending &&
                            PostFxResources.SMAA_EdgeDetectionVS && PostFxResources.SMAA_BlendingWeightsCalculationVS && PostFxResources.SMAA_NeighborhoodBlendingVS &&
                            PostFxResources.areaTex && PostFxResources.searchTex && PostFxResources.edgesTex && PostFxResources.blendTex
                            ) {
                            DWORD oldSample = 0;
                            vec4[0] = 1.f / ((getWindowWidth() - 0) * (useSSAA ? PostFxResources.ResSSAA : 1));
                            vec4[1] = 1.f / ((getWindowHeight() - 0) * (useSSAA ? PostFxResources.ResSSAA : 1));
                            vec4[2] = ((getWindowWidth() - 0) * (useSSAA ? PostFxResources.ResSSAA : 1));
                            vec4[3] = ((getWindowHeight() - 0) * (useSSAA ? PostFxResources.ResSSAA : 1));
                            pDevice->SetPixelShaderConstantF(24, vec4, 1);
                            pDevice->SetVertexShaderConstantF(24, vec4, 1);

                            PostFxResources.edgesTex->GetSurfaceLevel(0, &edgesSurf);
                            PostFxResources.blendTex->GetSurfaceLevel(0, &blendSurf);

                            // SMAA_EdgeDetection
                            pDevice->GetRenderState(D3DRS_SRGBWRITEENABLE, &OldSRGB); // save srgb state
                            pDevice->SetRenderState(D3DRS_SRGBWRITEENABLE, 0);
                            pDevice->SetPixelShader(PostFxResources.SMAA_EdgeDetection);
                            pDevice->SetVertexShader(PostFxResources.SMAA_EdgeDetectionVS);
                            pDevice->SetPixelShaderConstantF(24, vec4, 1);
                            pDevice->SetVertexShaderConstantF(24, vec4, 1);
                            pDevice->SetRenderTarget(0, edgesSurf);
                            //pDevice->SetTexture(2, 0);
                            pDevice->SetTexture(0, PostFxResources.textureRead);
                            pDevice->GetSamplerState(3, D3DSAMP_MAGFILTER, &oldSample);
                            pDevice->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
                            pDevice->SetSamplerState(1, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
                            pDevice->SetSamplerState(2, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
                            pDevice->SetSamplerState(3, D3DSAMP_MAGFILTER, D3DTEXF_POINT);
                            pDevice->SetSamplerState(4, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
                            pDevice->Clear(0, 0, D3DCLEAR_TARGET, 0, 0, 0);
                            pDevice->SetSamplerState(0, D3DSAMP_SRGBTEXTURE, 1);
                            DrawPrimitiveOriginal.unsafe_stdcall<HRESULT>(pDevice, PrimitiveType, StartVertex, PrimitiveCount);
                            pDevice->SetSamplerState(0, D3DSAMP_SRGBTEXTURE, 0);
                            pDevice->SetTexture(0, 0);

                            // SMAA_BlendingWeightsCalculation
                            pDevice->SetPixelShader(PostFxResources.SMAA_BlendingWeightsCalculation);
                            pDevice->SetVertexShader(PostFxResources.SMAA_BlendingWeightsCalculationVS);
                            pDevice->SetPixelShaderConstantF(24, vec4, 1);
                            pDevice->SetVertexShaderConstantF(24, vec4, 1);
                            pDevice->SetRenderTarget(0, blendSurf);
                            pDevice->SetTexture(1, PostFxResources.edgesTex);
                            pDevice->SetTexture(2, PostFxResources.areaTex);
                            pDevice->SetTexture(3, PostFxResources.searchTex);
                            pDevice->Clear(0, 0, D3DCLEAR_TARGET, 0, 0, 0);
                            DrawPrimitiveOriginal.unsafe_stdcall<HRESULT>(pDevice, PrimitiveType, StartVertex, PrimitiveCount);
                            //pDevice->SetTexture(0, 0);
                            //pDevice->SetTexture(1, 0);
                            //pDevice->SetTexture(2, 0);
                            //pDevice->SetTexture(3, 0);
                            pDevice->SetRenderState(D3DRS_SRGBWRITEENABLE, OldSRGB);// restore srgb state

                            // SMAA_NeighborhoodBlending
                            pDevice->SetPixelShader(PostFxResources.SMAA_NeighborhoodBlending);
                            pDevice->SetVertexShader(PostFxResources.SMAA_NeighborhoodBlendingVS);
                            pDevice->SetPixelShaderConstantF(24, vec4, 1);
                            pDevice->SetVertexShaderConstantF(24, vec4, 1);
                            vec4[0] = static_cast<float>(UsePostFxAA->get());
                            vec4[1] = 1.0f;
                            vec4[2] = 3.0f;
                            vec4[3] = 4.0f;
                            pDevice->SetPixelShaderConstantF(5, vec4, 1);

                            pDevice->SetRenderTarget(0, PostFxResources.renderTargetSurf);

                            //pDevice->SetTexture(2,  textureRead);
                            pDevice->SetTexture(0, PostFxResources.textureRead);
                            pDevice->SetTexture(1, PostFxResources.edgesTex);
                            pDevice->SetTexture(4, PostFxResources.blendTex);
                            pDevice->SetRenderState(D3DRS_SRGBWRITEENABLE, 0);
                            DrawPrimitiveOriginal.unsafe_stdcall<HRESULT>(pDevice, PrimitiveType, StartVertex, PrimitiveCount);
                            PostFxResources.swapbuffers();
                            pDevice->SetTexture(0, PostFxResources.prePostFx[0]);
                            pDevice->SetTexture(1, PostFxResources.prePostFx[1]);
                            pDevice->SetTexture(2, PostFxResources.textureRead);
                            pDevice->SetTexture(3, PostFxResources.prePostFx[3]);
                            pDevice->SetTexture(4, PostFxResources.prePostFx[4]);
                            pDevice->SetSamplerState(3, D3DSAMP_MAGFILTER, oldSample);
                            pDevice->SetPixelShader(pShader);
                            pDevice->SetVertexShader(vShader);
                            SAFE_RELEASE(edgesSurf);
                            SAFE_RELEASE(blendSurf);
                        }
                    }

                    static auto refSunShafts = FusionFixSettings.GetRef("PREF_SUNSHAFTS");
                    if (refSunShafts->get())
                    {
                        // one pass half res
                        if (PostFxResources.UseSunShafts == 1 && PostFxResources.SSDownsampler_PS && PostFxResources.depth_of_field_tent_ps && PostFxResources.SunShafts_PS)
                        {
                            pDevice->GetDepthStencilSurface(&ppZStencilSurface);
                            PostFxResources.depthStenciltex->GetSurfaceLevel(0, &ppZStencilSurface2);
                            PostFxResources.pHDRDownsampleTex->GetSurfaceLevel(0, &pHDRDownsampleSurf);
                            PostFxResources.pHDRDownsampleTex2->GetSurfaceLevel(0, &pHDRDownsampleSurf2);

                            if (ppZStencilSurface && ppZStencilSurface2 && pHDRDownsampleSurf && pHDRDownsampleSurf2)
                            {
                                {
                                    pDevice->SetSamplerState(2, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
                                    pDevice->SetSamplerState(8, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
                                    pDevice->SetSamplerState(8, D3DSAMP_ADDRESSU, D3DTADDRESS_MIRROR);
                                    pDevice->SetSamplerState(8, D3DSAMP_ADDRESSV, D3DTADDRESS_MIRROR);
                                    pDevice->SetSamplerState(9, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
                                    pDevice->SetSamplerState(9, D3DSAMP_ADDRESSU, D3DTADDRESS_MIRROR);
                                    pDevice->SetSamplerState(9, D3DSAMP_ADDRESSV, D3DTADDRESS_MIRROR);
                                    pDevice->SetSamplerState(11, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
                                    pDevice->SetSamplerState(11, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
                                    pDevice->SetSamplerState(11, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
                                    pDevice->SetSamplerState(13, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
                                    pDevice->SetSamplerState(13, D3DSAMP_ADDRESSU, D3DTADDRESS_MIRROR);
                                    pDevice->SetSamplerState(13, D3DSAMP_ADDRESSV, D3DTADDRESS_MIRROR);

                                    pDevice->SetPixelShaderConstantF(96, PostFxResources.SunShafts_params1, 1);
                                    pDevice->SetPixelShaderConstantF(99, PostFxResources.SunShafts_params2, 1);
                                    pDevice->SetPixelShaderConstantF(97, SunDirection, 1);
                                    pDevice->SetPixelShaderConstantF(98, SunColor, 1);
                                    pDevice->SetPixelShaderConstantF(100, currGrcViewport->mWorldMatrix[0], 4);
                                    pDevice->SetPixelShaderConstantF(104, currGrcViewport->mWorldViewMatrix[0], 4);
                                    pDevice->SetPixelShaderConstantF(108, currGrcViewport->mWorldViewProjMatrix[0], 4);
                                    pDevice->SetPixelShaderConstantF(112, currGrcViewport->mViewInverseMatrix[0], 4);
                                    pDevice->SetPixelShaderConstantF(116, gShadowMatrix, 4);
                                    pDevice->SetPixelShaderConstantF(120, SunShaftsSamples2, 1);
                                    pDevice->SetPixelShaderConstantI(5, SunShaftsSamplesa, 1);
                                    pDevice->SetPixelShaderConstantI(6, SunShaftsSamplesb, 1);
                                    pDevice->SetPixelShaderConstantI(7, SunShaftsSamplesc, 1);
                                    pDevice->SetPixelShaderConstantI(8, SunShaftsSamplesd, 1);
                                }

                                pDevice->SetPixelShaderConstantF(218, blueTimerVec4, 1);
                                pDevice->SetPixelShaderConstantF(144, PostFxResources.NoiseSale, 1);
                                pDevice->SetDepthStencilSurface(ppZStencilSurface2);

                                pDevice->SetTexture(13, 0);
                                pDevice->SetTexture(11, 0);
                                pDevice->SetTexture(8, 0);

                                pDevice->SetPixelShader(PostFxResources.SSDownsampler_PS);
                                pDevice->SetRenderTarget(0, pHDRDownsampleSurf2);
                                pDevice->SetTexture(2, PostFxResources.textureRead);
                                pDevice->SetTexture(13, PostFxResources.DiffuseTex);
                                DrawPrimitiveOriginal.unsafe_stdcall<HRESULT>(pDevice, PrimitiveType, StartVertex, PrimitiveCount);

                                pDevice->SetPixelShader(PostFxResources.depth_of_field_tent_ps);
                                pDevice->SetRenderTarget(0, pHDRDownsampleSurf);
                                pDevice->SetTexture(8, PostFxResources.pHDRDownsampleTex2);
                                DrawPrimitiveOriginal.unsafe_stdcall<HRESULT>(pDevice, PrimitiveType, StartVertex, PrimitiveCount);

                                pDevice->SetPixelShader(PostFxResources.SunShafts_PS);
                                pDevice->SetRenderTarget(0, pHDRDownsampleSurf2);
                                pDevice->SetTexture(8, 0);
                                pDevice->SetTexture(11, PostFxResources.pHDRDownsampleTex);
                                DrawPrimitiveOriginal.unsafe_stdcall<HRESULT>(pDevice, PrimitiveType, StartVertex, PrimitiveCount);

                                pDevice->SetTexture(8, 0);
                                pDevice->SetTexture(11, 0);
                                pDevice->SetTexture(13, 0);

                                pDevice->SetPixelShader(PostFxResources.SSAdd_PS);
                                pDevice->SetDepthStencilSurface(ppZStencilSurface);
                                pDevice->SetRenderTarget(0, PostFxResources.renderTargetSurf);
                                pDevice->SetTexture(2, PostFxResources.textureRead);
                                pDevice->SetTexture(8, PostFxResources.pHalfHDRTex);
                                pDevice->SetTexture(11, PostFxResources.pHDRDownsampleTex2);
                                if (PostFxResources.bluenoisevolume)
                                    pDevice->SetTexture(9, PostFxResources.bluenoisevolume);

                                DrawPrimitiveOriginal.unsafe_stdcall<HRESULT>(pDevice, PrimitiveType, StartVertex, PrimitiveCount);
                                PostFxResources.swapbuffers();
                                pDevice->SetTexture(13, 0);
                                pDevice->SetTexture(11, 0);
                                pDevice->SetTexture(8, 0);

                                pDevice->SetPixelShader(pShader);
                                SAFE_RELEASE(pHDRDownsampleSurf);
                                SAFE_RELEASE(pHDRDownsampleSurf2);

                                SAFE_RELEASE(ppZStencilSurface);
                                SAFE_RELEASE(ppZStencilSurface2);
                            }
                        }

                        // 2 passes half res
                        if (PostFxResources.UseSunShafts == 2 && PostFxResources.SSDownsampler_PS && PostFxResources.depth_of_field_tent_ps && PostFxResources.SunShafts_PS && PostFxResources.SunShafts2_PS)
                        {
                            pDevice->GetDepthStencilSurface(&ppZStencilSurface);
                            PostFxResources.depthStenciltex->GetSurfaceLevel(0, &ppZStencilSurface2);
                            PostFxResources.pHDRDownsampleTex->GetSurfaceLevel(0, &pHDRDownsampleSurf);
                            PostFxResources.pHDRDownsampleTex2->GetSurfaceLevel(0, &pHDRDownsampleSurf2);

                            if (ppZStencilSurface && ppZStencilSurface2 && pHDRDownsampleSurf && pHDRDownsampleSurf2)
                            {
                                pDevice->SetSamplerState(2, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
                                pDevice->SetSamplerState(8, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
                                pDevice->SetSamplerState(8, D3DSAMP_ADDRESSU, D3DTADDRESS_MIRROR);
                                pDevice->SetSamplerState(8, D3DSAMP_ADDRESSV, D3DTADDRESS_MIRROR);
                                pDevice->SetSamplerState(9, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
                                pDevice->SetSamplerState(9, D3DSAMP_ADDRESSU, D3DTADDRESS_MIRROR);
                                pDevice->SetSamplerState(9, D3DSAMP_ADDRESSV, D3DTADDRESS_MIRROR);
                                pDevice->SetSamplerState(11, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
                                pDevice->SetSamplerState(11, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
                                pDevice->SetSamplerState(11, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
                                pDevice->SetSamplerState(13, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
                                pDevice->SetSamplerState(13, D3DSAMP_ADDRESSU, D3DTADDRESS_MIRROR);
                                pDevice->SetSamplerState(13, D3DSAMP_ADDRESSV, D3DTADDRESS_MIRROR);

                                pDevice->SetPixelShaderConstantF(96, PostFxResources.SunShafts_params1, 1);
                                pDevice->SetPixelShaderConstantF(99, PostFxResources.SunShafts_params2, 1);
                                pDevice->SetPixelShaderConstantF(97, SunDirection, 1);
                                pDevice->SetPixelShaderConstantF(98, SunColor, 1);
                                pDevice->SetPixelShaderConstantF(100, currGrcViewport->mWorldMatrix[0], 4);
                                pDevice->SetPixelShaderConstantF(104, currGrcViewport->mWorldViewMatrix[0], 4);
                                pDevice->SetPixelShaderConstantF(108, currGrcViewport->mWorldViewProjMatrix[0], 4);
                                pDevice->SetPixelShaderConstantF(112, currGrcViewport->mViewInverseMatrix[0], 4);
                                pDevice->SetPixelShaderConstantF(116, gShadowMatrix, 4);
                                pDevice->SetPixelShaderConstantF(120, SunShaftsSamples2, 1);
                                pDevice->SetPixelShaderConstantI(5, SunShaftsSamplesa, 1);
                                pDevice->SetPixelShaderConstantI(6, SunShaftsSamplesb, 1);
                                pDevice->SetPixelShaderConstantI(7, SunShaftsSamplesc, 1);
                                pDevice->SetPixelShaderConstantI(8, SunShaftsSamplesd, 1);

                                pDevice->SetPixelShaderConstantF(218, blueTimerVec4, 1);
                                pDevice->SetPixelShaderConstantF(144, PostFxResources.NoiseSale, 1);
                                pDevice->SetDepthStencilSurface(ppZStencilSurface2);

                                pDevice->SetTexture(13, 0);
                                pDevice->SetTexture(11, 0);
                                pDevice->SetTexture(8, 0);

                                pDevice->SetPixelShader(PostFxResources.SSDownsampler_PS);
                                pDevice->SetRenderTarget(0, pHDRDownsampleSurf2);
                                pDevice->SetTexture(2, PostFxResources.textureRead);
                                pDevice->SetTexture(13, PostFxResources.DiffuseTex);
                                DrawPrimitiveOriginal.unsafe_stdcall<HRESULT>(pDevice, PrimitiveType, StartVertex, PrimitiveCount);

                                pDevice->SetPixelShader(PostFxResources.depth_of_field_tent_ps);
                                pDevice->SetRenderTarget(0, pHDRDownsampleSurf);
                                pDevice->SetTexture(8, PostFxResources.pHDRDownsampleTex2);
                                DrawPrimitiveOriginal.unsafe_stdcall<HRESULT>(pDevice, PrimitiveType, StartVertex, PrimitiveCount);

                                pDevice->SetPixelShader(PostFxResources.SunShafts_PS);
                                pDevice->SetRenderTarget(0, pHDRDownsampleSurf2);
                                pDevice->SetTexture(8, 0);
                                pDevice->SetTexture(11, PostFxResources.pHDRDownsampleTex);
                                DrawPrimitiveOriginal.unsafe_stdcall<HRESULT>(pDevice, PrimitiveType, StartVertex, PrimitiveCount);

                                pDevice->SetPixelShader(PostFxResources.SunShafts2_PS);
                                pDevice->SetRenderTarget(0, pHDRDownsampleSurf);
                                pDevice->SetTexture(11, PostFxResources.pHDRDownsampleTex2);
                                DrawPrimitiveOriginal.unsafe_stdcall<HRESULT>(pDevice, PrimitiveType, StartVertex, PrimitiveCount);

                                pDevice->SetTexture(8, 0);
                                pDevice->SetTexture(11, 0);
                                pDevice->SetTexture(13, 0);

                                pDevice->SetPixelShader(PostFxResources.SSAdd_PS);
                                pDevice->SetDepthStencilSurface(ppZStencilSurface);
                                pDevice->SetRenderTarget(0, PostFxResources.renderTargetSurf);
                                pDevice->SetTexture(2, PostFxResources.textureRead);
                                pDevice->SetTexture(8, PostFxResources.pHalfHDRTex);
                                pDevice->SetTexture(11, PostFxResources.pHDRDownsampleTex);
                                if (PostFxResources.bluenoisevolume)
                                    pDevice->SetTexture(9, PostFxResources.bluenoisevolume);

                                DrawPrimitiveOriginal.unsafe_stdcall<HRESULT>(pDevice, PrimitiveType, StartVertex, PrimitiveCount);
                                PostFxResources.swapbuffers();
                                pDevice->SetTexture(13, 0);
                                pDevice->SetTexture(11, 0);
                                pDevice->SetTexture(8, 0);

                                pDevice->SetPixelShader(pShader);
                                SAFE_RELEASE(pHDRDownsampleSurf);
                                SAFE_RELEASE(pHDRDownsampleSurf2);

                                SAFE_RELEASE(ppZStencilSurface);
                                SAFE_RELEASE(ppZStencilSurface2);
                            }
                        }

                        // full screen
                        if (PostFxResources.UseSunShafts == 3 && PostFxResources.depth_of_field_blur_ps && PostFxResources.depth_of_field_tent_ps && PostFxResources.depth_of_field_coc_ps && PostFxResources.SunShafts3_PS)
                        {
                            pDevice->GetDepthStencilSurface(&ppZStencilSurface);
                            PostFxResources.depthStenciltex->GetSurfaceLevel(0, &ppZStencilSurface2);
                            PostFxResources.pHDRDownsampleTex->GetSurfaceLevel(0, &pHDRDownsampleSurf);
                            PostFxResources.pHDRDownsampleTex2->GetSurfaceLevel(0, &pHDRDownsampleSurf2);

                            if (ppZStencilSurface && ppZStencilSurface2 && pHDRDownsampleSurf && pHDRDownsampleSurf2 && pHDRSurface3) {
                                pDevice->SetSamplerState(2, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
                                pDevice->SetSamplerState(8, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
                                pDevice->SetSamplerState(8, D3DSAMP_ADDRESSU, D3DTADDRESS_MIRROR);
                                pDevice->SetSamplerState(8, D3DSAMP_ADDRESSV, D3DTADDRESS_MIRROR);
                                pDevice->SetSamplerState(9, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
                                pDevice->SetSamplerState(9, D3DSAMP_ADDRESSU, D3DTADDRESS_MIRROR);
                                pDevice->SetSamplerState(9, D3DSAMP_ADDRESSV, D3DTADDRESS_MIRROR);
                                pDevice->SetSamplerState(11, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
                                pDevice->SetSamplerState(11, D3DSAMP_ADDRESSU, D3DTADDRESS_MIRROR);
                                pDevice->SetSamplerState(11, D3DSAMP_ADDRESSV, D3DTADDRESS_MIRROR);
                                pDevice->SetSamplerState(13, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
                                pDevice->SetSamplerState(13, D3DSAMP_ADDRESSU, D3DTADDRESS_MIRROR);
                                pDevice->SetSamplerState(13, D3DSAMP_ADDRESSV, D3DTADDRESS_MIRROR);

                                pDevice->SetPixelShader(PostFxResources.SSDownsampler2_PS);
                                pDevice->SetRenderTarget(0, pHDRSurface3);
                                pDevice->SetTexture(2, PostFxResources.textureRead);
                                pDevice->SetTexture(13, PostFxResources.DiffuseTex);
                                DrawPrimitiveOriginal.unsafe_stdcall<HRESULT>(pDevice, PrimitiveType, StartVertex, PrimitiveCount);

                                pDevice->SetPixelShader(PostFxResources.SunShafts3_PS);
                                pDevice->SetRenderTarget(0, PostFxResources.renderTargetSurf);

                                pDevice->SetPixelShaderConstantF(96, PostFxResources.SunShafts_params1, 1);
                                pDevice->SetPixelShaderConstantF(99, PostFxResources.SunShafts_params2, 1);
                                pDevice->SetPixelShaderConstantF(97, SunDirection, 1);
                                pDevice->SetPixelShaderConstantF(98, SunColor, 1);
                                pDevice->SetPixelShaderConstantF(100, currGrcViewport->mWorldMatrix[0], 4);
                                pDevice->SetPixelShaderConstantF(104, currGrcViewport->mWorldViewMatrix[0], 4);
                                pDevice->SetPixelShaderConstantF(108, currGrcViewport->mWorldViewProjMatrix[0], 4);
                                pDevice->SetPixelShaderConstantF(112, currGrcViewport->mViewInverseMatrix[0], 4);
                                pDevice->SetPixelShaderConstantF(116, gShadowMatrix, 4);
                                pDevice->SetPixelShaderConstantF(120, SunShaftsSamples2, 1);
                                pDevice->SetPixelShaderConstantI(5, SunShaftsSamplesa, 1);
                                pDevice->SetPixelShaderConstantI(6, SunShaftsSamplesb, 1);
                                pDevice->SetPixelShaderConstantI(7, SunShaftsSamplesc, 1);
                                pDevice->SetPixelShaderConstantI(8, SunShaftsSamplesd, 1);

                                pDevice->SetPixelShaderConstantF(218, blueTimerVec4, 1);
                                pDevice->SetPixelShaderConstantF(144, PostFxResources.NoiseSale, 1);
                                //pDevice->SetTexture(8,  pHalfHDRTex);
                                pDevice->SetTexture(11, PostFxResources.pHDRTex3);
                                if (PostFxResources.bluenoisevolume)
                                    pDevice->SetTexture(9, PostFxResources.bluenoisevolume);

                                //pDevice->SetTexture(11,  pHDRDownsampleTex);
                                DrawPrimitiveOriginal.unsafe_stdcall<HRESULT>(pDevice, PrimitiveType, StartVertex, PrimitiveCount);
                                PostFxResources.swapbuffers();

                                pDevice->SetPixelShader(pShader);
                                SAFE_RELEASE(pHDRDownsampleSurf);
                                SAFE_RELEASE(pHDRDownsampleSurf2);

                                SAFE_RELEASE(ppZStencilSurface);
                                SAFE_RELEASE(ppZStencilSurface2);
                            }
                        }

                        // full screen 2 passes
                        if (PostFxResources.UseSunShafts == 4 && PostFxResources.depth_of_field_blur_ps && PostFxResources.depth_of_field_tent_ps && PostFxResources.depth_of_field_coc_ps && PostFxResources.SunShafts4_PS)
                        {
                            pDevice->GetDepthStencilSurface(&ppZStencilSurface);
                            PostFxResources.depthStenciltex->GetSurfaceLevel(0, &ppZStencilSurface2);
                            PostFxResources.pHDRDownsampleTex->GetSurfaceLevel(0, &pHDRDownsampleSurf);
                            PostFxResources.pHDRDownsampleTex2->GetSurfaceLevel(0, &pHDRDownsampleSurf2);

                            if (ppZStencilSurface && ppZStencilSurface2 && pHDRDownsampleSurf && pHDRDownsampleSurf2) {
                                pDevice->SetSamplerState(2, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
                                pDevice->SetSamplerState(8, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
                                pDevice->SetSamplerState(8, D3DSAMP_ADDRESSU, D3DTADDRESS_MIRROR);
                                pDevice->SetSamplerState(8, D3DSAMP_ADDRESSV, D3DTADDRESS_MIRROR);
                                pDevice->SetSamplerState(9, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
                                pDevice->SetSamplerState(9, D3DSAMP_ADDRESSU, D3DTADDRESS_MIRROR);
                                pDevice->SetSamplerState(9, D3DSAMP_ADDRESSV, D3DTADDRESS_MIRROR);
                                pDevice->SetSamplerState(11, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
                                pDevice->SetSamplerState(11, D3DSAMP_ADDRESSU, D3DTADDRESS_MIRROR);
                                pDevice->SetSamplerState(11, D3DSAMP_ADDRESSV, D3DTADDRESS_MIRROR);
                                pDevice->SetSamplerState(13, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
                                pDevice->SetSamplerState(13, D3DSAMP_ADDRESSU, D3DTADDRESS_MIRROR);
                                pDevice->SetSamplerState(13, D3DSAMP_ADDRESSV, D3DTADDRESS_MIRROR);

                                if (!(PostFxResources.useDepthOfField == 2)) {
                                    pDevice->SetDepthStencilSurface(ppZStencilSurface2);

                                    pDevice->SetPixelShader(PostFxResources.depth_of_field_blur_ps);
                                    pDevice->SetRenderTarget(0, pHDRDownsampleSurf2);
                                    pDevice->SetTexture(2, PostFxResources.textureRead);
                                    DrawPrimitiveOriginal.unsafe_stdcall<HRESULT>(pDevice, PrimitiveType, StartVertex, PrimitiveCount);

                                    pDevice->SetPixelShader(PostFxResources.depth_of_field_tent_ps);
                                    pDevice->SetRenderTarget(0, pHDRDownsampleSurf);
                                    pDevice->SetTexture(8, PostFxResources.pHDRDownsampleTex2);
                                    DrawPrimitiveOriginal.unsafe_stdcall<HRESULT>(pDevice, PrimitiveType, StartVertex, PrimitiveCount);
                                    pDevice->SetDepthStencilSurface(ppZStencilSurface);
                                }
                                pDevice->SetRenderTarget(0, PostFxResources.renderTargetSurf);
                                pDevice->SetTexture(2, PostFxResources.textureRead);

                                pDevice->SetPixelShader(PostFxResources.SunShafts4_PS);

                                pDevice->SetPixelShaderConstantF(96, PostFxResources.SunShafts_params1, 1);
                                pDevice->SetPixelShaderConstantF(99, PostFxResources.SunShafts_params2, 1);
                                pDevice->SetPixelShaderConstantF(97, SunDirection, 1);
                                pDevice->SetPixelShaderConstantF(98, SunColor, 1);
                                pDevice->SetPixelShaderConstantF(100, currGrcViewport->mWorldMatrix[0], 4);
                                pDevice->SetPixelShaderConstantF(104, currGrcViewport->mWorldViewMatrix[0], 4);
                                pDevice->SetPixelShaderConstantF(108, currGrcViewport->mWorldViewProjMatrix[0], 4);
                                pDevice->SetPixelShaderConstantF(112, currGrcViewport->mViewInverseMatrix[0], 4);
                                pDevice->SetPixelShaderConstantF(116, gShadowMatrix, 4);
                                pDevice->SetPixelShaderConstantF(120, SunShaftsSamples2, 1);
                                pDevice->SetPixelShaderConstantI(5, SunShaftsSamplesa, 1);
                                pDevice->SetPixelShaderConstantI(6, SunShaftsSamplesb, 1);
                                pDevice->SetPixelShaderConstantI(7, SunShaftsSamplesc, 1);
                                pDevice->SetPixelShaderConstantI(8, SunShaftsSamplesd, 1);

                                pDevice->SetPixelShaderConstantF(218, blueTimerVec4, 1);
                                pDevice->SetPixelShaderConstantF(144, PostFxResources.NoiseSale, 1);
                                pDevice->SetTexture(8, PostFxResources.pHalfHDRTex);
                                if (PostFxResources.bluenoisevolume)
                                    pDevice->SetTexture(9, PostFxResources.bluenoisevolume);

                                pDevice->SetTexture(11, PostFxResources.pHDRDownsampleTex);
                                pDevice->SetTexture(13, PostFxResources.DiffuseTex);
                                DrawPrimitiveOriginal.unsafe_stdcall<HRESULT>(pDevice, PrimitiveType, StartVertex, PrimitiveCount);
                                PostFxResources.swapbuffers();

                                pDevice->SetPixelShader(pShader);
                                SAFE_RELEASE(pHDRDownsampleSurf);
                                SAFE_RELEASE(pHDRDownsampleSurf2);

                                SAFE_RELEASE(ppZStencilSurface);
                                SAFE_RELEASE(ppZStencilSurface2);
                            }
                        }
                    }

                    // game postfx
                    {
                        pDevice->SetRenderTarget(0, backBuffer);

                        for (int i = 0; i < 4; i++) {
                            if (PostFxResources.prePostFx[i]) {
                                pDevice->SetTexture(i, PostFxResources.prePostFx[i]);
                            }
                            pDevice->SetSamplerState(i, D3DSAMP_MAGFILTER, PostFxResources.Samplers[i]);
                        }
                        pDevice->SetSamplerState(8, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);

                        pDevice->SetPixelShaderConstantF(218, blueTimerVec4, 1);
                        pDevice->SetPixelShaderConstantF(144, PostFxResources.NoiseSale, 1);
                        pDevice->SetTexture(8, PostFxResources.pHalfHDRTex);
                        if (PostFxResources.bluenoisevolume)
                            pDevice->SetTexture(9, PostFxResources.bluenoisevolume);

                        pDevice->SetTexture(2, PostFxResources.textureRead);
                        pDevice->Clear(0, 0, D3DCLEAR_TARGET, 0, 0, 0);
                        hr = DrawPrimitiveOriginal.unsafe_stdcall<HRESULT>(pDevice, PrimitiveType, StartVertex, PrimitiveCount);
                    }
                    SAFE_RELEASE(backBuffer);
                    SAFE_RELEASE(PostFxResources.renderTargetSurf);
                    SAFE_RELEASE(PostFxResources.surfaceRead);
                    SAFE_RELEASE(aoSurface);
                    SAFE_RELEASE(mainTex);
                    SAFE_RELEASE(pHDRSurface3);

                    if (useSSAA) {
                        pDevice->SetSamplerState(0, D3DSAMP_MAGFILTER, OldSampler);
                    }
                    for (int i = 0; i < PostfxTextureCount; i++) {
                        if (PostFxResources.prePostFx[i]) {
                            pDevice->SetTexture(i, PostFxResources.prePostFx[i]);
                            //pDevice->SetSamplerState(i, D3DSAMP_MAGFILTER, Samplers[i]);
                            SAFE_RELEASE(PostFxResources.prePostFx[i]);
                        }
                        pDevice->SetSamplerState(i, D3DSAMP_MAGFILTER, PostFxResources.Samplers[i]);
                    }

                    return S_OK;
                }

                SAFE_RELEASE(backBuffer);
                SAFE_RELEASE(PostFxResources.renderTargetSurf);
                SAFE_RELEASE(PostFxResources.surfaceRead);
                SAFE_RELEASE(aoSurface);
                SAFE_RELEASE(mainTex);

                if (useSSAA) {
                    pDevice->SetSamplerState(0, D3DSAMP_MAGFILTER, OldSampler);
                }
                for (int i = 0; i < PostfxTextureCount; i++) {
                    if (PostFxResources.prePostFx[i]) {
                        pDevice->SetTexture(i, PostFxResources.prePostFx[i]);
                        pDevice->SetSamplerState(i, D3DSAMP_MAGFILTER, PostFxResources.Samplers[i]);
                        SAFE_RELEASE(PostFxResources.prePostFx[i]);
                    }
                    pDevice->SetSamplerState(i, D3DSAMP_MAGFILTER, PostFxResources.Samplers[i]);
                }

                return S_FALSE;
            }
        }

        for (int i = 0; i < PostfxTextureCount; i++) {
            if (PostFxResources.prePostFx[i]) {
                pDevice->SetTexture(i, PostFxResources.prePostFx[i]);
                pDevice->SetSamplerState(i, D3DSAMP_MAGFILTER, PostFxResources.Samplers[i]);
                SAFE_RELEASE(PostFxResources.prePostFx[i]);
            }
        }
        return S_FALSE;
    }
} PostFX;
