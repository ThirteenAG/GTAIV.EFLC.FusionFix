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

#define IDR_DeferredShadowBlur                   126
#define IDR_SunShafts_PS                         127
#define IDR_CascadeAtlasGen                      128

#define IDR_Blit_PS                              129

#define IDR_SSDraw_PS_compiled                   2127
#define IDR_SSPrepass_PS_compiled                2128
#define IDR_SSAdd_PS_compiled                    2129
#define IDR_FxaaPS_compiled                      2101
#define IDR_SMAA_EdgeDetection_compiled          2102
#define IDR_SMAA_BlendingWeightsCalculation_compiled 2103
#define IDR_SMAA_NeighborhoodBlending_compiled   2104
#define IDR_SMAA_EdgeDetectionVS_compiled        2105
#define IDR_SMAA_BlendingWeightsCalculationVS_compiled 2106
#define IDR_SMAA_NeighborhoodBlendingVS_compiled 2107

#ifndef SAFE_RELEASE
#define SAFE_RELEASE(p) { if (p) { (p)->Release(); (p)=NULL; } }
#endif

std::optional<std::reference_wrapper<int32_t>> UsePostFxAA;

class PostFxResource {
public:

    // --------- load --------- 
    IDirect3DTexture9* SMAA_areaTex = nullptr; // loaded from file
    IDirect3DTexture9* SMAA_searchTex = nullptr; // loaded from file

    // --------- game textures --------- 
    IDirect3DTexture9* FullScreenTex = nullptr; // game hdr texture
    IDirect3DTexture9* HalfScreenTex = nullptr; // game half res screen texture
    // IDirect3DTexture9* pQuarterHDRTex = nullptr; //  game 1/4 res screen texture
    // IDirect3DTexture9* CascadeAtlasTex = nullptr;
    // IDirect3DSurface9* CascadeAtlasSurf = nullptr;

    // IDirect3DTexture9* NormalTex = nullptr;
    IDirect3DTexture9* DiffuseTex = nullptr;
    // IDirect3DTexture9* SpecularTex = nullptr;
    // IDirect3DTexture9* DepthTex = nullptr;
    // IDirect3DTexture9* StencilTex = nullptr;
    // IDirect3DTexture9* BloomTex = nullptr;
    // IDirect3DTexture9* CurrentLumTex = nullptr;


    //------- full screen ---------
    // postfx textures created
    rage::grcRenderTargetPC* FullScreenTex_temp1 = nullptr; // main temp texture
    rage::grcRenderTargetPC* FullScreenTex_temp2 = nullptr; // main temp texture

    //rage::grcRenderTargetPC* pShadowBlurTex1 = nullptr; // main shadow temp texture
    //rage::grcRenderTargetPC* pShadowBlurTex2 = nullptr; // main shadow temp texture

    // smaa textures
    rage::grcRenderTargetPC* edgesTex = nullptr; // smaa gen
    rage::grcRenderTargetPC* blendTex = nullptr; // smaa gen

    // temp set and used in postfx
    IDirect3DTexture9* renderTargetTex = nullptr;
    IDirect3DTexture9* textureRead = nullptr;
    IDirect3DSurface9* renderTargetSurf = nullptr;
    IDirect3DSurface9* surfaceRead = nullptr;

    // Pre alpha pass depth texture copy
    rage::grcRenderTargetPC* PreAlphaDepthCopyRT = nullptr;

    //-------- half resolution screen --------------
    rage::grcRenderTargetPC* FullScreenDownsampleTex = nullptr; // main downsampled texture
    rage::grcRenderTargetPC* FullScreenDownsampleTex2 = nullptr; // main downsampled texture


    // game render targets
    // rage::grcRenderTargetPC* mSpecularAoRT = nullptr;
    // rage::grcRenderTargetPC* mNormalRT = nullptr;
    rage::grcRenderTargetPC* mDiffuseRT = nullptr;
    // rage::grcRenderTargetPC* mSpecularRT = nullptr;
    // rage::grcRenderTargetPC* mDepthRT = nullptr;
    // rage::grcRenderTargetPC* mStencilRT = nullptr;
    rage::grcRenderTargetPC* mFullScreenRT = nullptr;
    // rage::grcRenderTargetPC* mFullScreenRT2 = nullptr;
    rage::grcRenderTargetPC* mHalfScreenRT = nullptr;
    // rage::grcRenderTargetPC* mCascadeAtlasRT = nullptr;
    // rage::grcRenderTargetPC* mQuarterScreenRT = nullptr;
    // rage::grcRenderTargetPC* mBloomRT = nullptr;
    // rage::grcRenderTargetPC* mCurrentLum = nullptr;


    // surfaces
    IDirect3DSurface9* FullScreenSurface = nullptr;
    IDirect3DSurface9* FullScreenSurface_temp1 = nullptr;
    IDirect3DSurface9* FullScreenSurface_temp2 = nullptr;
    IDirect3DSurface9* FullScreenDownsampleSurf = nullptr;
    IDirect3DSurface9* FullScreenDownsampleSurf2 = nullptr;
    IDirect3DSurface9* backBuffer = nullptr;
    IDirect3DSurface9* edgesSurf = nullptr;
    IDirect3DSurface9* blendSurf = nullptr;
    //IDirect3DSurface9* pShadowBlurSurf1 = nullptr;
    //IDirect3DSurface9* pShadowBlurSurf2 = nullptr;

    IDirect3DSurface9* PreAlphaDepthSurface = nullptr;

    // shaders
    IDirect3DPixelShader9* FxaaPS = nullptr;

    IDirect3DPixelShader9* SSDraw_PS = nullptr;
    IDirect3DPixelShader9* SSAdd_PS = nullptr;
    IDirect3DPixelShader9* SSPrepass_PS = nullptr;
    // IDirect3DPixelShader9* SSAO_gen_ps = nullptr;
    // IDirect3DPixelShader9* SSAO_blend_ps = nullptr;


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

    IDirect3DPixelShader9* Blit_PS = nullptr;

    //IDirect3DPixelShader9* DeferredShadowGen_ps = nullptr;
    //IDirect3DPixelShader9* DeferredShadowBlurH_ps = nullptr;
    //IDirect3DPixelShader9* DeferredShadowBlurV_ps = nullptr;
    //IDirect3DPixelShader9* DeferredShadowBlurCircle_ps = nullptr;
    // IDirect3DPixelShader9* deferred_lighting_PS1 = nullptr;
    // IDirect3DPixelShader9* deferred_lighting_PS2 = nullptr;

    // IDirect3DPixelShader9* CascadeAtlasGen = nullptr;

