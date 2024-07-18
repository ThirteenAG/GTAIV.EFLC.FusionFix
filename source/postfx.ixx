module;

#include <common.hxx>
#include <d3dx9tex.h>

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
#define IDR_depth_of_field_tent_ps               113
#define IDR_stipple_filter_ps                    116
#define IDR_DeferredShadowGen_ps                 117
#define IDR_deferred_lighting_PS1                118
#define IDR_deferred_lighting_PS2                119
#define IDR_SSAO_gen_ps                          120
#define IDR_SSAO_blend_ps                        121

#define IDR_DOF_ps                               125
#define IDR_DeferredShadowBlur                   126
#define IDR_SunShafts_PS                         127
#define IDR_CascadeAtlasGen                      128
#define IDR_SSDiffuseCloudsGen                   129

#ifndef SAFE_RELEASE
#define SAFE_RELEASE(p) { if (p) { (p)->Release(); (p)=NULL; } }
#endif

std::optional<std::reference_wrapper<int32_t>> UsePostFxAA;

class PostFxResource {
public:

    // --------- load --------- 
    IDirect3DTexture9* SMAA_areaTex = nullptr; // loaded from file
    IDirect3DTexture9* SMAA_searchTex = nullptr; // loaded from file
    IDirect3DVolumeTexture9* blueNoiseVolume = nullptr; // loaded from file

    // --------- game textures --------- 
    IDirect3DTexture9* FullScreenTex = nullptr; // game hdr texture
    IDirect3DTexture9* FullScreenTex2 = nullptr; // game hdr texture
    IDirect3DTexture9* HalfScreenTex = nullptr; // game half res screen texture
    IDirect3DTexture9* pQuarterHDRTex = nullptr; //  game 1/4 res screen texture
    IDirect3DTexture9* CascadeAtlasTex = nullptr;
    IDirect3DSurface9* CascadeAtlasSurf = nullptr;

    IDirect3DTexture9* NormalTex = nullptr;
    IDirect3DTexture9* DiffuseTex = nullptr;
    IDirect3DTexture9* SpecularTex = nullptr;
    IDirect3DTexture9* DepthTex = nullptr;
    IDirect3DTexture9* StencilTex = nullptr;
    IDirect3DTexture9* BloomTex = nullptr;
    IDirect3DTexture9* CurrentLumTex = nullptr;


    //------- full screen ---------
    // postfx textures created
    rage::grcRenderTargetPC* FullScreenTex_temp1 = nullptr; // main temp texture
    rage::grcRenderTargetPC* FullScreenTex_temp2 = nullptr; // main temp texture

    rage::grcRenderTargetPC* pShadowBlurTex1 = nullptr; // main shadow temp texture
    rage::grcRenderTargetPC* pShadowBlurTex2 = nullptr; // main shadow temp texture

    // smaa textures
    rage::grcRenderTargetPC* edgesTex = nullptr; // smaa gen
    rage::grcRenderTargetPC* blendTex = nullptr; // smaa gen

    // temp set and used in postfx
    IDirect3DTexture9* renderTargetTex = nullptr;
    IDirect3DTexture9* textureRead = nullptr;
    IDirect3DSurface9* renderTargetSurf = nullptr;
    IDirect3DSurface9* surfaceRead = nullptr;


    //-------- half resolution screen --------------
    rage::grcRenderTargetPC* FullScreenDownsampleTex = nullptr; // main downsampled texture
    rage::grcRenderTargetPC* FullScreenDownsampleTex2 = nullptr; // main downsampled texture


    // ----------- unused ----------- 
    IDirect3DTexture9* stencilDownsampled = nullptr; // gen
    rage::grcRenderTargetPC* HalfDepthStenciltex = nullptr; // gen


    // game render targets
    rage::grcRenderTargetPC* mSpecularAoRT = nullptr;
    rage::grcRenderTargetPC* mNormalRT = nullptr;
    rage::grcRenderTargetPC* mDiffuseRT = nullptr;
    rage::grcRenderTargetPC* mSpecularRT = nullptr;
    rage::grcRenderTargetPC* mDepthRT = nullptr;
    rage::grcRenderTargetPC* mStencilRT = nullptr;
    rage::grcRenderTargetPC* mFullScreenRT = nullptr;
    rage::grcRenderTargetPC* mFullScreenRT2 = nullptr;
    rage::grcRenderTargetPC* mHalfScreenRT = nullptr;
    rage::grcRenderTargetPC* mCascadeAtlasRT = nullptr;
    rage::grcRenderTargetPC* mQuarterScreenRT = nullptr;
    rage::grcRenderTargetPC* mBloomRT = nullptr;
    rage::grcRenderTargetPC* mCurrentLum = nullptr;


    // surfaces
    IDirect3DSurface9* FullScreenSurface = nullptr;
    IDirect3DSurface9* FullScreenSurface2 = nullptr;
    IDirect3DSurface9* FullScreenSurface_temp1 = nullptr;
    IDirect3DSurface9* FullScreenSurface_temp2 = nullptr;
    IDirect3DSurface9* FullScreenDownsampleSurf = nullptr;
    IDirect3DSurface9* FullScreenDownsampleSurf2 = nullptr;
    IDirect3DSurface9* backBuffer = nullptr;
    IDirect3DSurface9* edgesSurf = nullptr;
    IDirect3DSurface9* blendSurf = nullptr;
    IDirect3DSurface9* ppZStencilSurface = nullptr; // game depth buffer, get with GetDepthStencilSurface
    IDirect3DSurface9* halfZStencilSurface = nullptr; // half screen temp depth buffer
    IDirect3DSurface9* pShadowBlurSurf1 = nullptr;
    IDirect3DSurface9* pShadowBlurSurf2 = nullptr;

    IDirect3DSurface9* stencilDownsampledSurf = nullptr;


    // shaders
    IDirect3DPixelShader9* FxaaPS = nullptr;

    IDirect3DPixelShader9* SunShafts_PS = nullptr;
    IDirect3DPixelShader9* SunShafts2_PS = nullptr;
    IDirect3DPixelShader9* SunShafts3_PS = nullptr;
    IDirect3DPixelShader9* SunShafts4_PS = nullptr;
    IDirect3DPixelShader9* SSAdd_PS = nullptr;
    IDirect3DPixelShader9* SSDownsampler_PS = nullptr;
    IDirect3DPixelShader9* SSDownsampler2_PS = nullptr;
    IDirect3DPixelShader9* SSDiffuseCloudsGen_PS = nullptr;
    IDirect3DPixelShader9* SSAO_gen_ps = nullptr;
    IDirect3DPixelShader9* SSAO_blend_ps = nullptr;


    IDirect3DPixelShader9* dof_blur_ps = nullptr;
    IDirect3DPixelShader9* dof_coc_ps = nullptr;

    IDirect3DPixelShader9* depth_of_field_tent_ps = nullptr;

    IDirect3DPixelShader9* stipple_filter_ps = nullptr;

    IDirect3DPixelShader9* SMAA_EdgeDetection = nullptr;
    IDirect3DPixelShader9* SMAA_BlendingWeightsCalculation = nullptr;
    IDirect3DPixelShader9* SMAA_NeighborhoodBlending = nullptr;
    IDirect3DVertexShader9* SMAA_EdgeDetectionVS = nullptr;
    IDirect3DVertexShader9* SMAA_BlendingWeightsCalculationVS = nullptr;
    IDirect3DVertexShader9* SMAA_NeighborhoodBlendingVS = nullptr;

    IDirect3DPixelShader9* DeferredShadowGen_ps = nullptr;
    IDirect3DPixelShader9* DeferredShadowBlurH_ps = nullptr;
    IDirect3DPixelShader9* DeferredShadowBlurV_ps = nullptr;
    IDirect3DPixelShader9* DeferredShadowBlurCircle_ps = nullptr;
    IDirect3DPixelShader9* deferred_lighting_PS1 = nullptr;
    IDirect3DPixelShader9* deferred_lighting_PS2 = nullptr;

    IDirect3DPixelShader9* CascadeAtlasGen = nullptr;

    //std::unordered_map<IDirect3DPixelShader9*, int> ShaderListPS;
    //std::unordered_map<IDirect3DVertexShader9*, int> ShaderListVS;

    bool EnablePostfx = false;
    float AoDistance = 100;
    float AOFocusPoint[4] = { 350, 0, 0, 0 };
    float AOFocusScale[4] = { 300, 0, 0, 0 };
    float AAparameters[4] = { 0.25f , 0.125f , 0.0f, 0.0f };
    float SunShafts_params1[4] = { 1.5f, 0.95f, 0.01f, 0.975f };
    float SunShafts_params2[4] = { 0.5f, 1.f, 2.f, 1.5f };

    float ResSSAA = 1.0f;

    // 0 off, 1 horizontal, 2 vertical, 3 horizontal e vertical.
    int useScreenSpaceShadowsBlur = 3;
    int UseSunShafts = 2;
    int useDepthOfField = 3;
    int UseDebugTextures = 0;
    bool UseSSAO = false;
    bool useStippleFilter = true;
    bool useHardwareBilinearSampling = false;

    int SunShaftsSamples[4] = { 20, 20, 20, 20 };
    int DofSamples[4] = { 20 , 20, 20 ,20 };
    float NoiseSale[4] = { 1.f / 256, 0.3f, -0.5f, 0 };
    float BilateralDepthTreshold[4] = { 0.003f, 0.002f, 0.004f, 0.005f };

    bool shadersLoaded = false;

    bool loadShaders(LPDIRECT3DDEVICE9 pDevice, HMODULE hm) {
        ID3DXBuffer* bf1 = nullptr;
        ID3DXBuffer* bf2 = nullptr;
        ID3DXConstantTable* ppConstantTable = nullptr;

        //asm
        if(!FxaaPS && D3DXAssembleShaderFromResourceW(hm, MAKEINTRESOURCEW(IDR_FXAA), NULL, NULL, 0, &bf1, &bf2) == S_OK) {                                                                                             if(pDevice->CreatePixelShader( (DWORD*) bf1->GetBufferPointer(), &FxaaPS) != S_OK || !FxaaPS) SAFE_RELEASE(FxaaPS); SAFE_RELEASE(bf1); SAFE_RELEASE(bf2); }
        if(!dof_blur_ps && D3DXAssembleShaderFromResourceW(hm, MAKEINTRESOURCEW(IDR_dof_blur_ps), NULL, NULL, 0, &bf1, &bf2) == S_OK) {                                                                                 if(pDevice->CreatePixelShader( (DWORD*) bf1->GetBufferPointer(), &dof_blur_ps) != S_OK || !dof_blur_ps) SAFE_RELEASE(dof_blur_ps); SAFE_RELEASE(bf1); SAFE_RELEASE(bf2); }
        if(!dof_coc_ps && D3DXAssembleShaderFromResourceW(hm, MAKEINTRESOURCEW(IDR_dof_coc_ps), NULL, NULL, 0, &bf1, &bf2) == S_OK) {                                                                                   if(pDevice->CreatePixelShader( (DWORD*) bf1->GetBufferPointer(), &dof_coc_ps) != S_OK || !dof_coc_ps) SAFE_RELEASE(dof_coc_ps); SAFE_RELEASE(bf1); SAFE_RELEASE(bf2); }
        if(!depth_of_field_tent_ps && D3DXAssembleShaderFromResourceW(hm, MAKEINTRESOURCEW(IDR_depth_of_field_tent_ps), NULL, NULL, 0, &bf1, &bf2) == S_OK) {                                                           if(pDevice->CreatePixelShader( (DWORD*) bf1->GetBufferPointer(), &depth_of_field_tent_ps) != S_OK || !depth_of_field_tent_ps) SAFE_RELEASE(depth_of_field_tent_ps); SAFE_RELEASE(bf1); SAFE_RELEASE(bf2); }
        if(!stipple_filter_ps && D3DXAssembleShaderFromResourceW(hm, MAKEINTRESOURCEW(IDR_stipple_filter_ps), NULL, NULL, 0, &bf1, &bf2) == S_OK) {                                                                     if(pDevice->CreatePixelShader( (DWORD*) bf1->GetBufferPointer(), &stipple_filter_ps) != S_OK || !stipple_filter_ps) SAFE_RELEASE(stipple_filter_ps); SAFE_RELEASE(bf1); SAFE_RELEASE(bf2); }
        if(!DeferredShadowGen_ps && D3DXAssembleShaderFromResourceW(hm, MAKEINTRESOURCEW(IDR_DeferredShadowGen_ps), NULL, NULL, 0, &bf1, &bf2) == S_OK) {                                                               if(pDevice->CreatePixelShader( (DWORD*) bf1->GetBufferPointer(), &DeferredShadowGen_ps) != S_OK || !DeferredShadowGen_ps) SAFE_RELEASE(DeferredShadowGen_ps); SAFE_RELEASE(bf1); SAFE_RELEASE(bf2); }
        if(!deferred_lighting_PS1 && D3DXAssembleShaderFromResourceW(hm, MAKEINTRESOURCEW(IDR_deferred_lighting_PS1), NULL, NULL, 0, &bf1, &bf2) == S_OK) {                                                             if(pDevice->CreatePixelShader( (DWORD*) bf1->GetBufferPointer(), &deferred_lighting_PS1) != S_OK || !deferred_lighting_PS1) SAFE_RELEASE(deferred_lighting_PS1); SAFE_RELEASE(bf1); SAFE_RELEASE(bf2); }
        if(!deferred_lighting_PS2 && D3DXAssembleShaderFromResourceW(hm, MAKEINTRESOURCEW(IDR_deferred_lighting_PS2), NULL, NULL, 0, &bf1, &bf2) == S_OK) {                                                             if(pDevice->CreatePixelShader( (DWORD*) bf1->GetBufferPointer(), &deferred_lighting_PS2) != S_OK || !deferred_lighting_PS2) SAFE_RELEASE(deferred_lighting_PS2); SAFE_RELEASE(bf1); SAFE_RELEASE(bf2); }
        if(!SSAO_gen_ps && D3DXAssembleShaderFromResourceW(hm, MAKEINTRESOURCEW(IDR_SSAO_gen_ps), NULL, NULL, 0, &bf1, &bf2) == S_OK) {                                                                                 if(pDevice->CreatePixelShader( (DWORD*) bf1->GetBufferPointer(), &SSAO_gen_ps) != S_OK || !SSAO_gen_ps) SAFE_RELEASE(SSAO_gen_ps); SAFE_RELEASE(bf1); SAFE_RELEASE(bf2); }
        if(!SSAO_blend_ps && D3DXAssembleShaderFromResourceW(hm, MAKEINTRESOURCEW(IDR_SSAO_blend_ps), NULL, NULL, 0, &bf1, &bf2) == S_OK) {                                                                             if(pDevice->CreatePixelShader( (DWORD*) bf1->GetBufferPointer(), &SSAO_blend_ps) != S_OK || !SSAO_blend_ps) SAFE_RELEASE(SSAO_blend_ps); SAFE_RELEASE(bf1); SAFE_RELEASE(bf2); }
        if(!CascadeAtlasGen && D3DXAssembleShaderFromResourceW(hm, MAKEINTRESOURCEW(IDR_CascadeAtlasGen), NULL, NULL, 0, &bf1, &bf2) == S_OK) {                                                                         if(pDevice->CreatePixelShader( (DWORD*) bf1->GetBufferPointer(), &CascadeAtlasGen) != S_OK || !CascadeAtlasGen) SAFE_RELEASE(CascadeAtlasGen); SAFE_RELEASE(bf1); SAFE_RELEASE(bf2); }
        if(!SSDiffuseCloudsGen_PS && D3DXAssembleShaderFromResourceW(hm, MAKEINTRESOURCEW(IDR_SSDiffuseCloudsGen), NULL, NULL, 0, &bf1, &bf2) == S_OK) {                                                                if(pDevice->CreatePixelShader( (DWORD*) bf1->GetBufferPointer(), &SSDiffuseCloudsGen_PS) != S_OK || !SSDiffuseCloudsGen_PS) SAFE_RELEASE(SSDiffuseCloudsGen_PS); SAFE_RELEASE(bf1); SAFE_RELEASE(bf2); }

        //hlsl
        if(!SunShafts_PS && D3DXCompileShaderFromResourceW(hm, MAKEINTRESOURCEW(IDR_SunShafts_PS), NULL, NULL, "SunShafts1", "ps_3_0", 0, &bf1, &bf2, &ppConstantTable) == S_OK) {                                      if(pDevice->CreatePixelShader( (DWORD*) bf1->GetBufferPointer(), &SunShafts_PS) != S_OK || !SunShafts_PS) SAFE_RELEASE(SunShafts_PS); SAFE_RELEASE(bf1); SAFE_RELEASE(bf2); SAFE_RELEASE(ppConstantTable); }
        if(!SunShafts2_PS && D3DXCompileShaderFromResourceW(hm, MAKEINTRESOURCEW(IDR_SunShafts_PS), NULL, NULL, "SunShafts2", "ps_3_0", 0, &bf1, &bf2, &ppConstantTable) == S_OK) {                                     if(pDevice->CreatePixelShader( (DWORD*) bf1->GetBufferPointer(), &SunShafts2_PS) != S_OK || !SunShafts2_PS) SAFE_RELEASE(SunShafts2_PS); SAFE_RELEASE(bf1); SAFE_RELEASE(bf2); SAFE_RELEASE(ppConstantTable); }
        if(!SunShafts3_PS && D3DXCompileShaderFromResourceW(hm, MAKEINTRESOURCEW(IDR_SunShafts_PS), NULL, NULL, "SunShafts3", "ps_3_0", 0, &bf1, &bf2, &ppConstantTable) == S_OK) {                                     if(pDevice->CreatePixelShader( (DWORD*) bf1->GetBufferPointer(), &SunShafts3_PS) != S_OK || !SunShafts3_PS) SAFE_RELEASE(SunShafts3_PS); SAFE_RELEASE(bf1); SAFE_RELEASE(bf2); SAFE_RELEASE(ppConstantTable); }
        if(!SunShafts4_PS && D3DXCompileShaderFromResourceW(hm, MAKEINTRESOURCEW(IDR_SunShafts_PS), NULL, NULL, "SunShafts4", "ps_3_0", 0, &bf1, &bf2, &ppConstantTable) == S_OK) {                                     if(pDevice->CreatePixelShader( (DWORD*) bf1->GetBufferPointer(), &SunShafts4_PS) != S_OK || !SunShafts4_PS) SAFE_RELEASE(SunShafts4_PS); SAFE_RELEASE(bf1); SAFE_RELEASE(bf2); SAFE_RELEASE(ppConstantTable); }
        if(!SSDownsampler_PS && D3DXCompileShaderFromResourceW(hm, MAKEINTRESOURCEW(IDR_SunShafts_PS), NULL, NULL, "SSDownsampler", "ps_3_0", 0, &bf1, &bf2, &ppConstantTable) == S_OK) {                               if(pDevice->CreatePixelShader( (DWORD*) bf1->GetBufferPointer(), &SSDownsampler_PS) != S_OK || !SSDownsampler_PS) SAFE_RELEASE(SSDownsampler_PS); SAFE_RELEASE(bf1); SAFE_RELEASE(bf2); SAFE_RELEASE(ppConstantTable); }
        if(!SSDownsampler2_PS && D3DXCompileShaderFromResourceW(hm, MAKEINTRESOURCEW(IDR_SunShafts_PS), NULL, NULL, "SSDownsampler2", "ps_3_0", 0, &bf1, &bf2, &ppConstantTable) == S_OK) {                             if(pDevice->CreatePixelShader( (DWORD*) bf1->GetBufferPointer(), &SSDownsampler2_PS) != S_OK || !SSDownsampler2_PS) SAFE_RELEASE(SSDownsampler2_PS); SAFE_RELEASE(bf1); SAFE_RELEASE(bf2); SAFE_RELEASE(ppConstantTable); }

        if(!SSAdd_PS && D3DXCompileShaderFromResourceW(hm, MAKEINTRESOURCEW(IDR_SunShafts_PS), NULL, NULL, "SSAdd", "ps_3_0", 0, &bf1, &bf2, &ppConstantTable) == S_OK) {                                               if(pDevice->CreatePixelShader( (DWORD*) bf1->GetBufferPointer(), &SSAdd_PS) != S_OK || !SSAdd_PS) SAFE_RELEASE(SSAdd_PS); SAFE_RELEASE(bf1); SAFE_RELEASE(bf2); SAFE_RELEASE(ppConstantTable); }
        if(!DeferredShadowBlurH_ps && D3DXCompileShaderFromResourceW(hm, MAKEINTRESOURCEW(IDR_DeferredShadowBlur), NULL, NULL, "BlurHorizontal", "ps_3_0", 0, &bf1, &bf2, &ppConstantTable) == S_OK) {                  if(pDevice->CreatePixelShader( (DWORD*) bf1->GetBufferPointer(), &DeferredShadowBlurH_ps) != S_OK || !DeferredShadowBlurH_ps) SAFE_RELEASE(DeferredShadowBlurH_ps); SAFE_RELEASE(bf1); SAFE_RELEASE(bf2); SAFE_RELEASE(ppConstantTable); }
        if(!DeferredShadowBlurV_ps && D3DXCompileShaderFromResourceW(hm, MAKEINTRESOURCEW(IDR_DeferredShadowBlur), NULL, NULL, "BlurVertical", "ps_3_0", 0, &bf1, &bf2, &ppConstantTable) == S_OK) {                    if(pDevice->CreatePixelShader( (DWORD*) bf1->GetBufferPointer(), &DeferredShadowBlurV_ps) != S_OK || !DeferredShadowBlurV_ps) SAFE_RELEASE(DeferredShadowBlurV_ps); SAFE_RELEASE(bf1); SAFE_RELEASE(bf2); SAFE_RELEASE(ppConstantTable); }
        if(!DeferredShadowBlurCircle_ps && D3DXCompileShaderFromResourceW(hm, MAKEINTRESOURCEW(IDR_DeferredShadowBlur), NULL, NULL, "BlurCircular", "ps_3_0", 0, &bf1, &bf2, &ppConstantTable) == S_OK) {               if(pDevice->CreatePixelShader( (DWORD*) bf1->GetBufferPointer(), &DeferredShadowBlurCircle_ps) != S_OK || !DeferredShadowBlurCircle_ps) SAFE_RELEASE(DeferredShadowBlurCircle_ps); SAFE_RELEASE(bf1); SAFE_RELEASE(bf2); SAFE_RELEASE(ppConstantTable); }

        if(!SMAA_EdgeDetection && D3DXCompileShaderFromResourceW(hm, MAKEINTRESOURCEW(IDR_SMAA), NULL, NULL, "SMAAColorEdgeDetectionPS", "ps_3_0", 0, &bf1, &bf2, &ppConstantTable) == S_OK) {                          if(pDevice->CreatePixelShader( (DWORD*) bf1->GetBufferPointer(), &SMAA_EdgeDetection) != S_OK || !SMAA_EdgeDetection) SAFE_RELEASE(SMAA_EdgeDetection); SAFE_RELEASE(bf1); SAFE_RELEASE(bf2); SAFE_RELEASE(ppConstantTable); }
        if(!SMAA_BlendingWeightsCalculation && D3DXCompileShaderFromResourceW(hm, MAKEINTRESOURCEW(IDR_SMAA), NULL, NULL, "SMAABlendingWeightCalculationPS", "ps_3_0", 0, &bf1, &bf2, &ppConstantTable) == S_OK) {      if(pDevice->CreatePixelShader( (DWORD*) bf1->GetBufferPointer(), &SMAA_BlendingWeightsCalculation) != S_OK || !SMAA_BlendingWeightsCalculation) SAFE_RELEASE(SMAA_BlendingWeightsCalculation); SAFE_RELEASE(bf1); SAFE_RELEASE(bf2); SAFE_RELEASE(ppConstantTable); }
        if(!SMAA_NeighborhoodBlending && D3DXCompileShaderFromResourceW(hm, MAKEINTRESOURCEW(IDR_SMAA), NULL, NULL, "SMAANeighborhoodBlendingPS", "ps_3_0", 0, &bf1, &bf2, &ppConstantTable) == S_OK) {                 if(pDevice->CreatePixelShader( (DWORD*) bf1->GetBufferPointer(), &SMAA_NeighborhoodBlending) != S_OK || !SMAA_NeighborhoodBlending) SAFE_RELEASE(SMAA_NeighborhoodBlending); SAFE_RELEASE(bf1); SAFE_RELEASE(bf2); SAFE_RELEASE(ppConstantTable); }
        if(!SMAA_EdgeDetectionVS && D3DXCompileShaderFromResourceW(hm, MAKEINTRESOURCEW(IDR_SMAA), NULL, NULL, "SMAAEdgeDetectionVS", "vs_3_0", 0, &bf1, &bf2, &ppConstantTable) == S_OK) {                             if(pDevice->CreateVertexShader((DWORD*) bf1->GetBufferPointer(), &SMAA_EdgeDetectionVS) != S_OK || !SMAA_EdgeDetectionVS) SAFE_RELEASE(SMAA_EdgeDetectionVS); SAFE_RELEASE(bf1); SAFE_RELEASE(bf2); SAFE_RELEASE(ppConstantTable); }
        if(!SMAA_BlendingWeightsCalculationVS && D3DXCompileShaderFromResourceW(hm, MAKEINTRESOURCEW(IDR_SMAA), NULL, NULL, "SMAABlendingWeightCalculationVS", "vs_3_0", 0, &bf1, &bf2, &ppConstantTable) == S_OK) {    if(pDevice->CreateVertexShader((DWORD*) bf1->GetBufferPointer(), &SMAA_BlendingWeightsCalculationVS) != S_OK || !SMAA_BlendingWeightsCalculationVS) SAFE_RELEASE(SMAA_BlendingWeightsCalculationVS); SAFE_RELEASE(bf1); SAFE_RELEASE(bf2); SAFE_RELEASE(ppConstantTable); }
        if(!SMAA_NeighborhoodBlendingVS && D3DXCompileShaderFromResourceW(hm, MAKEINTRESOURCEW(IDR_SMAA), NULL, NULL, "SMAANeighborhoodBlendingVS", "vs_3_0", 0, &bf1, &bf2, &ppConstantTable) == S_OK) {               if(pDevice->CreateVertexShader((DWORD*) bf1->GetBufferPointer(), &SMAA_NeighborhoodBlendingVS) != S_OK || !SMAA_NeighborhoodBlendingVS) SAFE_RELEASE(SMAA_NeighborhoodBlendingVS); SAFE_RELEASE(bf1); SAFE_RELEASE(bf2); SAFE_RELEASE(ppConstantTable); }
        return ShadersFinishedLoading();
    }

#define PostfxTextureCount 15
    IDirect3DBaseTexture9* prePostFx[PostfxTextureCount] = { 0 };
    DWORD Samplers[PostfxTextureCount] = { D3DTEXF_LINEAR };