    //std::unordered_map<IDirect3DPixelShader9*, int> ShaderListPS;
    //std::unordered_map<IDirect3DVertexShader9*, int> ShaderListVS;

    bool EnablePostfx = false;

    // 0 off, 1 horizontal, 2 vertical, 3 horizontal e vertical.
    //int useScreenSpaceShadowsBlur = 3;
    bool useStippleFilter = true;

    bool shadersLoaded = false;

    bool bEnablePreAlphaDepth = false;

    bool loadShaders(LPDIRECT3DDEVICE9 pDevice, HMODULE hm)
    {
        ID3DXBuffer* bf1 = nullptr;
        ID3DXBuffer* bf2 = nullptr;
        ID3DXConstantTable* ppConstantTable = nullptr;

        // Lambda helper to load compiled shader bytecode from resource
        auto loadCompiledShader = [&](int resourceID, auto& shader) -> bool {
            HRSRC hRes = FindResourceW(hm, MAKEINTRESOURCEW(resourceID), RT_RCDATA);
            if (!hRes) return false;
            HGLOBAL hGlob = LoadResource(hm, hRes);
            if (!hGlob) return false;
            void* buffer = LockResource(hGlob);
            if (!buffer) return false;
            using ShaderType = std::remove_reference_t<decltype(shader)>;
            if constexpr (std::is_same_v<ShaderType, IDirect3DPixelShader9*>)
            {
                return pDevice->CreatePixelShader((DWORD*)buffer, &shader) == S_OK && shader;
            }
            else if constexpr (std::is_same_v<ShaderType, IDirect3DVertexShader9*>)
            {
                return pDevice->CreateVertexShader((DWORD*)buffer, &shader) == S_OK && shader;
            }
            return false;
        };

        //asm
        if (!dof_blur_ps)
        {
            if (D3DXAssembleShaderFromResourceW(hm, MAKEINTRESOURCEW(IDR_dof_blur_ps), NULL, NULL, 0, &bf1, &bf2) == S_OK)
            {
                if (pDevice->CreatePixelShader((DWORD*)bf1->GetBufferPointer(), &dof_blur_ps) != S_OK || !dof_blur_ps)
                    SAFE_RELEASE(dof_blur_ps);
                SAFE_RELEASE(bf1);
                SAFE_RELEASE(bf2);
            }
        }

        if (!dof_coc_ps)
        {
            if (D3DXAssembleShaderFromResourceW(hm, MAKEINTRESOURCEW(IDR_dof_coc_ps), NULL, NULL, 0, &bf1, &bf2) == S_OK)
            {
                if (pDevice->CreatePixelShader((DWORD*)bf1->GetBufferPointer(), &dof_coc_ps) != S_OK || !dof_coc_ps)
                    SAFE_RELEASE(dof_coc_ps);
                SAFE_RELEASE(bf1);
                SAFE_RELEASE(bf2);
            }
        }

        if (!depth_of_field_tent_ps)
        {
            if (D3DXAssembleShaderFromResourceW(hm, MAKEINTRESOURCEW(IDR_depth_of_field_tent_ps), NULL, NULL, 0, &bf1, &bf2) == S_OK)
            {
                if (pDevice->CreatePixelShader((DWORD*)bf1->GetBufferPointer(), &depth_of_field_tent_ps) != S_OK || !depth_of_field_tent_ps)
                    SAFE_RELEASE(depth_of_field_tent_ps);
                SAFE_RELEASE(bf1);
                SAFE_RELEASE(bf2);
            }
        }

        if (!stipple_filter_ps)
        {
            if (D3DXAssembleShaderFromResourceW(hm, MAKEINTRESOURCEW(IDR_stipple_filter_ps), NULL, NULL, 0, &bf1, &bf2) == S_OK)
            {
                if (pDevice->CreatePixelShader((DWORD*)bf1->GetBufferPointer(), &stipple_filter_ps) != S_OK || !stipple_filter_ps)
                    SAFE_RELEASE(stipple_filter_ps);
                SAFE_RELEASE(bf1);
                SAFE_RELEASE(bf2);
            }
        }

        //hlsl
        if (!SSDraw_PS)
        {
            if (D3DXCompileShaderFromResourceW(hm, MAKEINTRESOURCEW(IDR_SunShafts_PS), NULL, NULL, "SSDraw", "ps_3_0", 0, &bf1, &bf2, &ppConstantTable) == S_OK)
            {
                if (pDevice->CreatePixelShader((DWORD*)bf1->GetBufferPointer(), &SSDraw_PS) != S_OK || !SSDraw_PS)
                    SAFE_RELEASE(SSDraw_PS);
            }
            else
            {
                loadCompiledShader(IDR_SSDraw_PS_compiled, SSDraw_PS);
            }
            SAFE_RELEASE(bf1);
            SAFE_RELEASE(bf2);
            SAFE_RELEASE(ppConstantTable);
        }

        if (!SSPrepass_PS)
        {
            if (D3DXCompileShaderFromResourceW(hm, MAKEINTRESOURCEW(IDR_SunShafts_PS), NULL, NULL, "SSPrepass", "ps_3_0", 0, &bf1, &bf2, &ppConstantTable) == S_OK)
            {
                if (pDevice->CreatePixelShader((DWORD*)bf1->GetBufferPointer(), &SSPrepass_PS) != S_OK || !SSPrepass_PS)
                    SAFE_RELEASE(SSPrepass_PS);
            }
            else
            {
                loadCompiledShader(IDR_SSPrepass_PS_compiled, SSPrepass_PS);
            }
            SAFE_RELEASE(bf1);
            SAFE_RELEASE(bf2);
            SAFE_RELEASE(ppConstantTable);
        }

        if (!SSAdd_PS)
        {
            if (D3DXCompileShaderFromResourceW(hm, MAKEINTRESOURCEW(IDR_SunShafts_PS), NULL, NULL, "SSAdd", "ps_3_0", 0, &bf1, &bf2, &ppConstantTable) == S_OK)
            {
                if (pDevice->CreatePixelShader((DWORD*)bf1->GetBufferPointer(), &SSAdd_PS) != S_OK || !SSAdd_PS)
                    SAFE_RELEASE(SSAdd_PS);
            }
            else
            {
                loadCompiledShader(IDR_SSAdd_PS_compiled, SSAdd_PS);
            }
            SAFE_RELEASE(bf1);
            SAFE_RELEASE(bf2);
            SAFE_RELEASE(ppConstantTable);
        }

        if (!FxaaPS)
        {
            if (D3DXCompileShaderFromResourceW(hm, MAKEINTRESOURCEW(IDR_FXAA), NULL, NULL, "ApplyFXAA", "ps_3_0", 0, &bf1, &bf2, &ppConstantTable) == S_OK)
            {
                if (pDevice->CreatePixelShader((DWORD*)bf1->GetBufferPointer(), &FxaaPS) != S_OK || !FxaaPS)
                    SAFE_RELEASE(FxaaPS);
            }
            else
            {
                loadCompiledShader(IDR_FxaaPS_compiled, FxaaPS);
            }
            SAFE_RELEASE(bf1);
            SAFE_RELEASE(bf2);
            SAFE_RELEASE(ppConstantTable);
        }

        if (!SMAA_EdgeDetection)
        {
            if (D3DXCompileShaderFromResourceW(hm, MAKEINTRESOURCEW(IDR_SMAA), NULL, NULL, "DX9_SMAALumaEdgeDetectionPS", "ps_3_0", 0, &bf1, &bf2, &ppConstantTable) == S_OK)
            {
                if (pDevice->CreatePixelShader((DWORD*)bf1->GetBufferPointer(), &SMAA_EdgeDetection) != S_OK || !SMAA_EdgeDetection)
                    SAFE_RELEASE(SMAA_EdgeDetection);
            }
            else
            {
                loadCompiledShader(IDR_SMAA_EdgeDetection_compiled, SMAA_EdgeDetection);
            }
            SAFE_RELEASE(bf1);
            SAFE_RELEASE(bf2);
            SAFE_RELEASE(ppConstantTable);
        }

        if (!SMAA_BlendingWeightsCalculation)
        {
            if (D3DXCompileShaderFromResourceW(hm, MAKEINTRESOURCEW(IDR_SMAA), NULL, NULL, "DX9_SMAABlendingWeightCalculationPS", "ps_3_0", 0, &bf1, &bf2, &ppConstantTable) == S_OK)
            {
                if (pDevice->CreatePixelShader((DWORD*)bf1->GetBufferPointer(), &SMAA_BlendingWeightsCalculation) != S_OK || !SMAA_BlendingWeightsCalculation)
                    SAFE_RELEASE(SMAA_BlendingWeightsCalculation);
            }
            else
            {
                loadCompiledShader(IDR_SMAA_BlendingWeightsCalculation_compiled, SMAA_BlendingWeightsCalculation);
            }
            SAFE_RELEASE(bf1);
            SAFE_RELEASE(bf2);
            SAFE_RELEASE(ppConstantTable);
        }

        if (!SMAA_NeighborhoodBlending)
        {
            if (D3DXCompileShaderFromResourceW(hm, MAKEINTRESOURCEW(IDR_SMAA), NULL, NULL, "DX9_SMAANeighborhoodBlendingPS", "ps_3_0", 0, &bf1, &bf2, &ppConstantTable) == S_OK)
            {
                if (pDevice->CreatePixelShader((DWORD*)bf1->GetBufferPointer(), &SMAA_NeighborhoodBlending) != S_OK || !SMAA_NeighborhoodBlending)
                    SAFE_RELEASE(SMAA_NeighborhoodBlending);
            }
            else
            {
                loadCompiledShader(IDR_SMAA_NeighborhoodBlending_compiled, SMAA_NeighborhoodBlending);
            }
            SAFE_RELEASE(bf1);
            SAFE_RELEASE(bf2);
            SAFE_RELEASE(ppConstantTable);
        }

        if (!SMAA_EdgeDetectionVS)
        {
            if (D3DXCompileShaderFromResourceW(hm, MAKEINTRESOURCEW(IDR_SMAA), NULL, NULL, "DX9_SMAAEdgeDetectionVS", "vs_3_0", 0, &bf1, &bf2, &ppConstantTable) == S_OK)
            {
                if (pDevice->CreateVertexShader((DWORD*)bf1->GetBufferPointer(), &SMAA_EdgeDetectionVS) != S_OK || !SMAA_EdgeDetectionVS)
                    SAFE_RELEASE(SMAA_EdgeDetectionVS);
            }
            else
            {
                loadCompiledShader(IDR_SMAA_EdgeDetectionVS_compiled, SMAA_EdgeDetectionVS);
            }
            SAFE_RELEASE(bf1);
            SAFE_RELEASE(bf2);
            SAFE_RELEASE(ppConstantTable);
        }

        if (!SMAA_BlendingWeightsCalculationVS)
        {
            if (D3DXCompileShaderFromResourceW(hm, MAKEINTRESOURCEW(IDR_SMAA), NULL, NULL, "DX9_SMAABlendingWeightCalculationVS", "vs_3_0", 0, &bf1, &bf2, &ppConstantTable) == S_OK)
            {
                if (pDevice->CreateVertexShader((DWORD*)bf1->GetBufferPointer(), &SMAA_BlendingWeightsCalculationVS) != S_OK || !SMAA_BlendingWeightsCalculationVS)
                    SAFE_RELEASE(SMAA_BlendingWeightsCalculationVS);
            }
            else
            {
                loadCompiledShader(IDR_SMAA_BlendingWeightsCalculationVS_compiled, SMAA_BlendingWeightsCalculationVS);
            }
            SAFE_RELEASE(bf1);
            SAFE_RELEASE(bf2);
            SAFE_RELEASE(ppConstantTable);
        }

        if (!SMAA_NeighborhoodBlendingVS)
        {
            if (D3DXCompileShaderFromResourceW(hm, MAKEINTRESOURCEW(IDR_SMAA), NULL, NULL, "DX9_SMAANeighborhoodBlendingVS", "vs_3_0", 0, &bf1, &bf2, &ppConstantTable) == S_OK)
            {
                if (pDevice->CreateVertexShader((DWORD*)bf1->GetBufferPointer(), &SMAA_NeighborhoodBlendingVS) != S_OK || !SMAA_NeighborhoodBlendingVS)
                    SAFE_RELEASE(SMAA_NeighborhoodBlendingVS);
            }
            else
            {
                loadCompiledShader(IDR_SMAA_NeighborhoodBlendingVS_compiled, SMAA_NeighborhoodBlendingVS);
            }
            SAFE_RELEASE(bf1);
            SAFE_RELEASE(bf2);
            SAFE_RELEASE(ppConstantTable);
        }

        if (!Blit_PS)
        {
            if (D3DXAssembleShaderFromResourceW(hm, MAKEINTRESOURCEW(IDR_Blit_PS), NULL, NULL, 0, &bf1, &bf2) == S_OK)
            {
                if (pDevice->CreatePixelShader((DWORD*)bf1->GetBufferPointer(), &Blit_PS) != S_OK || !Blit_PS)
                    SAFE_RELEASE(Blit_PS);
                SAFE_RELEASE(bf1);
                SAFE_RELEASE(bf2);
            }
        }

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
        // NormalTex = nullptr;
        DiffuseTex = nullptr;
        // SpecularTex = nullptr;
        // StencilTex = nullptr;
        // BloomTex = nullptr;
        // CurrentLumTex = nullptr;
        // DepthTex = nullptr;
        // CascadeAtlasTex = nullptr;

        FullScreenTex = nullptr;
        HalfScreenTex = nullptr;
        // pQuarterHDRTex = nullptr;

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

        if (PreAlphaDepthCopyRT)
        {
            PreAlphaDepthCopyRT->Destroy();
            PreAlphaDepthCopyRT = nullptr;
        }
        //if (pShadowBlurTex1)
        //{
        //    pShadowBlurTex1->Destroy();
        //    pShadowBlurTex1 = nullptr;
        //}
        //
        //if (pShadowBlurTex2)
        //{
        //    pShadowBlurTex2->Destroy();
        //    pShadowBlurTex2 = nullptr;
        //}
    }