    bool resourcesFinishedLoading() {
        return SMAA_EdgeDetection && SMAA_BlendingWeightsCalculation && SMAA_NeighborhoodBlending &&
            SMAA_EdgeDetectionVS && SMAA_BlendingWeightsCalculationVS && SMAA_NeighborhoodBlendingVS &&
            SMAA_areaTex && SMAA_searchTex && edgesTex && blendTex;
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
        CurrentLumTex = nullptr;
        DepthTex = nullptr;
        CascadeAtlasTex = nullptr;

        FullScreenTex = nullptr;
        FullScreenTex2 = nullptr;
        HalfScreenTex = nullptr;
        pQuarterHDRTex = nullptr;

        if (FullScreenTex_temp1)
        {
            FullScreenTex_temp1->Destroy();
            FullScreenTex_temp1 = nullptr;
        }

        if (FullScreenTex_temp2)
        {
            FullScreenTex_temp2->Destroy();
            FullScreenTex_temp2 = nullptr;
        }

        if (edgesTex)
        {
            edgesTex->Destroy();
            edgesTex = nullptr;
        }

        if (blendTex)
        {
            blendTex->Destroy();
            blendTex = nullptr;
        }


        //SAFE_RELEASE(DepthStenciltex);
        SAFE_RELEASE(stencilDownsampled);

        if (FullScreenDownsampleTex)
        {
            FullScreenDownsampleTex->Destroy();
            FullScreenDownsampleTex = nullptr;
        }

        if (FullScreenDownsampleTex2)
        {
            FullScreenDownsampleTex2->Destroy();
            FullScreenDownsampleTex2 = nullptr;
        }

        if (HalfDepthStenciltex)
        {
            HalfDepthStenciltex->Destroy();
            HalfDepthStenciltex = nullptr;
        }

        if (pShadowBlurTex1)
        {
            pShadowBlurTex1->Destroy();
            pShadowBlurTex1 = nullptr;
        }

        if (pShadowBlurTex2)
        {
            pShadowBlurTex2->Destroy();
            pShadowBlurTex2 = nullptr;
        }
    }

    bool ShadersFinishedLoading() {
        if(FxaaPS && dof_blur_ps && dof_coc_ps && depth_of_field_tent_ps && stipple_filter_ps &&
           DeferredShadowGen_ps && deferred_lighting_PS1 && deferred_lighting_PS2 && SSAO_gen_ps &&
           SSAO_blend_ps && SunShafts_PS && SunShafts2_PS && SunShafts3_PS && SunShafts4_PS &&
           SSDownsampler_PS && SSDownsampler2_PS && SSAdd_PS && DeferredShadowBlurH_ps &&
           DeferredShadowBlurV_ps && DeferredShadowBlurCircle_ps
           && SMAA_EdgeDetection && SMAA_BlendingWeightsCalculation && SMAA_NeighborhoodBlending
           && SMAA_EdgeDetectionVS && SMAA_BlendingWeightsCalculationVS && SMAA_NeighborhoodBlendingVS)
            return true;

        return false;
    }

    void Readini() {
        CIniReader iniReader("");
        EnablePostfx = iniReader.ReadInteger("SRF", "EnablePostfx", 1);
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
        useStippleFilter = iniReader.ReadInteger("SRF", "StippleFilter", 1) != 0;

        // 0 off, 1 horizontal, 2 vertical, 3 horizontal e vertical.
        //useScreenSpaceShadowsBlur = iniReader.ReadInteger("SRF", "ScreenSpaceShadowsBlur", 0);
        //useHardwareBilinearSampling = iniReader.ReadInteger("SRF", "NewShadowAtlas", 0) != 0;

        DofSamples[0] = iniReader.ReadInteger("SRF", "DofSamples", 20);
        NoiseSale[0] = iniReader.ReadFloat("SRF", "NoiseSale", 1.f / 256.f);
        BilateralDepthTreshold[0] = iniReader.ReadFloat("SRF", "BilateralDepthTreshold", 0.003f);
    }

    void createTextures(UINT Width, UINT Height, HMODULE hm)
    {
        IDirect3DDevice9* pDevice = rage::grcDevice::GetD3DDevice();

        rage::grcRenderTargetDesc desc{};
        desc.mMultisampleCount = 0;
        desc.field_0 = 1;
        desc.field_12 = 1;
        desc.mDepthRT = nullptr;
        desc.field_8 = 1;
        desc.field_10 = 1;
        desc.field_11 = 1;
        desc.field_24 = false;
        desc.mFormat = rage::GRCFMT_A16B16G16R16F;

        auto CreateEmptyRT = [](const char* name, int32_t a2, uint32_t width, uint32_t height, uint32_t bitsPerPixel, rage::grcRenderTargetDesc* desc) -> rage::grcRenderTargetPC*
        {
            auto rt = rage::grcTextureFactory::GetInstance()->CreateRenderTarget(name, a2, width, height, bitsPerPixel, desc);
            rage::grcDevice::grcResolveFlags resolveFlags{};
            rage::grcTextureFactoryPC::GetInstance()->LockRenderTarget(0, rt, nullptr);
            rage::grcTextureFactoryPC::GetInstance()->UnlockRenderTarget(0, &resolveFlags);
            return rt;
        };

        FullScreenTex_temp1 = CreateEmptyRT("FullScreenTex_temp1", 3, Width, Height, 32, &desc);
        FullScreenTex_temp2 = CreateEmptyRT("FullScreenTex_temp2", 3, Width, Height, 32, &desc);

        desc.mFormat = rage::GRCFMT_G16R16F;

        pShadowBlurTex1 = CreateEmptyRT("pShadowBlurTex1", 3, Width, Height, 32, &desc);
        pShadowBlurTex2 = CreateEmptyRT("pShadowBlurTex2", 3, Width, Height, 32, &desc);

        desc.mFormat = rage::GRCFMT_X8R8G8B8;

        edgesTex = CreateEmptyRT("edgesTex", 3, Width, Height, 32, &desc);

        desc.mFormat = rage::GRCFMT_A8R8G8B8;

        blendTex = CreateEmptyRT("blendTex", 3, Width, Height, 32, &desc);

        desc.mFormat = rage::GRCFMT_A16B16G16R16F;

        FullScreenDownsampleTex = CreateEmptyRT("FullScreenDownsampleTex", 3, Width / 2, Height / 2, 32, &desc);
        FullScreenDownsampleTex2 = CreateEmptyRT("FullScreenDownsampleTex2", 3, Width / 2, Height / 2, 32, &desc);

        desc.mFormat = rage::GRCFMT_D24S8;
        desc.field_24 = true; //D3DUSAGE_DEPTHSTENCIL ?

        //pDevice->CreateTexture(Width / 2, Height / 2, 1, D3DUSAGE_DEPTHSTENCIL, D3DFMT_D24S8, D3DPOOL_DEFAULT, &HalfDepthStenciltex, 0);
        HalfDepthStenciltex = CreateEmptyRT("HalfDepthStenciltex", 3, Width / 2, Height / 2, 32, &desc);

        if (!SMAA_areaTex)
            D3DXCreateTextureFromResourceExW(pDevice, hm, MAKEINTRESOURCEW(IDR_AreaTex), 0, 0, 0, 0, D3DFMT_UNKNOWN, D3DPOOL_MANAGED, D3DX_FILTER_LINEAR, D3DX_FILTER_LINEAR, D3DCOLOR_ARGB(150, 100, 100, 100), NULL, NULL, &SMAA_areaTex);
        if (!SMAA_searchTex)
            D3DXCreateTextureFromResourceExW(pDevice, hm, MAKEINTRESOURCEW(IDR_SearchTex), 0, 0, 0, 0, D3DFMT_UNKNOWN, D3DPOOL_MANAGED, D3DX_FILTER_LINEAR, D3DX_FILTER_LINEAR, D3DCOLOR_ARGB(150, 100, 100, 100), NULL, NULL, &SMAA_searchTex);
        if (!blueNoiseVolume)
            D3DXCreateVolumeTextureFromResourceExW(pDevice, hm, MAKEINTRESOURCEW(IDR_bluenoisevolume), 0, 0, 0, 0, 0, D3DFMT_UNKNOWN, D3DPOOL_MANAGED, D3DX_FILTER_LINEAR, D3DX_FILTER_LINEAR, D3DCOLOR_ARGB(150, 100, 100, 100), NULL, NULL, &blueNoiseVolume);
    }
};

PostFxResource PostFxResources;

class PostFX
{
private:
    struct VertexFormat
    {
        float Pos[3];
        float TexCoord[2];
    };

    static inline IDirect3DVertexBuffer9* mQuadVertexBuffer;
    static inline IDirect3DVertexDeclaration9* mQuadVertexDecl;

    static inline UINT lastoffset = 0;
    static inline UINT laststride = 0;

    static inline IDirect3DVertexBuffer9* last_VertexBuffer=0;
    static inline IDirect3DVertexDeclaration9* last_VertexDecl=0;

    static inline IDirect3DBaseTexture9* prePostFx[PostfxTextureCount] = { 0 };
    static inline DWORD Samplers[PostfxTextureCount] = { D3DTEXF_LINEAR };
    static inline HMODULE hm = NULL;

    static void saveRenderState() {
        IDirect3DDevice9* pDevice = rage::grcDevice::GetD3DDevice();
        for(int i = 0; i < PostfxTextureCount; i++) {
            pDevice->GetTexture(i, &prePostFx[i]);
            pDevice->GetSamplerState(i, D3DSAMP_MAGFILTER, &Samplers[i]);
        }
    }

    static void restoreRenderState() {
        IDirect3DDevice9* pDevice = rage::grcDevice::GetD3DDevice();
        for(int i = 0; i < PostfxTextureCount; i++) {
            pDevice->SetTexture(i, prePostFx[i]);
            pDevice->SetSamplerState(i, D3DSAMP_MAGFILTER, Samplers[i]);
            SAFE_RELEASE(prePostFx[i]);
        }
    }

    static void __fastcall OnDeviceLost()
    {
        PostFxResources.ReleaseTextures();
        PostFxResources.mSpecularAoRT    =nullptr;
        PostFxResources.mNormalRT        =nullptr;
        PostFxResources.mDiffuseRT       =nullptr;
        PostFxResources.mSpecularRT      =nullptr;
        PostFxResources.mDepthRT         =nullptr;
        PostFxResources.mStencilRT       =nullptr;
        PostFxResources.mFullScreenRT    =nullptr;
        PostFxResources.mFullScreenRT2   =nullptr;
        PostFxResources.mHalfScreenRT    =nullptr;
        PostFxResources.mCascadeAtlasRT  =nullptr;
        PostFxResources.mQuarterScreenRT =nullptr;
        PostFxResources.mBloomRT         =nullptr;
        PostFxResources.mCurrentLum      =nullptr;

        if (mQuadVertexBuffer)
        {
            mQuadVertexBuffer->Release();
            mQuadVertexBuffer = nullptr;
        }

        if (mQuadVertexDecl)
        {
            mQuadVertexDecl->Release();
            mQuadVertexBuffer = nullptr;
        }

    }