    bool ShadersFinishedLoading() {
        if(FxaaPS && dof_blur_ps && dof_coc_ps && depth_of_field_tent_ps && stipple_filter_ps
           && SSDraw_PS && SSPrepass_PS && SSAdd_PS
           && SMAA_EdgeDetection && SMAA_BlendingWeightsCalculation && SMAA_NeighborhoodBlending
           && SMAA_EdgeDetectionVS && SMAA_BlendingWeightsCalculationVS && SMAA_NeighborhoodBlendingVS
           && Blit_PS)
           // DeferredShadowGen_ps && deferred_lighting_PS1 && deferred_lighting_PS2 && SSAO_gen_ps && SSAO_blend_ps && DeferredShadowBlurH_ps && DeferredShadowBlurV_ps && DeferredShadowBlurCircle_ps
            return true;

        return false;
    }

    void Readini() {
        CIniReader iniReader("");
        EnablePostfx = iniReader.ReadInteger("SRF", "EnablePostfx", 1);

        useStippleFilter = iniReader.ReadInteger("SRF", "StippleFilter", 1) != 0;

        bEnablePreAlphaDepth = iniReader.ReadInteger("POSTFX", "EnablePreAlphaDepth", 1) != 0;

        // 0 off, 1 horizontal, 2 vertical, 3 horizontal e vertical.
        //useScreenSpaceShadowsBlur = iniReader.ReadInteger("SRF", "ScreenSpaceShadowsBlur", 0);
        //useHardwareBilinearSampling = iniReader.ReadInteger("SRF", "NewShadowAtlas", 0) != 0;
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

        FullScreenTex_temp1 = CreateEmptyRT("FullScreenTex_temp1", 3, Width, Height, 64, &desc);
        
        desc.mFormat = rage::GRCFMT_A8R8G8B8;
        FullScreenTex_temp2 = CreateEmptyRT("FullScreenTex_temp2", 3, Width, Height, 32, &desc);

        //desc.mFormat = rage::GRCFMT_G16R16F;
        // 
        //pShadowBlurTex1 = CreateEmptyRT("pShadowBlurTex1", 3, Width, Height, 32, &desc);
        //pShadowBlurTex2 = CreateEmptyRT("pShadowBlurTex2", 3, Width, Height, 32, &desc);

        desc.mFormat = rage::GRCFMT_X8R8G8B8;

        edgesTex = CreateEmptyRT("edgesTex", 3, Width, Height, 32, &desc);

        desc.mFormat = rage::GRCFMT_A8R8G8B8;

        blendTex = CreateEmptyRT("blendTex", 3, Width, Height, 32, &desc);

        desc.mFormat = rage::GRCFMT_A16B16G16R16F;

        FullScreenDownsampleTex = CreateEmptyRT("FullScreenDownsampleTex", 3, Width / 2, Height / 2, 64, &desc);
        FullScreenDownsampleTex2 = CreateEmptyRT("FullScreenDownsampleTex2", 3, Width / 2, Height / 2, 64, &desc);

        if (bEnablePreAlphaDepth)
        {
            desc.mFormat = rage::GRCFMT_R32F;
            PreAlphaDepthCopyRT = CreateEmptyRT("PreAlphaDepthCopy", 3, Width, Height, 32, &desc);
        }


        if (!SMAA_areaTex)
            D3DXCreateTextureFromResourceExW(pDevice, hm, MAKEINTRESOURCEW(IDR_AreaTex), 160, 560, 1, 0, D3DFMT_UNKNOWN, D3DPOOL_MANAGED, D3DX_FILTER_LINEAR, D3DX_FILTER_LINEAR, 0, NULL, NULL, &SMAA_areaTex);
        if (!SMAA_searchTex)
            D3DXCreateTextureFromResourceExW(pDevice, hm, MAKEINTRESOURCEW(IDR_SearchTex), 64, 16, 1, 0, D3DFMT_UNKNOWN, D3DPOOL_MANAGED, D3DX_FILTER_LINEAR, D3DX_FILTER_LINEAR, 0, NULL, NULL, &SMAA_searchTex);
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
        // PostFxResources.mSpecularAoRT    =nullptr;
        // PostFxResources.mNormalRT        =nullptr;
        PostFxResources.mDiffuseRT       =nullptr;
        // PostFxResources.mSpecularRT      =nullptr;
        // PostFxResources.mDepthRT         =nullptr;
        // PostFxResources.mStencilRT       =nullptr;
        PostFxResources.mFullScreenRT    =nullptr;
        // PostFxResources.mFullScreenRT2   =nullptr;
        PostFxResources.mHalfScreenRT    =nullptr;
        // PostFxResources.mCascadeAtlasRT  =nullptr;
        // PostFxResources.mQuarterScreenRT =nullptr;
        // PostFxResources.mBloomRT         =nullptr;
        // PostFxResources.mCurrentLum      =nullptr;

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
        // PostFxResources.mNormalRT       = rage::grcTextureFactoryPC::GetRTByName( "_DEFERRED_GBUFFER_1_"  );
        PostFxResources.mDiffuseRT      = rage::grcTextureFactoryPC::GetRTByName( "_DEFERRED_GBUFFER_0_"  );
        // PostFxResources.mSpecularRT     = rage::grcTextureFactoryPC::GetRTByName( "_DEFERRED_GBUFFER_2_"  );
        // PostFxResources.mDepthRT        = rage::grcTextureFactoryPC::GetRTByName( "_DEFERRED_GBUFFER_3_"  );
        // PostFxResources.mStencilRT      = rage::grcTextureFactoryPC::GetRTByName( "_STENCIL_BUFFER_"      );
        // PostFxResources.mCascadeAtlasRT = rage::grcTextureFactoryPC::GetRTByName( "CASCADE_ATLAS"         );
        PostFxResources.mFullScreenRT   = rage::grcTextureFactoryPC::GetRTByName( "FullScreenCopy"        );
        // PostFxResources.mFullScreenRT2  = rage::grcTextureFactoryPC::GetRTByName( "FullScreenCopy2"       );
        PostFxResources.mHalfScreenRT   = rage::grcTextureFactoryPC::GetRTByName( "Quarter Screen 0"      );
        // PostFxResources.mQuarterScreenRT= rage::grcTextureFactoryPC::GetRTByName( "Blur Screen 0"         );
        // PostFxResources.mBloomRT        = rage::grcTextureFactoryPC::GetRTByName( "Blur Screen 2 Copy"    );
        // PostFxResources.mCurrentLum     = rage::grcTextureFactoryPC::GetRTByName( "Current Lum"           );

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

    static void NewFog() {
        IDirect3DDevice9* pDevice = rage::grcDevice::GetD3DDevice();

        IDirect3DSurface9* prevSurface = nullptr;
        IDirect3DSurface9* prevDepthStencilSurface = nullptr;
        IDirect3DPixelShader9* prevPS = nullptr;

        pDevice->GetRenderTarget(0, &prevSurface);
        pDevice->GetDepthStencilSurface(&prevDepthStencilSurface);
        
        pDevice->GetPixelShader(&prevPS);

        {
            if (PostFxResources.PreAlphaDepthCopyRT)
            {
                PostFxResources.PreAlphaDepthCopyRT->mD3DTexture->GetSurfaceLevel(0, &PostFxResources.PreAlphaDepthSurface);

                // Blit depth to texture 
                if (PostFxResources.Blit_PS)
                {
                    if (PostFxResources.PreAlphaDepthSurface)
                    {
                        pDevice->SetRenderTarget(0, PostFxResources.PreAlphaDepthSurface);
                        pDevice->SetDepthStencilSurface(nullptr);

                        // No need to set texture here as the desired depth texture already set (GBufferTextureSampler3)

                        pDevice->SetPixelShader(PostFxResources.Blit_PS);

                        pDevice->DrawPrimitive(D3DPT_TRIANGLELIST, 0, 2);
                    }
                }
            }

            // Restore fog pass
            if (prevPS)
            {
                if (prevSurface)
                {
                    pDevice->SetRenderTarget(0, prevSurface);
                    pDevice->SetDepthStencilSurface(prevDepthStencilSurface);

                    pDevice->SetPixelShader(prevPS);

                    hbDrawPrimitivePostFX.fun();
                }
            }
        }

        SAFE_RELEASE(PostFxResources.PreAlphaDepthSurface);

        SAFE_RELEASE(prevSurface);
        SAFE_RELEASE(prevDepthStencilSurface);

        SAFE_RELEASE(prevPS);
    }

    static void NewPostFX() {
        IDirect3DPixelShader9* oldps = 0;
        IDirect3DVertexShader9* oldvs = 0;
        IDirect3DDevice9* pDevice = rage::grcDevice::GetD3DDevice();

        // get textures
        // if(PostFxResources.mNormalRT->mD3DTexture == nullptr)
        //     return;
        // PostFxResources.NormalTex = PostFxResources.mNormalRT->mD3DTexture;
        PostFxResources.DiffuseTex = PostFxResources.mDiffuseRT->mD3DTexture;
        // PostFxResources.SpecularTex = PostFxResources.mSpecularRT->mD3DTexture;
        // PostFxResources.StencilTex = PostFxResources.mStencilRT->mD3DTexture;
        // PostFxResources.BloomTex = PostFxResources.mBloomRT->mD3DTexture;
        // PostFxResources.CurrentLumTex = PostFxResources.mCurrentLum->mD3DTexture;
        // PostFxResources.DepthTex = PostFxResources.mDepthRT->mD3DTexture;
        // if (PostFxResources.mCascadeAtlasRT)
        //     PostFxResources.CascadeAtlasTex = PostFxResources.mCascadeAtlasRT->mD3DTexture;

        PostFxResources.FullScreenTex = PostFxResources.mFullScreenRT->mD3DTexture;
        PostFxResources.HalfScreenTex = PostFxResources.mHalfScreenRT->mD3DTexture;
        // PostFxResources.pQuarterHDRTex = PostFxResources.mQuarterScreenRT->mD3DTexture;

        // get surfaces
        PostFxResources.FullScreenTex->GetSurfaceLevel(0, &PostFxResources.FullScreenSurface);

        PostFxResources.FullScreenTex_temp1->mD3DTexture->GetSurfaceLevel(0, &PostFxResources.FullScreenSurface_temp1);
        PostFxResources.FullScreenTex_temp2->mD3DTexture->GetSurfaceLevel(0, &PostFxResources.FullScreenSurface_temp2);
        PostFxResources.edgesTex->mD3DTexture->GetSurfaceLevel(0, &PostFxResources.edgesSurf);
        PostFxResources.blendTex->mD3DTexture->GetSurfaceLevel(0, &PostFxResources.blendSurf);

        PostFxResources.FullScreenDownsampleTex->mD3DTexture->GetSurfaceLevel(0, &PostFxResources.FullScreenDownsampleSurf);
        PostFxResources.FullScreenDownsampleTex2->mD3DTexture->GetSurfaceLevel(0, &PostFxResources.FullScreenDownsampleSurf2);

        // if (PostFxResources.CascadeAtlasTex)
        //     PostFxResources.CascadeAtlasTex->GetSurfaceLevel(0, &PostFxResources.CascadeAtlasSurf);
        //PostFxResources.pShadowBlurTex1->mD3DTexture->GetSurfaceLevel(0, &PostFxResources.pShadowBlurSurf1);
        //PostFxResources.pShadowBlurTex2->mD3DTexture->GetSurfaceLevel(0, &PostFxResources.pShadowBlurSurf2);


        pDevice->GetRenderTarget(0, &PostFxResources.backBuffer);
        pDevice->GetPixelShader(&oldps);
        pDevice->GetVertexShader(&oldvs);

        saveRenderState();

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
        // SAFE_RELEASE(PostFxResources.CascadeAtlasSurf);

        SAFE_RELEASE(PostFxResources.FullScreenSurface);

        //SAFE_RELEASE(PostFxResources.pShadowBlurSurf1);
        //SAFE_RELEASE(PostFxResources.pShadowBlurSurf2);

        PostFxResources.renderTargetTex = nullptr;
        PostFxResources.textureRead = nullptr;
        PostFxResources.renderTargetSurf = nullptr;
        PostFxResources.surfaceRead = nullptr;
    }

    static HRESULT PostFx3(LPDIRECT3DDEVICE9 pDevice, IDirect3DPixelShader9* pShader, IDirect3DVertexShader9* vShader) {
        auto currGrcViewport = rage::GetCurrentViewport();

        HRESULT hr = S_FALSE;

        DWORD OldSRGB = 0;
        DWORD OldSampler = 0;

        // save render state between post processing steps, 
        // in general each step expects the environment as the game 
        // leaves it at the time post processing is used, 
        // each step works individually from each other
        for(int i = 0; i < PostfxTextureCount; i++) {
            pDevice->GetTexture(i, &PostFxResources.prePostFx[i]);
            pDevice->GetSamplerState(i, D3DSAMP_MAGFILTER, &PostFxResources.Samplers[i]);
        }

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
                    //if(PostFxResources.UseSSAO && PostFxResources.SSAO_gen_ps && PostFxResources.SSAO_blend_ps && PostFxResources.pShadowBlurSurf1 && PostFxResources.pShadowBlurSurf2) {
                    //
                    //    pDevice->SetPixelShader(PostFxResources.SSAO_gen_ps);
                    //    vec4[1] = PostFxResources.AoDistance;
                    //
                    //    //pDevice->SetTexture(2, 0);
                    //    pDevice->SetRenderTarget(0, PostFxResources.pShadowBlurSurf1);
                    //    //pDevice->SetTexture(2, PostFxResources.textureRead);
                    //    pDevice->DrawPrimitive(D3DPT_TRIANGLELIST, 0, 2);
                    //    pDevice->SetTexture(3, 0);
                    //
                    //    pDevice->SetPixelShader(PostFxResources.DeferredShadowBlurCircle_ps);
                    //    pDevice->SetRenderTarget(0, PostFxResources.pShadowBlurSurf2);
                    //    pDevice->SetTexture(11, PostFxResources.pShadowBlurTex1->mD3DTexture);
                    //    pDevice->DrawPrimitive(D3DPT_TRIANGLELIST, 0, 2);
                    //
                    //    pDevice->SetTexture(11, 0);
                    //
                    //    pDevice->SetPixelShader(PostFxResources.SSAO_blend_ps);
                    //    pDevice->SetRenderTarget(0, PostFxResources.renderTargetSurf);
                    //    pDevice->SetTexture(2, PostFxResources.textureRead);
                    //    pDevice->SetTexture(3, PostFxResources.pShadowBlurTex2->mD3DTexture);
                    //    pDevice->DrawPrimitive(D3DPT_TRIANGLELIST, 0, 2);
                    //    PostFxResources.swapbuffers();
                    //    pDevice->SetTexture(2, PostFxResources.textureRead);
                    //    pDevice->SetTexture(3, PostFxResources.prePostFx[3]);
                    //    pDevice->SetPixelShader(pShader);
                    //}

                    if(PostFxResources.useStippleFilter && PostFxResources.stipple_filter_ps) {
                        pDevice->SetPixelShader(PostFxResources.stipple_filter_ps);
                        pDevice->SetRenderTarget(0, PostFxResources.renderTargetSurf);
                        pDevice->SetTexture(2, PostFxResources.textureRead);
                        pDevice->DrawPrimitive(D3DPT_TRIANGLELIST, 0, 2);
                        PostFxResources.swapbuffers();
                        pDevice->SetPixelShader(pShader);
                    }

                    static auto dof = FusionFixSettings.GetRef("PREF_TCYC_DOF");
                    if(dof->get() > FusionFixSettings.DofText.eCutscenesOnly || (dof->get() == FusionFixSettings.DofText.eCutscenesOnly && CCutscenes::hasCutsceneFinished()) || shouldModifyMapMenuBackground()) {
                        if(PostFxResources.dof_blur_ps && PostFxResources.depth_of_field_tent_ps && PostFxResources.dof_coc_ps) {
                            if(PostFxResources.FullScreenDownsampleSurf && PostFxResources.FullScreenDownsampleSurf2) {
                                pDevice->SetSamplerState(8, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
                                pDevice->SetSamplerState(8, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
                                pDevice->SetSamplerState(8, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
                                pDevice->SetSamplerState(2, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);

                                pDevice->SetPixelShader(PostFxResources.dof_blur_ps);
                                pDevice->SetRenderTarget(0, PostFxResources.FullScreenDownsampleSurf);
                                pDevice->SetTexture(8, PostFxResources.HalfScreenTex);
                                pDevice->DrawPrimitive(D3DPT_TRIANGLELIST, 0, 2);
                                
                                pDevice->SetPixelShader(PostFxResources.depth_of_field_tent_ps);
                                pDevice->SetRenderTarget(0, PostFxResources.FullScreenDownsampleSurf2);
                                pDevice->SetTexture(8, PostFxResources.FullScreenDownsampleTex->mD3DTexture);
                                pDevice->DrawPrimitive(D3DPT_TRIANGLELIST, 0, 2);

                                pDevice->SetPixelShader(PostFxResources.dof_coc_ps);
                                pDevice->SetRenderTarget(0, PostFxResources.renderTargetSurf);
                                if (PostFxResources.bEnablePreAlphaDepth)
                                    pDevice->SetTexture(1, PostFxResources.PreAlphaDepthCopyRT->mD3DTexture);
                                pDevice->SetTexture(2, PostFxResources.textureRead);
                                pDevice->SetTexture(8, PostFxResources.FullScreenDownsampleTex2->mD3DTexture);
                                pDevice->DrawPrimitive(D3DPT_TRIANGLELIST, 0, 2);
                                PostFxResources.swapbuffers();

                                pDevice->SetPixelShader(pShader);
                            }
                        }
                    }

                    static auto refSunShafts = FusionFixSettings.GetRef("PREF_SUNSHAFTS");
                    if(refSunShafts->get()) {
                        // 2 passes at half res
                        if(PostFxResources.SSPrepass_PS && PostFxResources.SSDraw_PS) {
                            if(PostFxResources.FullScreenDownsampleSurf && PostFxResources.FullScreenDownsampleSurf2) {
                                pDevice->SetSamplerState(2, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
                                pDevice->SetSamplerState(8, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
                                pDevice->SetSamplerState(8, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
                                pDevice->SetSamplerState(8, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
                                pDevice->SetSamplerState(11, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
                                pDevice->SetSamplerState(11, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
                                pDevice->SetSamplerState(11, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
                                pDevice->SetSamplerState(13, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
                                pDevice->SetSamplerState(13, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
                                pDevice->SetSamplerState(13, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);

                                // crop around sun position
                                pDevice->SetPixelShader(PostFxResources.SSPrepass_PS);
                                pDevice->SetRenderTarget(0, PostFxResources.FullScreenDownsampleSurf);
                                if (PostFxResources.bEnablePreAlphaDepth)
                                    pDevice->SetTexture(1, PostFxResources.PreAlphaDepthCopyRT->mD3DTexture);
                                pDevice->SetTexture(2, PostFxResources.textureRead);
                                pDevice->SetTexture(13, PostFxResources.DiffuseTex);
                                pDevice->DrawPrimitive(D3DPT_TRIANGLELIST, 0, 2);

                                // sample sunshafts from a cropped texture
                                pDevice->SetPixelShader(PostFxResources.SSDraw_PS);
                                pDevice->SetRenderTarget(0, PostFxResources.FullScreenDownsampleSurf2);
                                pDevice->SetTexture(11, PostFxResources.FullScreenDownsampleTex->mD3DTexture);
                                pDevice->DrawPrimitive(D3DPT_TRIANGLELIST, 0, 2);

                                // second sunshafts pass
                                pDevice->SetPixelShader(PostFxResources.SSDraw_PS);
                                pDevice->SetRenderTarget(0, PostFxResources.FullScreenDownsampleSurf);
                                pDevice->SetTexture(11, PostFxResources.FullScreenDownsampleTex2->mD3DTexture);
                                pDevice->DrawPrimitive(D3DPT_TRIANGLELIST, 0, 2);

                                // add sunshafts to screen
                                pDevice->SetPixelShader(PostFxResources.SSAdd_PS);
                                pDevice->SetRenderTarget(0, PostFxResources.renderTargetSurf);
                                pDevice->SetTexture(2, PostFxResources.textureRead);
                                pDevice->SetTexture(11, PostFxResources.FullScreenDownsampleTex->mD3DTexture);

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

                        if(UsePostFxAA->get() > FusionFixSettings.AntialiasingText.eMO_OFF)
                            pDevice->SetRenderTarget(0, PostFxResources.FullScreenSurface_temp2);
                        else
                            pDevice->SetRenderTarget(0, PostFxResources.backBuffer);

                        if (PostFxResources.bEnablePreAlphaDepth)
                            pDevice->SetTexture(1, PostFxResources.PreAlphaDepthCopyRT->mD3DTexture);
                        pDevice->SetTexture(2, PostFxResources.textureRead);
                        pDevice->Clear(0, 0, D3DCLEAR_TARGET, 0, 0, 0);

                        pDevice->SetPixelShader(pShader);
                        pDevice->SetVertexShader(vShader);
                        //hr = pDevice->DrawPrimitive(D3DPT_TRIANGLELIST, 0, 2);
                        hbDrawPrimitivePostFX.fun();
                            // if(UsePostFxAA->get() > FusionFixSettings.AntialiasingText.eMO_OFF)
                            //     PostFxResources.swapbuffers();
                    }
                    
                    // Anti aliasing
                    if(UsePostFxAA && UsePostFxAA->get() > FusionFixSettings.AntialiasingText.eMO_OFF) {
                        // FXAA
                        if((UsePostFxAA->get() == FusionFixSettings.AntialiasingText.eFXAA) && PostFxResources.FxaaPS) {
                            pDevice->SetPixelShader(PostFxResources.FxaaPS);

                            // pDevice->SetRenderTarget(0, PostFxResources.renderTargetSurf);
                            pDevice->SetRenderTarget(0, PostFxResources.backBuffer);

                            pDevice->SetTexture(2, PostFxResources.FullScreenTex_temp2->mD3DTexture);
                            // pDevice->SetTexture(2, PostFxResources.textureRead);

                            hr = pDevice->DrawPrimitive(D3DPT_TRIANGLELIST, 0, 2);
                            pDevice->SetPixelShader(pShader);
                        }

                        // SMAA
                        if(UsePostFxAA->get() >= FusionFixSettings.AntialiasingText.eSMAA &&
                           PostFxResources.SMAA_EdgeDetection && PostFxResources.SMAA_BlendingWeightsCalculation && PostFxResources.SMAA_NeighborhoodBlending &&
                           PostFxResources.SMAA_EdgeDetectionVS && PostFxResources.SMAA_BlendingWeightsCalculationVS && PostFxResources.SMAA_NeighborhoodBlendingVS &&
                           PostFxResources.SMAA_areaTex && PostFxResources.SMAA_searchTex && PostFxResources.edgesTex && PostFxResources.blendTex
                           ) {
                            DWORD oldSample = 0;

                            for (int i = 0; i <= 4; ++i)
                            {
                                pDevice->GetSamplerState(i, D3DSAMP_MINFILTER, &oldSample);
                                pDevice->GetSamplerState(i, D3DSAMP_MAGFILTER, &oldSample);
                                pDevice->GetSamplerState(i, D3DSAMP_MIPFILTER, &oldSample);
                                pDevice->GetSamplerState(i, D3DSAMP_ADDRESSU, &oldSample);
                                pDevice->GetSamplerState(i, D3DSAMP_ADDRESSV, &oldSample);
                                pDevice->GetSamplerState(i, D3DSAMP_ADDRESSW, &oldSample);
                            }

                            // colorTex / colorGammaTex
                            pDevice->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
                            pDevice->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
                            pDevice->SetSamplerState(0, D3DSAMP_MIPFILTER, D3DTEXF_POINT);
                            pDevice->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
                            pDevice->SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
                            pDevice->SetSamplerState(0, D3DSAMP_ADDRESSW, D3DTADDRESS_WRAP);

                            // edgesTex
                            pDevice->SetSamplerState(1, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
                            pDevice->SetSamplerState(1, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
                            pDevice->SetSamplerState(1, D3DSAMP_MIPFILTER, D3DTEXF_LINEAR);
                            pDevice->SetSamplerState(1, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
                            pDevice->SetSamplerState(1, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
                            pDevice->SetSamplerState(1, D3DSAMP_ADDRESSW, D3DTADDRESS_WRAP);

                            // blendTex
                            pDevice->SetSamplerState(4, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
                            pDevice->SetSamplerState(4, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
                            pDevice->SetSamplerState(4, D3DSAMP_MIPFILTER, D3DTEXF_LINEAR);
                            pDevice->SetSamplerState(4, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
                            pDevice->SetSamplerState(4, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
                            pDevice->SetSamplerState(4, D3DSAMP_ADDRESSW, D3DTADDRESS_WRAP);

                            // areaTex
                            pDevice->SetSamplerState(2, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
                            pDevice->SetSamplerState(2, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
                            pDevice->SetSamplerState(2, D3DSAMP_MIPFILTER, D3DTEXF_LINEAR);
                            pDevice->SetSamplerState(2, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
                            pDevice->SetSamplerState(2, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
                            pDevice->SetSamplerState(2, D3DSAMP_ADDRESSW, D3DTADDRESS_CLAMP);

                            // searchTex
                            pDevice->SetSamplerState(3, D3DSAMP_MINFILTER, D3DTEXF_POINT);
                            pDevice->SetSamplerState(3, D3DSAMP_MAGFILTER, D3DTEXF_POINT);
                            pDevice->SetSamplerState(3, D3DSAMP_MIPFILTER, D3DTEXF_POINT);
                            pDevice->SetSamplerState(3, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
                            pDevice->SetSamplerState(3, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
                            pDevice->SetSamplerState(3, D3DSAMP_ADDRESSW, D3DTADDRESS_CLAMP);

                            // SMAA_EdgeDetection
                            pDevice->SetPixelShader(PostFxResources.SMAA_EdgeDetection);
                            pDevice->SetVertexShader(PostFxResources.SMAA_EdgeDetectionVS);
                            pDevice->SetRenderTarget(0, PostFxResources.edgesSurf);
                            pDevice->SetTexture(0, PostFxResources.FullScreenTex_temp2->mD3DTexture);
                            pDevice->Clear(0, 0, D3DCLEAR_TARGET, 0, 0, 0);
                            pDevice->DrawPrimitive(D3DPT_TRIANGLELIST, 0, 2);

                            // SMAA_BlendingWeightsCalculation
                            pDevice->SetPixelShader(PostFxResources.SMAA_BlendingWeightsCalculation);
                            pDevice->SetVertexShader(PostFxResources.SMAA_BlendingWeightsCalculationVS);
                            pDevice->SetRenderTarget(0, PostFxResources.blendSurf);
                            pDevice->SetTexture(1, PostFxResources.edgesTex->mD3DTexture);
                            pDevice->SetTexture(2, PostFxResources.SMAA_areaTex);
                            pDevice->SetTexture(3, PostFxResources.SMAA_searchTex);
                            pDevice->Clear(0, 0, D3DCLEAR_TARGET, 0, 0, 0);
                            pDevice->DrawPrimitive(D3DPT_TRIANGLELIST, 0, 2);

                            // SMAA_NeighborhoodBlending
                            pDevice->SetPixelShader(PostFxResources.SMAA_NeighborhoodBlending);
                            pDevice->SetVertexShader(PostFxResources.SMAA_NeighborhoodBlendingVS);

                            // pDevice->SetRenderTarget(0, PostFxResources.renderTargetSurf);
                            pDevice->SetRenderTarget(0, PostFxResources.backBuffer);

                            pDevice->SetTexture(0, PostFxResources.FullScreenTex_temp2->mD3DTexture);
                            pDevice->SetTexture(4, PostFxResources.blendTex->mD3DTexture);

                            pDevice->GetSamplerState(0, D3DSAMP_SRGBTEXTURE, &oldSample);
                            pDevice->GetRenderState(D3DRS_SRGBWRITEENABLE, &OldSRGB); // save srgb state
                            pDevice->SetSamplerState(0, D3DSAMP_SRGBTEXTURE, 1);
                            pDevice->SetRenderState(D3DRS_SRGBWRITEENABLE, 1);

                            pDevice->DrawPrimitive(D3DPT_TRIANGLELIST, 0, 2);

                            pDevice->SetSamplerState(0, D3DSAMP_SRGBTEXTURE, oldSample);
                            pDevice->SetRenderState(D3DRS_SRGBWRITEENABLE, OldSRGB); // restore srgb state

                            for (int i = 0; i <= 4; ++i)
                            {
                                pDevice->SetSamplerState(i, D3DSAMP_MINFILTER, oldSample);
                                pDevice->SetSamplerState(i, D3DSAMP_MAGFILTER, oldSample);
                                pDevice->SetSamplerState(i, D3DSAMP_MIPFILTER, oldSample);
                                pDevice->SetSamplerState(i, D3DSAMP_ADDRESSU, oldSample);
                                pDevice->SetSamplerState(i, D3DSAMP_ADDRESSV, oldSample);
                                pDevice->SetSamplerState(i, D3DSAMP_ADDRESSW, oldSample);
                            }

                            pDevice->SetTexture(0, PostFxResources.prePostFx[0]);
                            pDevice->SetTexture(1, PostFxResources.prePostFx[1]);
                            pDevice->SetTexture(2, PostFxResources.FullScreenTex_temp2->mD3DTexture);
                            pDevice->SetTexture(3, PostFxResources.prePostFx[3]);
                            pDevice->SetTexture(4, PostFxResources.prePostFx[4]);
                            pDevice->SetPixelShader(pShader);
                            pDevice->SetVertexShader(vShader);
                        }
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

    static inline thread_local bool bInsteadDrawPrimitiveFog = false;
    static inline injector::hook_back<void(__fastcall*)(void*, void*, int, int, int)> hbDrawCallFog;
    static void __fastcall DrawCallFog(void* _this, void* edx, int a2, int a3, int a4) {
        bInsteadDrawPrimitiveFog = true;
        hbDrawCallFog.fun(_this, edx, a2, a3, a4);
        bInsteadDrawPrimitiveFog = false;
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
        if (bInsteadDrawPrimitiveFog)
        {
            bInsteadDrawPrimitiveFog = false;
            // Do not initialize shaders, RTs etc. here or we get a device reset error for some reason
            NewFog();
        }
        else if (bInsteadDrawPrimitivePostFX)
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
        // atmoscatt clouds
        if (PostFxResources.DiffuseTex != nullptr)
        {
            IDirect3DSurface9* DiffuseSurf = nullptr;
            PostFxResources.DiffuseTex->GetSurfaceLevel(0, &DiffuseSurf);
            if (DiffuseSurf)
            {
                IDirect3DSurface9* oldRenderTarget1 = 0;
                pDevice->GetRenderTarget(1, &oldRenderTarget1);
                pDevice->SetRenderTarget(1, DiffuseSurf);
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
                    
                    if (PostFxResources.bEnablePreAlphaDepth)
                    {
                        pattern = hook::pattern("6A ? E8 ? ? ? ? 5E 8B E5 5D C3");
                        if (!pattern.empty())
                            hbDrawCallFog.fun = injector::MakeCALL(pattern.get_first(2), DrawCallFog).get();
                        else
                        {
                            pattern = hook::pattern("6A ? 8B CE E8 ? ? ? ? 5E 8B E5");
                            hbDrawCallFog.fun = injector::MakeCALL(pattern.get_first(4), DrawCallFog).get();
                        }
                    }

                }
            }
        };
    }
} PostFX;