    static void __fastcall OnDeviceReset()
    {
        PostFxResources.mNormalRT       = rage::grcTextureFactoryPC::GetRTByName( "_DEFERRED_GBUFFER_1_"  );
        PostFxResources.mDiffuseRT      = rage::grcTextureFactoryPC::GetRTByName( "_DEFERRED_GBUFFER_0_"  );
        PostFxResources.mSpecularRT     = rage::grcTextureFactoryPC::GetRTByName( "_DEFERRED_GBUFFER_2_"  );
        PostFxResources.mDepthRT        = rage::grcTextureFactoryPC::GetRTByName( "_DEFERRED_GBUFFER_3_"  );
        PostFxResources.mStencilRT      = rage::grcTextureFactoryPC::GetRTByName( "_STENCIL_BUFFER_"      );
        PostFxResources.mCascadeAtlasRT = rage::grcTextureFactoryPC::GetRTByName( "CASCADE_ATLAS"         );
        PostFxResources.mFullScreenRT   = rage::grcTextureFactoryPC::GetRTByName( "FullScreenCopy"        );
        PostFxResources.mFullScreenRT2  = rage::grcTextureFactoryPC::GetRTByName( "FullScreenCopy2"       );
        PostFxResources.mHalfScreenRT   = rage::grcTextureFactoryPC::GetRTByName( "Quarter Screen 0"      );
        PostFxResources.mQuarterScreenRT= rage::grcTextureFactoryPC::GetRTByName( "Blur Screen 0"         );
        PostFxResources.mBloomRT        = rage::grcTextureFactoryPC::GetRTByName( "Blur Screen 2 Copy"    );
        PostFxResources.mCurrentLum     = rage::grcTextureFactoryPC::GetRTByName( "Current Lum"           );

        auto width = *rage::grcDevice::ms_nActiveWidth;
        auto height = *rage::grcDevice::ms_nActiveHeight;

        PostFxResources.createTextures(width, height, hm);

        D3DVERTEXELEMENT9 vertexDeclElements[] =
        {
            {0, 0, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0},
            {0, 12, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0},
            D3DDECL_END()
        };

        rage::grcDevice::GetD3DDevice()->CreateVertexDeclaration(vertexDeclElements, &mQuadVertexDecl);

        rage::grcDevice::GetD3DDevice()->CreateVertexBuffer(6 * sizeof(VertexFormat), 0, 0, D3DPOOL_DEFAULT, &mQuadVertexBuffer, NULL);

        VertexFormat* vertexData = 0;
        D3DXVECTOR2 pixelSize =  D3DXVECTOR2(1.0f / width, 1.0f / height);

        mQuadVertexBuffer->Lock(0, 0, (void**)&vertexData, 0);

        vertexData[0] = { -1.0f - pixelSize.x, -1.0f + pixelSize.y, 0.0f, 0.0f, 1.0f };
        vertexData[1] = { -1.0f - pixelSize.x,  1.0f + pixelSize.y, 0.0f, 0.0f, 0.0f };
        vertexData[2] = {  1.0f - pixelSize.x, -1.0f + pixelSize.y, 0.0f, 1.0f, 1.0f };
        vertexData[3] = { -1.0f - pixelSize.x,  1.0f + pixelSize.y, 0.0f, 0.0f, 0.0f };
        vertexData[4] = {  1.0f - pixelSize.x,  1.0f + pixelSize.y, 0.0f, 1.0f, 0.0f };
        vertexData[5] = {  1.0f - pixelSize.x, -1.0f + pixelSize.y, 0.0f, 1.0f, 1.0f };

        mQuadVertexBuffer->Unlock();
    }

    static void Init() {
        static bool initialized = false;
        if(initialized)
            return;

        // none, fxaa, smaa, blend, edge
        UsePostFxAA = FusionFixSettings.GetRef("PREF_ANTIALIASING");

        auto onLostCB = rage::grcDevice::Functor0(NULL, OnDeviceLost, NULL, 0);
        auto onResetCB = rage::grcDevice::Functor0(NULL, OnDeviceReset, NULL, 0);
        rage::grcDevice::RegisterDeviceCallbacks(onLostCB, onResetCB);

        GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCWSTR) &Init, &hm);

        PostFxResources.loadShaders(rage::grcDevice::GetD3DDevice(), hm);

        OnDeviceReset();

        initialized = true;
    }

    static void NewPostFX() {
        IDirect3DPixelShader9* oldps = 0;
        IDirect3DVertexShader9* oldvs = 0;
        IDirect3DDevice9* pDevice = rage::grcDevice::GetD3DDevice();

        // get textures
        if(PostFxResources.mNormalRT->mD3DTexture == nullptr)
            return;
        PostFxResources.NormalTex = PostFxResources.mNormalRT->mD3DTexture;
        PostFxResources.DiffuseTex = PostFxResources.mDiffuseRT->mD3DTexture;
        PostFxResources.SpecularTex = PostFxResources.mSpecularRT->mD3DTexture;
        PostFxResources.StencilTex = PostFxResources.mStencilRT->mD3DTexture;
        PostFxResources.BloomTex = PostFxResources.mBloomRT->mD3DTexture;
        PostFxResources.CurrentLumTex = PostFxResources.mCurrentLum->mD3DTexture;
        PostFxResources.DepthTex = PostFxResources.mDepthRT->mD3DTexture;
        if (PostFxResources.mCascadeAtlasRT)
            PostFxResources.CascadeAtlasTex = PostFxResources.mCascadeAtlasRT->mD3DTexture;

        PostFxResources.FullScreenTex = PostFxResources.mFullScreenRT->mD3DTexture;
        PostFxResources.FullScreenTex2 = PostFxResources.mFullScreenRT2->mD3DTexture;
        PostFxResources.HalfScreenTex = PostFxResources.mHalfScreenRT->mD3DTexture;
        PostFxResources.pQuarterHDRTex = PostFxResources.mQuarterScreenRT->mD3DTexture;

        // get surfaces
        PostFxResources.FullScreenTex->GetSurfaceLevel(0, &PostFxResources.FullScreenSurface);
        if(PostFxResources.FullScreenTex2)
            PostFxResources.FullScreenTex2->GetSurfaceLevel(0, &PostFxResources.FullScreenSurface2);

        PostFxResources.FullScreenTex_temp1->mD3DTexture->GetSurfaceLevel(0, &PostFxResources.FullScreenSurface_temp1);
        PostFxResources.FullScreenTex_temp2->mD3DTexture->GetSurfaceLevel(0, &PostFxResources.FullScreenSurface_temp2);
        PostFxResources.edgesTex->mD3DTexture->GetSurfaceLevel(0, &PostFxResources.edgesSurf);
        PostFxResources.blendTex->mD3DTexture->GetSurfaceLevel(0, &PostFxResources.blendSurf);

        PostFxResources.HalfDepthStenciltex->mD3DTexture->GetSurfaceLevel(0, &PostFxResources.halfZStencilSurface);
        PostFxResources.FullScreenDownsampleTex->mD3DTexture->GetSurfaceLevel(0, &PostFxResources.FullScreenDownsampleSurf);
        PostFxResources.FullScreenDownsampleTex2->mD3DTexture->GetSurfaceLevel(0, &PostFxResources.FullScreenDownsampleSurf2);

        if (PostFxResources.CascadeAtlasTex)
            PostFxResources.CascadeAtlasTex->GetSurfaceLevel(0, &PostFxResources.CascadeAtlasSurf);
        PostFxResources.pShadowBlurTex1->mD3DTexture->GetSurfaceLevel(0, &PostFxResources.pShadowBlurSurf1);
        PostFxResources.pShadowBlurTex2->mD3DTexture->GetSurfaceLevel(0, &PostFxResources.pShadowBlurSurf2);

        //PostFxResources.stencilDownsampled->GetSurfaceLevel(0, &stencilDownsampledSurf       );


        pDevice->GetRenderTarget(0, &PostFxResources.backBuffer);
        pDevice->GetDepthStencilSurface(&PostFxResources.ppZStencilSurface);
        pDevice->GetPixelShader(&oldps);
        pDevice->GetVertexShader(&oldvs);

        saveRenderState();

        pDevice->SetPixelShaderConstantF(214, PostFxResources.AAparameters, 1);
        pDevice->SetSamplerState(2, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);

        // new 
        PostFx3(pDevice, oldps, oldvs);

        restoreRenderState();

        // restore
        pDevice->SetPixelShader(oldps);
        pDevice->SetVertexShader(oldvs);
        pDevice->SetRenderTarget(0, PostFxResources.backBuffer);

        // release
        SAFE_RELEASE(oldps);
        SAFE_RELEASE(oldvs);

        SAFE_RELEASE(last_VertexDecl);
        SAFE_RELEASE(last_VertexBuffer);

        SAFE_RELEASE(PostFxResources.backBuffer);
        SAFE_RELEASE(PostFxResources.FullScreenSurface_temp1);
        SAFE_RELEASE(PostFxResources.FullScreenSurface_temp2);
        SAFE_RELEASE(PostFxResources.FullScreenDownsampleSurf);
        SAFE_RELEASE(PostFxResources.FullScreenDownsampleSurf2);
        SAFE_RELEASE(PostFxResources.edgesSurf);
        SAFE_RELEASE(PostFxResources.blendSurf);
        SAFE_RELEASE(PostFxResources.CascadeAtlasSurf);

        SAFE_RELEASE(PostFxResources.FullScreenSurface);
        SAFE_RELEASE(PostFxResources.FullScreenSurface2);

        SAFE_RELEASE(PostFxResources.ppZStencilSurface);

        SAFE_RELEASE(PostFxResources.halfZStencilSurface);
        SAFE_RELEASE(PostFxResources.pShadowBlurSurf1);
        SAFE_RELEASE(PostFxResources.pShadowBlurSurf2);

        SAFE_RELEASE(PostFxResources.stencilDownsampledSurf);

        PostFxResources.renderTargetTex = nullptr;
        PostFxResources.textureRead = nullptr;
        PostFxResources.renderTargetSurf = nullptr;
        PostFxResources.surfaceRead = nullptr;
    }

    static HRESULT PostFx3(LPDIRECT3DDEVICE9 pDevice, IDirect3DPixelShader9* pShader, IDirect3DVertexShader9* vShader) {
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

        // save render state between post processing steps, 
        // in general each step expects the environment as the game 
        // leaves it at the time post processing is used, 
        // each step works individually from each other
        for(int i = 0; i < PostfxTextureCount; i++) {
            pDevice->GetTexture(i, &PostFxResources.prePostFx[i]);
            pDevice->GetSamplerState(i, D3DSAMP_MAGFILTER, &PostFxResources.Samplers[i]);
        }

        if(PostFxResources.blueNoiseVolume) {
            PostFxResources.blueNoiseVolume->GetLevelDesc(0, &volumeDescription);

            blueTimer++;
            blueTimer = blueTimer % (volumeDescription.Depth + 1);
            blueTimerVec4[0] = getWindowWidth() / float(volumeDescription.Width);
            blueTimerVec4[1] = getWindowHeight() / float(volumeDescription.Height);
            blueTimerVec4[2] = static_cast<float>(blueTimer) / float(volumeDescription.Depth);
            blueTimerVec4[3] = static_cast<float>(blueTimer);
        }
        vec4[0] = 1.f / static_cast<float>(getWindowWidth());
        vec4[1] = 1.f / static_cast<float>(getWindowHeight());
        vec4[2] = static_cast<float>(getWindowWidth());
        vec4[3] = static_cast<float>(getWindowHeight());


        pDevice->SetPixelShaderConstantI(5, SunShaftsSamplesa, 1);
        pDevice->SetPixelShaderConstantI(6, SunShaftsSamplesb, 1);
        pDevice->SetPixelShaderConstantI(7, SunShaftsSamplesc, 1);
        pDevice->SetPixelShaderConstantI(8, SunShaftsSamplesd, 1);
        pDevice->SetVertexShaderConstantF(24, vec4, 1);
        pDevice->SetPixelShaderConstantF(24, vec4, 1);
        pDevice->SetPixelShaderConstantF(96, PostFxResources.SunShafts_params1, 1);
        pDevice->SetPixelShaderConstantF(97, SunDirection, 1);
        pDevice->SetPixelShaderConstantF(98, SunColor, 1);
        pDevice->SetPixelShaderConstantF(99, PostFxResources.SunShafts_params2, 1);
        pDevice->SetPixelShaderConstantF(100, currGrcViewport->mWorldMatrix[0], 4);
        pDevice->SetPixelShaderConstantF(104, currGrcViewport->mWorldViewMatrix[0], 4);
        pDevice->SetPixelShaderConstantF(108, currGrcViewport->mWorldViewProjMatrix[0], 4);
        pDevice->SetPixelShaderConstantF(112, currGrcViewport->mViewInverseMatrix[0], 4);
        pDevice->SetPixelShaderConstantF(116, gShadowMatrix, 4);
        pDevice->SetPixelShaderConstantF(120, SunShaftsSamples2, 1);
        pDevice->SetPixelShaderConstantF(140, PostFxResources.BilateralDepthTreshold, 1);
        pDevice->SetPixelShaderConstantF(144, PostFxResources.NoiseSale, 1);
        pDevice->SetPixelShaderConstantF(214, PostFxResources.AAparameters, 1);
        pDevice->SetPixelShaderConstantF(218, blueTimerVec4, 1);

        pDevice->SetTexture(8, PostFxResources.HalfScreenTex);
        if(PostFxResources.blueNoiseVolume)
            pDevice->SetTexture(9, PostFxResources.blueNoiseVolume);

        // main postfx passes
        {
            if(PostFxResources.FullScreenTex_temp1 && PostFxResources.FullScreenTex /*&& PostFxResources.aoTex*/) {
                PostFxResources.renderTargetTex = PostFxResources.FullScreenTex_temp1->mD3DTexture;

                PostFxResources.textureRead = PostFxResources.FullScreenTex;
                PostFxResources.surfaceRead = PostFxResources.FullScreenSurface;

                PostFxResources.renderTargetTex = PostFxResources.FullScreenTex_temp1->mD3DTexture;
                PostFxResources.renderTargetSurf = PostFxResources.FullScreenSurface_temp1;

                // ready for new post processing?
                if(PostFxResources.backBuffer && PostFxResources.renderTargetSurf && PostFxResources.surfaceRead) {
                    if(PostFxResources.UseSSAO && PostFxResources.SSAO_gen_ps && PostFxResources.SSAO_blend_ps && PostFxResources.pShadowBlurSurf1 && PostFxResources.pShadowBlurSurf2) {

                        pDevice->SetPixelShader(PostFxResources.SSAO_gen_ps);
                        vec4[1] = PostFxResources.AoDistance;
                        pDevice->SetPixelShaderConstantF(211, vec4, 1);

                        //pDevice->SetTexture(2, 0);
                        pDevice->SetRenderTarget(0, PostFxResources.pShadowBlurSurf1);
                        //pDevice->SetTexture(2, PostFxResources.textureRead);
                        pDevice->DrawPrimitive(D3DPT_TRIANGLELIST, 0, 2);
                        pDevice->SetTexture(3, 0);

                        pDevice->SetPixelShader(PostFxResources.DeferredShadowBlurCircle_ps);
                        pDevice->SetRenderTarget(0, PostFxResources.pShadowBlurSurf2);
                        pDevice->SetTexture(11, PostFxResources.pShadowBlurTex1->mD3DTexture);
                        pDevice->DrawPrimitive(D3DPT_TRIANGLELIST, 0, 2);

                        pDevice->SetTexture(11, 0);

                        pDevice->SetPixelShader(PostFxResources.SSAO_blend_ps);
                        pDevice->SetRenderTarget(0, PostFxResources.renderTargetSurf);
                        pDevice->SetTexture(2, PostFxResources.textureRead);
                        pDevice->SetTexture(3, PostFxResources.pShadowBlurTex2->mD3DTexture);
                        pDevice->DrawPrimitive(D3DPT_TRIANGLELIST, 0, 2);
                        PostFxResources.swapbuffers();
                        pDevice->SetTexture(2, PostFxResources.textureRead);
                        pDevice->SetTexture(3, PostFxResources.prePostFx[3]);
                        pDevice->SetPixelShader(pShader);
                    }

                    if(PostFxResources.useStippleFilter && PostFxResources.stipple_filter_ps) {
                        pDevice->SetPixelShader(PostFxResources.stipple_filter_ps);
                        pDevice->SetRenderTarget(0, PostFxResources.renderTargetSurf);
                        pDevice->SetTexture(2, PostFxResources.textureRead);
                        pDevice->DrawPrimitive(D3DPT_TRIANGLELIST, 0, 2);
                        PostFxResources.swapbuffers();
                        pDevice->SetPixelShader(pShader);
                    }

                    // This implementation has some problems with cuts in some 
                    // places (when smoothing some diagonal lines there are pixels that break the smoothing), 
                    // I didn't find the reason, but using antialiasing.ixx it works almost perfectly despite being 
                    // the same implementation. Maybe it's the rendering order, IDK.
                    if(UsePostFxAA && UsePostFxAA->get() > FusionFixSettings.AntialiasingText.eMO_OFF) {

                        // FXAA
                        if((UsePostFxAA->get() == FusionFixSettings.AntialiasingText.eFXAA) && PostFxResources.FxaaPS) {
                            pDevice->GetRenderState(D3DRS_SRGBWRITEENABLE, &OldSRGB); // save srgb state
                            pDevice->SetPixelShader(PostFxResources.FxaaPS);

                            pDevice->SetRenderTarget(0, PostFxResources.renderTargetSurf);

                            pDevice->SetTexture(2, PostFxResources.textureRead);
                            hr = pDevice->DrawPrimitive(D3DPT_TRIANGLELIST, 0, 2);
                            PostFxResources.swapbuffers();
                            pDevice->SetSamplerState(2, D3DSAMP_SRGBTEXTURE, 0);
                            //pDevice->SetTexture(2, 0);
                            pDevice->SetPixelShader(pShader);
                            pDevice->SetRenderState(D3DRS_SRGBWRITEENABLE, OldSRGB);// restore srgb state
                        }

                        // SMAA
                        if(UsePostFxAA->get() >= FusionFixSettings.AntialiasingText.eSMAA &&
                           PostFxResources.SMAA_EdgeDetection && PostFxResources.SMAA_BlendingWeightsCalculation && PostFxResources.SMAA_NeighborhoodBlending &&
                           PostFxResources.SMAA_EdgeDetectionVS && PostFxResources.SMAA_BlendingWeightsCalculationVS && PostFxResources.SMAA_NeighborhoodBlendingVS &&
                           PostFxResources.SMAA_areaTex && PostFxResources.SMAA_searchTex && PostFxResources.edgesTex && PostFxResources.blendTex
                           ) {
                            DWORD oldSample = 0;

                            // SMAA_EdgeDetection
                            pDevice->GetRenderState(D3DRS_SRGBWRITEENABLE, &OldSRGB); // save srgb state
                            pDevice->SetRenderState(D3DRS_SRGBWRITEENABLE, 0);
                            pDevice->SetPixelShader(PostFxResources.SMAA_EdgeDetection);
                            pDevice->SetVertexShader(PostFxResources.SMAA_EdgeDetectionVS);
                            pDevice->SetRenderTarget(0, PostFxResources.edgesSurf);
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
                            pDevice->DrawPrimitive(D3DPT_TRIANGLELIST, 0, 2);
                            pDevice->SetSamplerState(0, D3DSAMP_SRGBTEXTURE, 0);
                            pDevice->SetTexture(0, 0);

                            // SMAA_BlendingWeightsCalculation
                            pDevice->SetPixelShader(PostFxResources.SMAA_BlendingWeightsCalculation);
                            pDevice->SetVertexShader(PostFxResources.SMAA_BlendingWeightsCalculationVS);
                            pDevice->SetRenderTarget(0, PostFxResources.blendSurf);
                            pDevice->SetTexture(1, PostFxResources.edgesTex->mD3DTexture);
                            pDevice->SetTexture(2, PostFxResources.SMAA_areaTex);
                            pDevice->SetTexture(3, PostFxResources.SMAA_searchTex);
                            pDevice->Clear(0, 0, D3DCLEAR_TARGET, 0, 0, 0);
                            pDevice->DrawPrimitive(D3DPT_TRIANGLELIST, 0, 2);

                            pDevice->SetRenderState(D3DRS_SRGBWRITEENABLE, OldSRGB);// restore srgb state

                            // SMAA_NeighborhoodBlending
                            pDevice->SetPixelShader(PostFxResources.SMAA_NeighborhoodBlending);
                            pDevice->SetVertexShader(PostFxResources.SMAA_NeighborhoodBlendingVS);
                            vec4[0] = static_cast<float>(UsePostFxAA->get());
                            vec4[1] = 1.0f;
                            vec4[2] = 3.0f;
                            vec4[3] = 4.0f;
                            pDevice->SetPixelShaderConstantF(5, vec4, 1);

                            pDevice->SetRenderTarget(0, PostFxResources.renderTargetSurf);
                            pDevice->SetSamplerState(2, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);

                            //pDevice->SetTexture(2,  textureRead);
                            pDevice->SetTexture(0, PostFxResources.textureRead);
                            pDevice->SetTexture(1, PostFxResources.edgesTex->mD3DTexture);
                            pDevice->SetTexture(4, PostFxResources.blendTex->mD3DTexture);
                            pDevice->SetRenderState(D3DRS_SRGBWRITEENABLE, 0);
                            pDevice->DrawPrimitive(D3DPT_TRIANGLELIST, 0, 2);
                            PostFxResources.swapbuffers();
                            pDevice->SetTexture(0, PostFxResources.prePostFx[0]);
                            pDevice->SetTexture(1, PostFxResources.prePostFx[1]);
                            pDevice->SetTexture(2, PostFxResources.textureRead);
                            pDevice->SetTexture(3, PostFxResources.prePostFx[3]);
                            pDevice->SetTexture(4, PostFxResources.prePostFx[4]);
                            pDevice->SetSamplerState(3, D3DSAMP_MAGFILTER, oldSample);
                            pDevice->SetPixelShader(pShader);
                            pDevice->SetVertexShader(vShader);
                        }
                    }

                    static auto dof = FusionFixSettings.GetRef("PREF_TCYC_DOF");
                    if(dof->get() > FusionFixSettings.DofText.eCutscenesOnly || (dof->get() == FusionFixSettings.DofText.eCutscenesOnly && Natives::HasCutsceneFinished())) {
                        if(PostFxResources.useDepthOfField > 0 && PostFxResources.dof_blur_ps && PostFxResources.dof_coc_ps) {
                            if(PostFxResources.ppZStencilSurface && PostFxResources.halfZStencilSurface && PostFxResources.FullScreenDownsampleSurf && PostFxResources.FullScreenDownsampleSurf2) {
                                pDevice->SetSamplerState(8, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
                                pDevice->SetSamplerState(8, D3DSAMP_ADDRESSU, D3DTADDRESS_MIRROR);
                                pDevice->SetSamplerState(8, D3DSAMP_ADDRESSV, D3DTADDRESS_MIRROR);
                                pDevice->SetSamplerState(2, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);

                                pDevice->SetDepthStencilSurface(PostFxResources.halfZStencilSurface);

                                pDevice->SetPixelShader(PostFxResources.dof_blur_ps);
                                pDevice->SetRenderTarget(0, PostFxResources.FullScreenDownsampleSurf);
                                pDevice->SetTexture(2, PostFxResources.textureRead);
                                pDevice->SetTexture(8, PostFxResources.HalfScreenTex);
                                pDevice->DrawPrimitive(D3DPT_TRIANGLELIST, 0, 2);

                                pDevice->SetPixelShader(PostFxResources.dof_coc_ps);
                                pDevice->SetDepthStencilSurface(PostFxResources.ppZStencilSurface);
                                pDevice->SetRenderTarget(0, PostFxResources.renderTargetSurf);
                                pDevice->SetTexture(2, PostFxResources.textureRead);
                                pDevice->SetTexture(8, PostFxResources.FullScreenDownsampleTex->mD3DTexture);
                                pDevice->DrawPrimitive(D3DPT_TRIANGLELIST, 0, 2);
                                PostFxResources.swapbuffers();
                                pDevice->SetTexture(8, PostFxResources.HalfScreenTex);

                                pDevice->SetPixelShader(pShader);
                            }
                        }
                    }

                    static auto refSunShafts = FusionFixSettings.GetRef("PREF_SUNSHAFTS");
                    if(refSunShafts->get()) {
                        // one pass half res
                        if(PostFxResources.UseSunShafts == 1 && PostFxResources.SSDownsampler_PS && PostFxResources.depth_of_field_tent_ps && PostFxResources.SunShafts_PS) {
                            if(PostFxResources.ppZStencilSurface && PostFxResources.halfZStencilSurface && PostFxResources.FullScreenDownsampleSurf && PostFxResources.FullScreenDownsampleSurf2) {
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
                                }

                                pDevice->SetDepthStencilSurface(PostFxResources.halfZStencilSurface);

                                pDevice->SetTexture(13, 0);
                                pDevice->SetTexture(11, 0);
                                pDevice->SetTexture(8, 0);

                                // crop arround sun position and save into a smaller texture
                                pDevice->SetPixelShader(PostFxResources.SSDownsampler_PS);
                                pDevice->SetRenderTarget(0, PostFxResources.FullScreenDownsampleSurf2);
                                pDevice->SetTexture(2, PostFxResources.textureRead);
                                pDevice->SetTexture(13, PostFxResources.DiffuseTex);
                                pDevice->DrawPrimitive(D3DPT_TRIANGLELIST, 0, 2);

                                // litle blur
                                pDevice->SetPixelShader(PostFxResources.depth_of_field_tent_ps);
                                pDevice->SetRenderTarget(0, PostFxResources.FullScreenDownsampleSurf);
                                pDevice->SetTexture(8, PostFxResources.FullScreenDownsampleTex2->mD3DTexture);
                                pDevice->DrawPrimitive(D3DPT_TRIANGLELIST, 0, 2);

                                // sample sun shafts
                                pDevice->SetPixelShader(PostFxResources.SunShafts_PS);
                                pDevice->SetRenderTarget(0, PostFxResources.FullScreenDownsampleSurf2);
                                pDevice->SetTexture(8, 0);
                                pDevice->SetTexture(11, PostFxResources.FullScreenDownsampleTex->mD3DTexture);
                                pDevice->DrawPrimitive(D3DPT_TRIANGLELIST, 0, 2);

                                pDevice->SetTexture(8, 0);
                                pDevice->SetTexture(11, 0);
                                pDevice->SetTexture(13, 0);

                                // blendadd sunshafts with screen
                                pDevice->SetPixelShader(PostFxResources.SSAdd_PS);
                                pDevice->SetDepthStencilSurface(PostFxResources.ppZStencilSurface);
                                pDevice->SetRenderTarget(0, PostFxResources.renderTargetSurf);
                                pDevice->SetTexture(2, PostFxResources.textureRead);
                                pDevice->SetTexture(8, PostFxResources.HalfScreenTex);
                                pDevice->SetTexture(11, PostFxResources.FullScreenDownsampleTex2->mD3DTexture);
                                if(PostFxResources.blueNoiseVolume)
                                    pDevice->SetTexture(9, PostFxResources.blueNoiseVolume);

                                pDevice->DrawPrimitive(D3DPT_TRIANGLELIST, 0, 2);
                                PostFxResources.swapbuffers();
                                pDevice->SetTexture(13, 0);
                                pDevice->SetTexture(11, 0);
                                pDevice->SetTexture(8, 0);

                                pDevice->SetPixelShader(pShader);
                            }
                        }

                        // 2 passes half res
                        if(PostFxResources.UseSunShafts == 2 && PostFxResources.SSDownsampler_PS && PostFxResources.depth_of_field_tent_ps && PostFxResources.SunShafts_PS && PostFxResources.SunShafts2_PS) {
                            if(PostFxResources.ppZStencilSurface && PostFxResources.halfZStencilSurface && PostFxResources.FullScreenDownsampleSurf && PostFxResources.FullScreenDownsampleSurf2) {
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

                                pDevice->SetDepthStencilSurface(PostFxResources.halfZStencilSurface);

                                pDevice->SetTexture(13, 0);
                                pDevice->SetTexture(11, 0);
                                pDevice->SetTexture(8, 0);

                                // crop arround sun position
                                pDevice->SetPixelShader(PostFxResources.SSDownsampler_PS);
                                pDevice->SetRenderTarget(0, PostFxResources.FullScreenDownsampleSurf2);
                                pDevice->SetTexture(2, PostFxResources.textureRead);
                                pDevice->SetTexture(13, PostFxResources.DiffuseTex);
                                pDevice->DrawPrimitive(D3DPT_TRIANGLELIST, 0, 2);

                                // litle blur
                                pDevice->SetPixelShader(PostFxResources.depth_of_field_tent_ps);
                                pDevice->SetRenderTarget(0, PostFxResources.FullScreenDownsampleSurf);
                                pDevice->SetTexture(8, PostFxResources.FullScreenDownsampleTex2->mD3DTexture);
                                pDevice->DrawPrimitive(D3DPT_TRIANGLELIST, 0, 2);

                                // sample sunshafts from a croped texture
                                pDevice->SetPixelShader(PostFxResources.SunShafts_PS);
                                pDevice->SetRenderTarget(0, PostFxResources.FullScreenDownsampleSurf2);
                                pDevice->SetTexture(8, 0);
                                pDevice->SetTexture(11, PostFxResources.FullScreenDownsampleTex->mD3DTexture);
                                pDevice->DrawPrimitive(D3DPT_TRIANGLELIST, 0, 2);

                                // second sunshafts pass
                                pDevice->SetPixelShader(PostFxResources.SunShafts2_PS);
                                pDevice->SetRenderTarget(0, PostFxResources.FullScreenDownsampleSurf);
                                pDevice->SetTexture(11, PostFxResources.FullScreenDownsampleTex2->mD3DTexture);
                                pDevice->DrawPrimitive(D3DPT_TRIANGLELIST, 0, 2);

                                pDevice->SetTexture(8, 0);
                                pDevice->SetTexture(11, 0);
                                pDevice->SetTexture(13, 0);

                                // add sunshafts on screen
                                pDevice->SetPixelShader(PostFxResources.SSAdd_PS);
                                pDevice->SetDepthStencilSurface(PostFxResources.ppZStencilSurface);
                                pDevice->SetRenderTarget(0, PostFxResources.renderTargetSurf);
                                pDevice->SetTexture(2, PostFxResources.textureRead);
                                pDevice->SetTexture(8, PostFxResources.HalfScreenTex);
                                pDevice->SetTexture(11, PostFxResources.FullScreenDownsampleTex->mD3DTexture);
                                if(PostFxResources.blueNoiseVolume)
                                    pDevice->SetTexture(9, PostFxResources.blueNoiseVolume);

                                pDevice->DrawPrimitive(D3DPT_TRIANGLELIST, 0, 2);
                                PostFxResources.swapbuffers();
                                pDevice->SetTexture(13, 0);
                                pDevice->SetTexture(11, 0);
                                pDevice->SetTexture(8, 0);

                                pDevice->SetPixelShader(pShader);
                            }
                        }

                        // full screen
                        if(PostFxResources.UseSunShafts == 3 && PostFxResources.depth_of_field_tent_ps && PostFxResources.SunShafts3_PS) {
                            if(PostFxResources.ppZStencilSurface && PostFxResources.halfZStencilSurface && PostFxResources.FullScreenDownsampleSurf && PostFxResources.FullScreenDownsampleSurf2 && PostFxResources.FullScreenSurface_temp2) {
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

                                // premultiply textures 
                                pDevice->SetPixelShader(PostFxResources.SSDownsampler2_PS);
                                pDevice->SetRenderTarget(0, PostFxResources.FullScreenSurface_temp2);
                                pDevice->SetTexture(2, PostFxResources.textureRead);
                                pDevice->SetTexture(13, PostFxResources.DiffuseTex);
                                pDevice->DrawPrimitive(D3DPT_TRIANGLELIST, 0, 2);

                                // sunshafts at full screen and add to screen
                                pDevice->SetPixelShader(PostFxResources.SunShafts3_PS);
                                pDevice->SetRenderTarget(0, PostFxResources.renderTargetSurf);

                                //pDevice->SetTexture(8,  HalfScreenTex);
                                pDevice->SetTexture(11, PostFxResources.FullScreenTex_temp2->mD3DTexture);
                                if(PostFxResources.blueNoiseVolume)
                                    pDevice->SetTexture(9, PostFxResources.blueNoiseVolume);

                                //pDevice->SetTexture(11,  FullScreenDownsampleTex);
                                pDevice->DrawPrimitive(D3DPT_TRIANGLELIST, 0, 2);
                                PostFxResources.swapbuffers();

                                pDevice->SetPixelShader(pShader);
                            }
                        }

                        // full screen 2 passes
                        if(PostFxResources.UseSunShafts == 4 && PostFxResources.depth_of_field_tent_ps && PostFxResources.SunShafts4_PS) {
                            if(PostFxResources.ppZStencilSurface && PostFxResources.halfZStencilSurface && PostFxResources.FullScreenDownsampleSurf && PostFxResources.FullScreenDownsampleSurf2) {
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

                                pDevice->SetRenderTarget(0, PostFxResources.renderTargetSurf);
                                pDevice->SetTexture(2, PostFxResources.textureRead);

                                // sunshafts full screen
                                pDevice->SetPixelShader(PostFxResources.SunShafts4_PS);

                                pDevice->SetTexture(8, PostFxResources.HalfScreenTex);
                                if(PostFxResources.blueNoiseVolume)
                                    pDevice->SetTexture(9, PostFxResources.blueNoiseVolume);

                                pDevice->SetTexture(11, PostFxResources.FullScreenDownsampleTex->mD3DTexture);
                                pDevice->SetTexture(13, PostFxResources.DiffuseTex);
                                pDevice->DrawPrimitive(D3DPT_TRIANGLELIST, 0, 2);
                                PostFxResources.swapbuffers();

                                pDevice->SetPixelShader(pShader);
                            }
                        }
                    }

                    // game postfx
                    {
                        for(int i = 0; i < 4; i++) {
                            pDevice->SetTexture(i, PostFxResources.prePostFx[i]);
                            pDevice->SetSamplerState(i, D3DSAMP_MAGFILTER, PostFxResources.Samplers[i]);
                        }

                        pDevice->SetRenderTarget(0, PostFxResources.backBuffer);
                        pDevice->SetTexture(9, PostFxResources.blueNoiseVolume);
                        pDevice->SetTexture(2, PostFxResources.textureRead);
                        pDevice->Clear(0, 0, D3DCLEAR_TARGET, 0, 0, 0);

                        pDevice->SetPixelShader(pShader);
                        pDevice->SetVertexShader(vShader);
                        //hr = pDevice->DrawPrimitive(D3DPT_TRIANGLELIST, 0, 2);
                        hbDrawPrimitivePostFX.fun();
                        pDevice->SetTexture(9, 0);
                    }

                    for(int i = 0; i < PostfxTextureCount; i++) {
                        pDevice->SetTexture(i, PostFxResources.prePostFx[i]);
                        pDevice->SetSamplerState(i, D3DSAMP_MAGFILTER, PostFxResources.Samplers[i]);
                        SAFE_RELEASE(PostFxResources.prePostFx[i]);
                    }
                    return S_OK;
                }

                for(int i = 0; i < PostfxTextureCount; i++) {
                    pDevice->SetTexture(i, PostFxResources.prePostFx[i]);
                    pDevice->SetSamplerState(i, D3DSAMP_MAGFILTER, PostFxResources.Samplers[i]);
                    SAFE_RELEASE(PostFxResources.prePostFx[i]);
                }
                return S_FALSE;
            }
        }

        for(int i = 0; i < PostfxTextureCount; i++) {
            pDevice->SetTexture(i, PostFxResources.prePostFx[i]);
            pDevice->SetSamplerState(i, D3DSAMP_MAGFILTER, PostFxResources.Samplers[i]);
            SAFE_RELEASE(PostFxResources.prePostFx[i]);
        }
        return S_FALSE;
    }

    static inline thread_local bool bInsteadDrawPrimitivePostFX = false;
    static inline injector::hook_back<void(__fastcall*)(void*, void*, int, int, int)> hbDrawCallPostFX;
    static void __fastcall DrawCallPostFX(void* _this, void* edx, int a2, int a3, int a4) {
        bInsteadDrawPrimitivePostFX = true;
        hbDrawCallPostFX.fun(_this, edx, a2, a3, a4);
        bInsteadDrawPrimitivePostFX = false;
    }

    static inline injector::hook_back<void(__stdcall*)()> hbDrawPrimitivePostFX;
    static void __stdcall DrawPrimitivePostFX() {
        if (bInsteadDrawPrimitivePostFX)
        {
            bInsteadDrawPrimitivePostFX = false;
            Init();
            NewPostFX();
        }
        else
        {
            hbDrawPrimitivePostFX.fun();
        }
    }

    static inline injector::hook_back<int(__fastcall*)(int, void*, int, int, char, char, int, char)> hbDrawSkyHook;
    static int __fastcall DrawSky(int _this, void* edx, int a2, int a3, char a4, char a5, int a6, char a7)
    {
        auto pDevice = rage::grcDevice::GetD3DDevice();
        IDirect3DPixelShader9* pShader = nullptr;
        HRESULT hr = S_FALSE;
        pDevice->GetPixelShader(&pShader);
        // atmoscat clouds
        if (PostFxResources.DiffuseTex != nullptr)
        {
            IDirect3DSurface9* DiffuseSurf = nullptr;
            PostFxResources.DiffuseTex->GetSurfaceLevel(0, &DiffuseSurf);
            if (DiffuseSurf)
            {
                IDirect3DSurface9* oldRenderTarget1 = 0;
                pDevice->GetRenderTarget(1, &oldRenderTarget1);
                pDevice->SetRenderTarget(1, DiffuseSurf);
                pDevice->SetPixelShader(PostFxResources.SSDiffuseCloudsGen_PS);
                hr = hbDrawSkyHook.fun(_this, edx, a2, a3, a4, a5, a6, a7);
                pDevice->SetPixelShader(pShader);
                pDevice->SetRenderTarget(1, oldRenderTarget1);
                hr = hbDrawSkyHook.fun(_this, edx, a2, a3, a4, a5, a6, a7);
                SAFE_RELEASE(oldRenderTarget1);
                SAFE_RELEASE(DiffuseSurf);
                SAFE_RELEASE(pShader);
                return hr;
            }
            SAFE_RELEASE(DiffuseSurf);
            SAFE_RELEASE(pShader);
        }
        return hbDrawSkyHook.fun(_this, edx, a2, a3, a4, a5, a6, a7);
    }

public:
    PostFX()
    {
        FusionFix::onInitEventAsync() += []()
        {
            if (GetD3DX9_43DLL())
            {
                PostFxResources.Readini();
            
                //if(PostFxResources.EnablePostfx)
                {
                    auto pattern = find_pattern("E8 ? ? ? ? 8B 4F 60 E8 ? ? ? ? 8B 4F 60", "E8 ? ? ? ? 8B 4F 60 E8 ? ? ? ? 8B 4F 60");
                    hbDrawPrimitivePostFX.fun = injector::MakeCALL(pattern.get_first(0), DrawPrimitivePostFX).get();
            
                    pattern = find_pattern("E8 ? ? ? ? 8D 44 24 60 50 8B CF E8 ? ? ? ? 8D 84 24", "E8 ? ? ? ? 8D 44 24 40 50 8B CE E8 ? ? ? ? 8D 8C 24");
                    hbDrawSkyHook.fun = injector::MakeCALL(pattern.get_first(0), DrawSky).get();
            
                    pattern = find_pattern("E8 ? ? ? ? 6A 0A FF B7", "E8 ? ? ? ? 8B 8E ? ? ? ? 8B 56 10");
                    hbDrawCallPostFX.fun = injector::MakeCALL(pattern.get_first(0), DrawCallPostFX).get();
                }
            }
        };
    }
} PostFX;
