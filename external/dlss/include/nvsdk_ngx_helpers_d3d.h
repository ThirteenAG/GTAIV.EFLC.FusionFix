/*
 * SPDX-FileCopyrightText: Copyright (c) 2019-2023 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: LicenseRef-NvidiaProprietary
 *
 * NVIDIA CORPORATION, its affiliates and licensors retain all intellectual
 * property and proprietary rights in and to this material, related
 * documentation and any modifications thereto. Any use, reproduction,
 * disclosure or distribution of this material and related documentation
 * without an express license agreement from NVIDIA CORPORATION or
 * its affiliates is strictly prohibited.
 */

#ifndef NVSDK_NGX_HELPERS_D3D_H
#define NVSDK_NGX_HELPERS_D3D_H
#pragma once

#include "nvsdk_ngx.h"
#include "nvsdk_ngx_defs.h"

/*** D3D11 ***/
typedef struct NVSDK_NGX_D3D11_Feature_Eval_Params
{
    ID3D11Resource*  pInColor;
    ID3D11Resource*  pInOutput;
    /*** OPTIONAL for DLSS ***/
    float       InSharpness;
} NVSDK_NGX_D3D11_Feature_Eval_Params;

typedef struct NVSDK_NGX_D3D11_GBuffer
{
    ID3D11Resource* pInAttrib[NVSDK_NGX_GBUFFERTYPE_NUM];
} NVSDK_NGX_D3D11_GBuffer;

typedef struct NVSDK_NGX_D3D11_DLSS_Eval_Params
{
    NVSDK_NGX_D3D11_Feature_Eval_Params Feature;
    ID3D11Resource*                     pInDepth;
    ID3D11Resource*                     pInMotionVectors;
    float                               InJitterOffsetX;     /* Jitter offset must be in input/render pixel space */
    float                               InJitterOffsetY;
    NVSDK_NGX_Dimensions                InRenderSubrectDimensions;
    /*** OPTIONAL - leave to 0/0.0f if unused ***/
    int                                 InReset;             /* Set to 1 when scene changes completely (new level etc) */
    float                               InMVScaleX;          /* If MVs need custom scaling to convert to pixel space */
    float                               InMVScaleY;
    ID3D11Resource*                     pInTransparencyMask; /* Unused/Reserved for future use */
    ID3D11Resource*                     pInExposureTexture;
    ID3D11Resource*                     pInBiasCurrentColorMask;
    NVSDK_NGX_Coordinates               InColorSubrectBase;
    NVSDK_NGX_Coordinates               InDepthSubrectBase;
    NVSDK_NGX_Coordinates               InMVSubrectBase;
    NVSDK_NGX_Coordinates               InTranslucencySubrectBase;
    NVSDK_NGX_Coordinates               InBiasCurrentColorSubrectBase;
    NVSDK_NGX_Coordinates               InOutputSubrectBase;
    float                               InPreExposure;
    float                               InExposureScale;
    int                                 InIndicatorInvertXAxis;
    int                                 InIndicatorInvertYAxis;
    /*** OPTIONAL - only for research purposes ***/
    NVSDK_NGX_D3D11_GBuffer             GBufferSurface;
    NVSDK_NGX_ToneMapperType            InToneMapperType;
    ID3D11Resource*                     pInMotionVectors3D;
    ID3D11Resource*                     pInIsParticleMask; /* to identify which pixels contains particles, essentially that are not drawn as part of base pass */
    ID3D11Resource*                     pInAnimatedTextureMask; /* a binary mask covering pixels occupied by animated textures */
    ID3D11Resource*                     pInDepthHighRes;
    ID3D11Resource*                     pInPositionViewSpace;
    float                               InFrameTimeDeltaInMsec; /* helps in determining the amount to denoise or anti-alias based on the speed of the object from motion vector magnitudes and fps as determined by this delta */
    ID3D11Resource*                     pInRayTracingHitDistance; /* for each effect - approximation to the amount of noise in a ray-traced color */
    ID3D11Resource*                     pInMotionVectorsReflections; /* motion vectors of reflected objects like for mirrored surfaces */
} NVSDK_NGX_D3D11_DLSS_Eval_Params;

typedef struct NVSDK_NGX_D3D11_DLISP_Eval_Params
{
    NVSDK_NGX_D3D11_Feature_Eval_Params Feature;
    /*** OPTIONAL - leave to 0/0.0f if unused  ***/
    unsigned int                    InRectX;
    unsigned int                    InRectY;
    unsigned int                    InRectW;
    unsigned int                    InRectH;
    float                           InDenoise;
} NVSDK_NGX_D3D11_DLISP_Eval_Params;



static inline NVSDK_NGX_Result NGX_D3D11_CREATE_DLSS_EXT(
    ID3D11DeviceContext *pInCtx,
    NVSDK_NGX_Handle **ppOutHandle,
    NVSDK_NGX_Parameter *pInParams,
    NVSDK_NGX_DLSS_Create_Params *pInDlssCreateParams)
{
    NVSDK_NGX_Parameter_SetUI(pInParams, NVSDK_NGX_Parameter_Width, pInDlssCreateParams->Feature.InWidth);
    NVSDK_NGX_Parameter_SetUI(pInParams, NVSDK_NGX_Parameter_Height, pInDlssCreateParams->Feature.InHeight);
    NVSDK_NGX_Parameter_SetUI(pInParams, NVSDK_NGX_Parameter_OutWidth, pInDlssCreateParams->Feature.InTargetWidth);
    NVSDK_NGX_Parameter_SetUI(pInParams, NVSDK_NGX_Parameter_OutHeight, pInDlssCreateParams->Feature.InTargetHeight);
    NVSDK_NGX_Parameter_SetI(pInParams, NVSDK_NGX_Parameter_PerfQualityValue, pInDlssCreateParams->Feature.InPerfQualityValue);
    NVSDK_NGX_Parameter_SetI(pInParams, NVSDK_NGX_Parameter_DLSS_Feature_Create_Flags, pInDlssCreateParams->InFeatureCreateFlags);
    NVSDK_NGX_Parameter_SetI(pInParams, NVSDK_NGX_Parameter_DLSS_Enable_Output_Subrects, pInDlssCreateParams->InEnableOutputSubrects ? 1 : 0);

    return NVSDK_NGX_D3D11_CreateFeature(pInCtx, NVSDK_NGX_Feature_SuperSampling, pInParams, ppOutHandle);
}


static inline NVSDK_NGX_Result NGX_D3D11_EVALUATE_DLSS_EXT(
    ID3D11DeviceContext *pInCtx,
    NVSDK_NGX_Handle *pInHandle,
    NVSDK_NGX_Parameter *pInParams,
    NVSDK_NGX_D3D11_DLSS_Eval_Params *pInDlssEvalParams)
{
    NVSDK_NGX_Parameter_SetD3d11Resource(pInParams, NVSDK_NGX_Parameter_Color, pInDlssEvalParams->Feature.pInColor);
    NVSDK_NGX_Parameter_SetD3d11Resource(pInParams, NVSDK_NGX_Parameter_Output, pInDlssEvalParams->Feature.pInOutput);
    NVSDK_NGX_Parameter_SetD3d11Resource(pInParams, NVSDK_NGX_Parameter_Depth, pInDlssEvalParams->pInDepth);
    NVSDK_NGX_Parameter_SetD3d11Resource(pInParams, NVSDK_NGX_Parameter_MotionVectors, pInDlssEvalParams->pInMotionVectors);
    NVSDK_NGX_Parameter_SetF(pInParams, NVSDK_NGX_Parameter_Jitter_Offset_X, pInDlssEvalParams->InJitterOffsetX);
    NVSDK_NGX_Parameter_SetF(pInParams, NVSDK_NGX_Parameter_Jitter_Offset_Y, pInDlssEvalParams->InJitterOffsetY);
    NVSDK_NGX_Parameter_SetF(pInParams, NVSDK_NGX_Parameter_Sharpness, pInDlssEvalParams->Feature.InSharpness);
    NVSDK_NGX_Parameter_SetI(pInParams, NVSDK_NGX_Parameter_Reset, pInDlssEvalParams->InReset);
    NVSDK_NGX_Parameter_SetF(pInParams, NVSDK_NGX_Parameter_MV_Scale_X, pInDlssEvalParams->InMVScaleX == 0.0f ? 1.0f : pInDlssEvalParams->InMVScaleX);
    NVSDK_NGX_Parameter_SetF(pInParams, NVSDK_NGX_Parameter_MV_Scale_Y, pInDlssEvalParams->InMVScaleY == 0.0f ? 1.0f : pInDlssEvalParams->InMVScaleY);
    NVSDK_NGX_Parameter_SetD3d11Resource(pInParams, NVSDK_NGX_Parameter_TransparencyMask, pInDlssEvalParams->pInTransparencyMask);
    NVSDK_NGX_Parameter_SetD3d11Resource(pInParams, NVSDK_NGX_Parameter_ExposureTexture, pInDlssEvalParams->pInExposureTexture);
    NVSDK_NGX_Parameter_SetD3d11Resource(pInParams, NVSDK_NGX_Parameter_DLSS_Input_Bias_Current_Color_Mask, pInDlssEvalParams->pInBiasCurrentColorMask);
    NVSDK_NGX_Parameter_SetD3d11Resource(pInParams, NVSDK_NGX_Parameter_GBuffer_Albedo, pInDlssEvalParams->GBufferSurface.pInAttrib[NVSDK_NGX_GBUFFER_ALBEDO]);
    NVSDK_NGX_Parameter_SetD3d11Resource(pInParams, NVSDK_NGX_Parameter_GBuffer_Roughness, pInDlssEvalParams->GBufferSurface.pInAttrib[NVSDK_NGX_GBUFFER_ROUGHNESS]);
    NVSDK_NGX_Parameter_SetD3d11Resource(pInParams, NVSDK_NGX_Parameter_GBuffer_Metallic, pInDlssEvalParams->GBufferSurface.pInAttrib[NVSDK_NGX_GBUFFER_METALLIC]);
    NVSDK_NGX_Parameter_SetD3d11Resource(pInParams, NVSDK_NGX_Parameter_GBuffer_Specular, pInDlssEvalParams->GBufferSurface.pInAttrib[NVSDK_NGX_GBUFFER_SPECULAR]);
    NVSDK_NGX_Parameter_SetD3d11Resource(pInParams, NVSDK_NGX_Parameter_GBuffer_Subsurface, pInDlssEvalParams->GBufferSurface.pInAttrib[NVSDK_NGX_GBUFFER_SUBSURFACE]);
    NVSDK_NGX_Parameter_SetD3d11Resource(pInParams, NVSDK_NGX_Parameter_GBuffer_Normals, pInDlssEvalParams->GBufferSurface.pInAttrib[NVSDK_NGX_GBUFFER_NORMALS]);
    NVSDK_NGX_Parameter_SetD3d11Resource(pInParams, NVSDK_NGX_Parameter_GBuffer_ShadingModelId, pInDlssEvalParams->GBufferSurface.pInAttrib[NVSDK_NGX_GBUFFER_SHADINGMODELID]);
    NVSDK_NGX_Parameter_SetD3d11Resource(pInParams, NVSDK_NGX_Parameter_GBuffer_MaterialId, pInDlssEvalParams->GBufferSurface.pInAttrib[NVSDK_NGX_GBUFFER_MATERIALID]);
    NVSDK_NGX_Parameter_SetD3d11Resource(pInParams, NVSDK_NGX_Parameter_GBuffer_Atrrib_8, pInDlssEvalParams->GBufferSurface.pInAttrib[8]);
    NVSDK_NGX_Parameter_SetD3d11Resource(pInParams, NVSDK_NGX_Parameter_GBuffer_Atrrib_9, pInDlssEvalParams->GBufferSurface.pInAttrib[9]);
    NVSDK_NGX_Parameter_SetD3d11Resource(pInParams, NVSDK_NGX_Parameter_GBuffer_Atrrib_10, pInDlssEvalParams->GBufferSurface.pInAttrib[10]);
    NVSDK_NGX_Parameter_SetD3d11Resource(pInParams, NVSDK_NGX_Parameter_GBuffer_Atrrib_11, pInDlssEvalParams->GBufferSurface.pInAttrib[11]);
    NVSDK_NGX_Parameter_SetD3d11Resource(pInParams, NVSDK_NGX_Parameter_GBuffer_Atrrib_12, pInDlssEvalParams->GBufferSurface.pInAttrib[12]);
    NVSDK_NGX_Parameter_SetD3d11Resource(pInParams, NVSDK_NGX_Parameter_GBuffer_Atrrib_13, pInDlssEvalParams->GBufferSurface.pInAttrib[13]);
    NVSDK_NGX_Parameter_SetD3d11Resource(pInParams, NVSDK_NGX_Parameter_GBuffer_Atrrib_14, pInDlssEvalParams->GBufferSurface.pInAttrib[14]);
    NVSDK_NGX_Parameter_SetD3d11Resource(pInParams, NVSDK_NGX_Parameter_GBuffer_Atrrib_15, pInDlssEvalParams->GBufferSurface.pInAttrib[15]);
    NVSDK_NGX_Parameter_SetUI(pInParams, NVSDK_NGX_Parameter_TonemapperType, pInDlssEvalParams->InToneMapperType);
    NVSDK_NGX_Parameter_SetD3d11Resource(pInParams, NVSDK_NGX_Parameter_MotionVectors3D, pInDlssEvalParams->pInMotionVectors3D);
    NVSDK_NGX_Parameter_SetD3d11Resource(pInParams, NVSDK_NGX_Parameter_IsParticleMask, pInDlssEvalParams->pInIsParticleMask);
    NVSDK_NGX_Parameter_SetD3d11Resource(pInParams, NVSDK_NGX_Parameter_AnimatedTextureMask, pInDlssEvalParams->pInAnimatedTextureMask);
    NVSDK_NGX_Parameter_SetD3d11Resource(pInParams, NVSDK_NGX_Parameter_DepthHighRes, pInDlssEvalParams->pInDepthHighRes);
    NVSDK_NGX_Parameter_SetD3d11Resource(pInParams, NVSDK_NGX_Parameter_Position_ViewSpace, pInDlssEvalParams->pInPositionViewSpace);
    NVSDK_NGX_Parameter_SetF(pInParams, NVSDK_NGX_Parameter_FrameTimeDeltaInMsec, pInDlssEvalParams->InFrameTimeDeltaInMsec);
    NVSDK_NGX_Parameter_SetD3d11Resource(pInParams, NVSDK_NGX_Parameter_RayTracingHitDistance, pInDlssEvalParams->pInRayTracingHitDistance);
    NVSDK_NGX_Parameter_SetD3d11Resource(pInParams, NVSDK_NGX_Parameter_MotionVectorsReflection, pInDlssEvalParams->pInMotionVectorsReflections);
    NVSDK_NGX_Parameter_SetUI(pInParams, NVSDK_NGX_Parameter_DLSS_Input_Color_Subrect_Base_X, pInDlssEvalParams->InColorSubrectBase.X);
    NVSDK_NGX_Parameter_SetUI(pInParams, NVSDK_NGX_Parameter_DLSS_Input_Color_Subrect_Base_Y, pInDlssEvalParams->InColorSubrectBase.Y);
    NVSDK_NGX_Parameter_SetUI(pInParams, NVSDK_NGX_Parameter_DLSS_Input_Depth_Subrect_Base_X, pInDlssEvalParams->InDepthSubrectBase.X);
    NVSDK_NGX_Parameter_SetUI(pInParams, NVSDK_NGX_Parameter_DLSS_Input_Depth_Subrect_Base_Y, pInDlssEvalParams->InDepthSubrectBase.Y);
    NVSDK_NGX_Parameter_SetUI(pInParams, NVSDK_NGX_Parameter_DLSS_Input_MV_SubrectBase_X, pInDlssEvalParams->InMVSubrectBase.X);
    NVSDK_NGX_Parameter_SetUI(pInParams, NVSDK_NGX_Parameter_DLSS_Input_MV_SubrectBase_Y, pInDlssEvalParams->InMVSubrectBase.Y);
    NVSDK_NGX_Parameter_SetUI(pInParams, NVSDK_NGX_Parameter_DLSS_Input_Translucency_SubrectBase_X, pInDlssEvalParams->InTranslucencySubrectBase.X);
    NVSDK_NGX_Parameter_SetUI(pInParams, NVSDK_NGX_Parameter_DLSS_Input_Translucency_SubrectBase_Y, pInDlssEvalParams->InTranslucencySubrectBase.Y);
    NVSDK_NGX_Parameter_SetUI(pInParams, NVSDK_NGX_Parameter_DLSS_Input_Bias_Current_Color_SubrectBase_X, pInDlssEvalParams->InBiasCurrentColorSubrectBase.X);
    NVSDK_NGX_Parameter_SetUI(pInParams, NVSDK_NGX_Parameter_DLSS_Input_Bias_Current_Color_SubrectBase_Y, pInDlssEvalParams->InBiasCurrentColorSubrectBase.Y);
    NVSDK_NGX_Parameter_SetUI(pInParams, NVSDK_NGX_Parameter_DLSS_Output_Subrect_Base_X, pInDlssEvalParams->InOutputSubrectBase.X);
    NVSDK_NGX_Parameter_SetUI(pInParams, NVSDK_NGX_Parameter_DLSS_Output_Subrect_Base_Y, pInDlssEvalParams->InOutputSubrectBase.Y);
    NVSDK_NGX_Parameter_SetUI(pInParams, NVSDK_NGX_Parameter_DLSS_Render_Subrect_Dimensions_Width , pInDlssEvalParams->InRenderSubrectDimensions.Width);
    NVSDK_NGX_Parameter_SetUI(pInParams, NVSDK_NGX_Parameter_DLSS_Render_Subrect_Dimensions_Height, pInDlssEvalParams->InRenderSubrectDimensions.Height);
    NVSDK_NGX_Parameter_SetF(pInParams, NVSDK_NGX_Parameter_DLSS_Pre_Exposure, pInDlssEvalParams->InPreExposure == 0.0f ? 1.0f : pInDlssEvalParams->InPreExposure);
    NVSDK_NGX_Parameter_SetF(pInParams, NVSDK_NGX_Parameter_DLSS_Exposure_Scale, pInDlssEvalParams->InExposureScale == 0.0f ? 1.0f : pInDlssEvalParams->InExposureScale);
    NVSDK_NGX_Parameter_SetI(pInParams, NVSDK_NGX_Parameter_DLSS_Indicator_Invert_X_Axis, pInDlssEvalParams->InIndicatorInvertXAxis);
    NVSDK_NGX_Parameter_SetI(pInParams, NVSDK_NGX_Parameter_DLSS_Indicator_Invert_Y_Axis, pInDlssEvalParams->InIndicatorInvertYAxis);

    return NVSDK_NGX_D3D11_EvaluateFeature_C(pInCtx, pInHandle, pInParams, NULL);
}

static inline NVSDK_NGX_Result NGX_D3D11_CREATE_DLISP_EXT(
    ID3D11DeviceContext *pInCtx,
    NVSDK_NGX_Handle **ppOutHandle,
    NVSDK_NGX_Parameter *pInParams,
    NVSDK_NGX_Feature_Create_Params *pDlispCreateParams)
{
    NVSDK_NGX_Parameter_SetUI(pInParams, NVSDK_NGX_Parameter_Width, pDlispCreateParams->InWidth);
    NVSDK_NGX_Parameter_SetUI(pInParams, NVSDK_NGX_Parameter_Height, pDlispCreateParams->InHeight);
    NVSDK_NGX_Parameter_SetUI(pInParams, NVSDK_NGX_Parameter_OutWidth, pDlispCreateParams->InTargetWidth);
    NVSDK_NGX_Parameter_SetUI(pInParams, NVSDK_NGX_Parameter_OutHeight, pDlispCreateParams->InTargetHeight);

    return NVSDK_NGX_D3D11_CreateFeature(pInCtx, NVSDK_NGX_Feature_ImageSignalProcessing, pInParams, ppOutHandle);
}

static inline NVSDK_NGX_Result NGX_D3D11_EVALUATE_DLISP_EXT(
    ID3D11DeviceContext *pInCtx,
    NVSDK_NGX_Handle *pInHandle,
    NVSDK_NGX_Parameter *pInParams,
    NVSDK_NGX_D3D11_DLISP_Eval_Params *pDlispEvalParams)
{
    if (pDlispEvalParams->Feature.InSharpness < 0.0f || pDlispEvalParams->Feature.InSharpness > 1.0f || pDlispEvalParams->InDenoise < 0.0f || pDlispEvalParams->InDenoise > 1.0f)
    {
        return NVSDK_NGX_Result_FAIL_InvalidParameter;
    }
    NVSDK_NGX_Parameter_SetD3d11Resource(pInParams, NVSDK_NGX_Parameter_Color, pDlispEvalParams->Feature.pInColor);
    NVSDK_NGX_Parameter_SetD3d11Resource(pInParams, NVSDK_NGX_Parameter_Output, pDlispEvalParams->Feature.pInOutput);
    // Both sharpness and denoise in range [0.0f,1.0f]
    NVSDK_NGX_Parameter_SetF(pInParams, NVSDK_NGX_Parameter_Sharpness, pDlispEvalParams->Feature.InSharpness);
    NVSDK_NGX_Parameter_SetF(pInParams, NVSDK_NGX_Parameter_Denoise, pDlispEvalParams->InDenoise);
    // If input is atlas - use RECT to upscale only the required area
    if (pDlispEvalParams->InRectW)
    {
        NVSDK_NGX_Parameter_SetUI(pInParams, NVSDK_NGX_Parameter_Rect_X, pDlispEvalParams->InRectX);
        NVSDK_NGX_Parameter_SetUI(pInParams, NVSDK_NGX_Parameter_Rect_Y, pDlispEvalParams->InRectY);
        NVSDK_NGX_Parameter_SetUI(pInParams, NVSDK_NGX_Parameter_Rect_W, pDlispEvalParams->InRectW);
        NVSDK_NGX_Parameter_SetUI(pInParams, NVSDK_NGX_Parameter_Rect_H, pDlispEvalParams->InRectH);
    }

    return NVSDK_NGX_D3D11_EvaluateFeature_C(pInCtx, pInHandle, pInParams, NULL);
}


/*** D3D12 ***/
typedef struct NVSDK_NGX_D3D12_Feature_Eval_Params
{
    ID3D12Resource*  pInColor;
    ID3D12Resource*  pInOutput;
    /*** OPTIONAL for DLSS ***/
    float       InSharpness;
} NVSDK_NGX_D3D12_Feature_Eval_Params;

typedef struct NVSDK_NGX_D3D12_GBuffer
{
    ID3D12Resource* pInAttrib[NVSDK_NGX_GBUFFERTYPE_NUM];
} NVSDK_NGX_D3D12_GBuffer;

typedef struct NVSDK_NGX_D3D12_DLSS_Eval_Params
{
    NVSDK_NGX_D3D12_Feature_Eval_Params Feature;
    ID3D12Resource*                     pInDepth;
    ID3D12Resource*                     pInMotionVectors;
    float                               InJitterOffsetX;     /* Jitter offset must be in input/render pixel space */
    float                               InJitterOffsetY;
    NVSDK_NGX_Dimensions                InRenderSubrectDimensions;
    /*** OPTIONAL - leave to 0/0.0f if unused ***/
    int                                 InReset;             /* Set to 1 when scene changes completely (new level etc) */
    float                               InMVScaleX;          /* If MVs need custom scaling to convert to pixel space */
    float                               InMVScaleY;
    ID3D12Resource*                     pInTransparencyMask; /* Unused/Reserved for future use */
    ID3D12Resource*                     pInExposureTexture;
    ID3D12Resource*                     pInBiasCurrentColorMask;
    NVSDK_NGX_Coordinates               InColorSubrectBase;
    NVSDK_NGX_Coordinates               InDepthSubrectBase;
    NVSDK_NGX_Coordinates               InMVSubrectBase;
    NVSDK_NGX_Coordinates               InTranslucencySubrectBase;
    NVSDK_NGX_Coordinates               InBiasCurrentColorSubrectBase;
    NVSDK_NGX_Coordinates               InOutputSubrectBase;
    float                               InPreExposure;
    float                               InExposureScale;
    int                                 InIndicatorInvertXAxis;
    int                                 InIndicatorInvertYAxis;
    /*** OPTIONAL - only for research purposes ***/
    NVSDK_NGX_D3D12_GBuffer             GBufferSurface;
    NVSDK_NGX_ToneMapperType            InToneMapperType;
    ID3D12Resource*                     pInMotionVectors3D;
    ID3D12Resource*                     pInIsParticleMask; /* to identify which pixels contains particles, essentially that are not drawn as part of base pass */
    ID3D12Resource*                     pInAnimatedTextureMask; /* a binary mask covering pixels occupied by animated textures */
    ID3D12Resource*                     pInDepthHighRes;
    ID3D12Resource*                     pInPositionViewSpace;
    float                               InFrameTimeDeltaInMsec; /* helps in determining the amount to denoise or anti-alias based on the speed of the object from motion vector magnitudes and fps as determined by this delta */
    ID3D12Resource*                     pInRayTracingHitDistance; /* for each effect - approximation to the amount of noise in a ray-traced color */
    ID3D12Resource*                     pInMotionVectorsReflections; /* motion vectors of reflected objects like for mirrored surfaces */
} NVSDK_NGX_D3D12_DLSS_Eval_Params;

typedef struct NVSDK_NGX_D3D12_DLISP_Eval_Params
{
    NVSDK_NGX_D3D12_Feature_Eval_Params Feature;
    /*** OPTIONAL ***/
    unsigned int                        InRectX;
    unsigned int                        InRectY;
    unsigned int                        InRectW;
    unsigned int                        InRectH;
    float                               InDenoise;
} NVSDK_NGX_D3D12_DLISP_Eval_Params;


static inline NVSDK_NGX_Result NGX_D3D12_CREATE_DLSS_EXT(
    ID3D12GraphicsCommandList *pInCmdList,
    unsigned int InCreationNodeMask,
    unsigned int InVisibilityNodeMask,
    NVSDK_NGX_Handle **ppOutHandle,
    NVSDK_NGX_Parameter *pInParams,
    NVSDK_NGX_DLSS_Create_Params *pInDlssCreateParams)
{
    NVSDK_NGX_Parameter_SetUI(pInParams, NVSDK_NGX_Parameter_CreationNodeMask, InCreationNodeMask);
    NVSDK_NGX_Parameter_SetUI(pInParams, NVSDK_NGX_Parameter_VisibilityNodeMask, InVisibilityNodeMask);
    NVSDK_NGX_Parameter_SetUI(pInParams, NVSDK_NGX_Parameter_Width, pInDlssCreateParams->Feature.InWidth);
    NVSDK_NGX_Parameter_SetUI(pInParams, NVSDK_NGX_Parameter_Height, pInDlssCreateParams->Feature.InHeight);
    NVSDK_NGX_Parameter_SetUI(pInParams, NVSDK_NGX_Parameter_OutWidth, pInDlssCreateParams->Feature.InTargetWidth);
    NVSDK_NGX_Parameter_SetUI(pInParams, NVSDK_NGX_Parameter_OutHeight, pInDlssCreateParams->Feature.InTargetHeight);
    NVSDK_NGX_Parameter_SetI(pInParams, NVSDK_NGX_Parameter_PerfQualityValue, pInDlssCreateParams->Feature.InPerfQualityValue);
    NVSDK_NGX_Parameter_SetI(pInParams, NVSDK_NGX_Parameter_DLSS_Feature_Create_Flags, pInDlssCreateParams->InFeatureCreateFlags);
    NVSDK_NGX_Parameter_SetI(pInParams, NVSDK_NGX_Parameter_DLSS_Enable_Output_Subrects, pInDlssCreateParams->InEnableOutputSubrects ? 1 : 0);

    return NVSDK_NGX_D3D12_CreateFeature(pInCmdList, NVSDK_NGX_Feature_SuperSampling, pInParams, ppOutHandle);
}


static inline NVSDK_NGX_Result NGX_D3D12_EVALUATE_DLSS_EXT(
    ID3D12GraphicsCommandList *pInCmdList,
    NVSDK_NGX_Handle *pInHandle,
    NVSDK_NGX_Parameter *pInParams,
    NVSDK_NGX_D3D12_DLSS_Eval_Params *pInDlssEvalParams)
{
    NVSDK_NGX_Parameter_SetD3d12Resource(pInParams, NVSDK_NGX_Parameter_Color, pInDlssEvalParams->Feature.pInColor);
    NVSDK_NGX_Parameter_SetD3d12Resource(pInParams, NVSDK_NGX_Parameter_Output, pInDlssEvalParams->Feature.pInOutput);
    NVSDK_NGX_Parameter_SetD3d12Resource(pInParams, NVSDK_NGX_Parameter_Depth, pInDlssEvalParams->pInDepth);
    NVSDK_NGX_Parameter_SetD3d12Resource(pInParams, NVSDK_NGX_Parameter_MotionVectors, pInDlssEvalParams->pInMotionVectors);
    NVSDK_NGX_Parameter_SetF(pInParams, NVSDK_NGX_Parameter_Jitter_Offset_X, pInDlssEvalParams->InJitterOffsetX);
    NVSDK_NGX_Parameter_SetF(pInParams, NVSDK_NGX_Parameter_Jitter_Offset_Y, pInDlssEvalParams->InJitterOffsetY);
    NVSDK_NGX_Parameter_SetF(pInParams, NVSDK_NGX_Parameter_Sharpness, pInDlssEvalParams->Feature.InSharpness);
    NVSDK_NGX_Parameter_SetI(pInParams, NVSDK_NGX_Parameter_Reset, pInDlssEvalParams->InReset);
    NVSDK_NGX_Parameter_SetF(pInParams, NVSDK_NGX_Parameter_MV_Scale_X, pInDlssEvalParams->InMVScaleX == 0.0f ? 1.0f : pInDlssEvalParams->InMVScaleX);
    NVSDK_NGX_Parameter_SetF(pInParams, NVSDK_NGX_Parameter_MV_Scale_Y, pInDlssEvalParams->InMVScaleY == 0.0f ? 1.0f : pInDlssEvalParams->InMVScaleY);
    NVSDK_NGX_Parameter_SetD3d12Resource(pInParams, NVSDK_NGX_Parameter_TransparencyMask, pInDlssEvalParams->pInTransparencyMask);
    NVSDK_NGX_Parameter_SetD3d12Resource(pInParams, NVSDK_NGX_Parameter_ExposureTexture, pInDlssEvalParams->pInExposureTexture);
    NVSDK_NGX_Parameter_SetD3d12Resource(pInParams, NVSDK_NGX_Parameter_DLSS_Input_Bias_Current_Color_Mask, pInDlssEvalParams->pInBiasCurrentColorMask);
    NVSDK_NGX_Parameter_SetD3d12Resource(pInParams, NVSDK_NGX_Parameter_GBuffer_Albedo, pInDlssEvalParams->GBufferSurface.pInAttrib[NVSDK_NGX_GBUFFER_ALBEDO]);
    NVSDK_NGX_Parameter_SetD3d12Resource(pInParams, NVSDK_NGX_Parameter_GBuffer_Roughness, pInDlssEvalParams->GBufferSurface.pInAttrib[NVSDK_NGX_GBUFFER_ROUGHNESS]);
    NVSDK_NGX_Parameter_SetD3d12Resource(pInParams, NVSDK_NGX_Parameter_GBuffer_Metallic, pInDlssEvalParams->GBufferSurface.pInAttrib[NVSDK_NGX_GBUFFER_METALLIC]);
    NVSDK_NGX_Parameter_SetD3d12Resource(pInParams, NVSDK_NGX_Parameter_GBuffer_Specular, pInDlssEvalParams->GBufferSurface.pInAttrib[NVSDK_NGX_GBUFFER_SPECULAR]);
    NVSDK_NGX_Parameter_SetD3d12Resource(pInParams, NVSDK_NGX_Parameter_GBuffer_Subsurface, pInDlssEvalParams->GBufferSurface.pInAttrib[NVSDK_NGX_GBUFFER_SUBSURFACE]);
    NVSDK_NGX_Parameter_SetD3d12Resource(pInParams, NVSDK_NGX_Parameter_GBuffer_Normals, pInDlssEvalParams->GBufferSurface.pInAttrib[NVSDK_NGX_GBUFFER_NORMALS]);
    NVSDK_NGX_Parameter_SetD3d12Resource(pInParams, NVSDK_NGX_Parameter_GBuffer_ShadingModelId, pInDlssEvalParams->GBufferSurface.pInAttrib[NVSDK_NGX_GBUFFER_SHADINGMODELID]);
    NVSDK_NGX_Parameter_SetD3d12Resource(pInParams, NVSDK_NGX_Parameter_GBuffer_MaterialId, pInDlssEvalParams->GBufferSurface.pInAttrib[NVSDK_NGX_GBUFFER_MATERIALID]);
    NVSDK_NGX_Parameter_SetD3d12Resource(pInParams, NVSDK_NGX_Parameter_GBuffer_Atrrib_8, pInDlssEvalParams->GBufferSurface.pInAttrib[8]);
    NVSDK_NGX_Parameter_SetD3d12Resource(pInParams, NVSDK_NGX_Parameter_GBuffer_Atrrib_9, pInDlssEvalParams->GBufferSurface.pInAttrib[9]);
    NVSDK_NGX_Parameter_SetD3d12Resource(pInParams, NVSDK_NGX_Parameter_GBuffer_Atrrib_10, pInDlssEvalParams->GBufferSurface.pInAttrib[10]);
    NVSDK_NGX_Parameter_SetD3d12Resource(pInParams, NVSDK_NGX_Parameter_GBuffer_Atrrib_11, pInDlssEvalParams->GBufferSurface.pInAttrib[11]);
    NVSDK_NGX_Parameter_SetD3d12Resource(pInParams, NVSDK_NGX_Parameter_GBuffer_Atrrib_12, pInDlssEvalParams->GBufferSurface.pInAttrib[12]);
    NVSDK_NGX_Parameter_SetD3d12Resource(pInParams, NVSDK_NGX_Parameter_GBuffer_Atrrib_13, pInDlssEvalParams->GBufferSurface.pInAttrib[13]);
    NVSDK_NGX_Parameter_SetD3d12Resource(pInParams, NVSDK_NGX_Parameter_GBuffer_Atrrib_14, pInDlssEvalParams->GBufferSurface.pInAttrib[14]);
    NVSDK_NGX_Parameter_SetD3d12Resource(pInParams, NVSDK_NGX_Parameter_GBuffer_Atrrib_15, pInDlssEvalParams->GBufferSurface.pInAttrib[15]);
    NVSDK_NGX_Parameter_SetUI(pInParams, NVSDK_NGX_Parameter_TonemapperType, pInDlssEvalParams->InToneMapperType);
    NVSDK_NGX_Parameter_SetD3d12Resource(pInParams, NVSDK_NGX_Parameter_MotionVectors3D, pInDlssEvalParams->pInMotionVectors3D);
    NVSDK_NGX_Parameter_SetD3d12Resource(pInParams, NVSDK_NGX_Parameter_IsParticleMask, pInDlssEvalParams->pInIsParticleMask);
    NVSDK_NGX_Parameter_SetD3d12Resource(pInParams, NVSDK_NGX_Parameter_AnimatedTextureMask, pInDlssEvalParams->pInAnimatedTextureMask);
    NVSDK_NGX_Parameter_SetD3d12Resource(pInParams, NVSDK_NGX_Parameter_DepthHighRes, pInDlssEvalParams->pInDepthHighRes);
    NVSDK_NGX_Parameter_SetD3d12Resource(pInParams, NVSDK_NGX_Parameter_Position_ViewSpace, pInDlssEvalParams->pInPositionViewSpace);
    NVSDK_NGX_Parameter_SetF(pInParams, NVSDK_NGX_Parameter_FrameTimeDeltaInMsec, pInDlssEvalParams->InFrameTimeDeltaInMsec);
    NVSDK_NGX_Parameter_SetD3d12Resource(pInParams, NVSDK_NGX_Parameter_RayTracingHitDistance, pInDlssEvalParams->pInRayTracingHitDistance);
    NVSDK_NGX_Parameter_SetD3d12Resource(pInParams, NVSDK_NGX_Parameter_MotionVectorsReflection, pInDlssEvalParams->pInMotionVectorsReflections);
    NVSDK_NGX_Parameter_SetUI(pInParams, NVSDK_NGX_Parameter_DLSS_Input_Color_Subrect_Base_X, pInDlssEvalParams->InColorSubrectBase.X);
    NVSDK_NGX_Parameter_SetUI(pInParams, NVSDK_NGX_Parameter_DLSS_Input_Color_Subrect_Base_Y, pInDlssEvalParams->InColorSubrectBase.Y);
    NVSDK_NGX_Parameter_SetUI(pInParams, NVSDK_NGX_Parameter_DLSS_Input_Depth_Subrect_Base_X, pInDlssEvalParams->InDepthSubrectBase.X);
    NVSDK_NGX_Parameter_SetUI(pInParams, NVSDK_NGX_Parameter_DLSS_Input_Depth_Subrect_Base_Y, pInDlssEvalParams->InDepthSubrectBase.Y);
    NVSDK_NGX_Parameter_SetUI(pInParams, NVSDK_NGX_Parameter_DLSS_Input_MV_SubrectBase_X, pInDlssEvalParams->InMVSubrectBase.X);
    NVSDK_NGX_Parameter_SetUI(pInParams, NVSDK_NGX_Parameter_DLSS_Input_MV_SubrectBase_Y, pInDlssEvalParams->InMVSubrectBase.Y);
    NVSDK_NGX_Parameter_SetUI(pInParams, NVSDK_NGX_Parameter_DLSS_Input_Translucency_SubrectBase_X, pInDlssEvalParams->InTranslucencySubrectBase.X);
    NVSDK_NGX_Parameter_SetUI(pInParams, NVSDK_NGX_Parameter_DLSS_Input_Translucency_SubrectBase_Y, pInDlssEvalParams->InTranslucencySubrectBase.Y);
    NVSDK_NGX_Parameter_SetUI(pInParams, NVSDK_NGX_Parameter_DLSS_Input_Bias_Current_Color_SubrectBase_X, pInDlssEvalParams->InBiasCurrentColorSubrectBase.X);
    NVSDK_NGX_Parameter_SetUI(pInParams, NVSDK_NGX_Parameter_DLSS_Input_Bias_Current_Color_SubrectBase_Y, pInDlssEvalParams->InBiasCurrentColorSubrectBase.Y);
    NVSDK_NGX_Parameter_SetUI(pInParams, NVSDK_NGX_Parameter_DLSS_Output_Subrect_Base_X, pInDlssEvalParams->InOutputSubrectBase.X);
    NVSDK_NGX_Parameter_SetUI(pInParams, NVSDK_NGX_Parameter_DLSS_Output_Subrect_Base_Y, pInDlssEvalParams->InOutputSubrectBase.Y);
    NVSDK_NGX_Parameter_SetUI(pInParams, NVSDK_NGX_Parameter_DLSS_Render_Subrect_Dimensions_Width , pInDlssEvalParams->InRenderSubrectDimensions.Width);
    NVSDK_NGX_Parameter_SetUI(pInParams, NVSDK_NGX_Parameter_DLSS_Render_Subrect_Dimensions_Height, pInDlssEvalParams->InRenderSubrectDimensions.Height);
    NVSDK_NGX_Parameter_SetF(pInParams, NVSDK_NGX_Parameter_DLSS_Pre_Exposure, pInDlssEvalParams->InPreExposure == 0.0f ? 1.0f : pInDlssEvalParams->InPreExposure);
    NVSDK_NGX_Parameter_SetF(pInParams, NVSDK_NGX_Parameter_DLSS_Exposure_Scale, pInDlssEvalParams->InExposureScale == 0.0f ? 1.0f : pInDlssEvalParams->InExposureScale);
    NVSDK_NGX_Parameter_SetI(pInParams, NVSDK_NGX_Parameter_DLSS_Indicator_Invert_X_Axis, pInDlssEvalParams->InIndicatorInvertXAxis);
    NVSDK_NGX_Parameter_SetI(pInParams, NVSDK_NGX_Parameter_DLSS_Indicator_Invert_Y_Axis, pInDlssEvalParams->InIndicatorInvertYAxis);

    return NVSDK_NGX_D3D12_EvaluateFeature_C(pInCmdList, pInHandle, pInParams, NULL);
}

static inline NVSDK_NGX_Result NGX_D3D12_CREATE_DLISP_EXT(
    ID3D12GraphicsCommandList *InCmdList,
    unsigned int InCreationNodeMask,
    unsigned int InVisibilityNodeMask,
    NVSDK_NGX_Handle **ppOutHandle,
    NVSDK_NGX_Parameter *pInParams,
    NVSDK_NGX_Feature_Create_Params *pDlispCreateParams)
{
    NVSDK_NGX_Parameter_SetUI(pInParams, NVSDK_NGX_Parameter_CreationNodeMask, InCreationNodeMask);
    NVSDK_NGX_Parameter_SetUI(pInParams, NVSDK_NGX_Parameter_VisibilityNodeMask, InVisibilityNodeMask);
    NVSDK_NGX_Parameter_SetUI(pInParams, NVSDK_NGX_Parameter_Width, pDlispCreateParams->InWidth);
    NVSDK_NGX_Parameter_SetUI(pInParams, NVSDK_NGX_Parameter_Height, pDlispCreateParams->InHeight);
    NVSDK_NGX_Parameter_SetUI(pInParams, NVSDK_NGX_Parameter_OutWidth, pDlispCreateParams->InTargetWidth);
    NVSDK_NGX_Parameter_SetUI(pInParams, NVSDK_NGX_Parameter_OutHeight, pDlispCreateParams->InTargetHeight);
    NVSDK_NGX_Parameter_SetI(pInParams, NVSDK_NGX_Parameter_PerfQualityValue, pDlispCreateParams->InPerfQualityValue);

    return NVSDK_NGX_D3D12_CreateFeature(InCmdList, NVSDK_NGX_Feature_ImageSignalProcessing, pInParams, ppOutHandle);
}

static inline NVSDK_NGX_Result NGX_D3D12_EVALUATE_DLISP_EXT(
    ID3D12GraphicsCommandList *pInCmdList,
    NVSDK_NGX_Handle *pInHandle,
    NVSDK_NGX_Parameter *pInParams,
    NVSDK_NGX_D3D12_DLISP_Eval_Params *pDlispEvalParams)
{
    if (pDlispEvalParams->Feature.InSharpness < 0.0f || pDlispEvalParams->Feature.InSharpness > 1.0f || pDlispEvalParams->InDenoise < 0.0f || pDlispEvalParams->InDenoise > 1.0f)
    {
        return NVSDK_NGX_Result_FAIL_InvalidParameter;
    }
    NVSDK_NGX_Parameter_SetD3d12Resource(pInParams, NVSDK_NGX_Parameter_Color, pDlispEvalParams->Feature.pInColor);
    NVSDK_NGX_Parameter_SetD3d12Resource(pInParams, NVSDK_NGX_Parameter_Output, pDlispEvalParams->Feature.pInOutput);
    // Both sharpness and denoise in range [0.0f,1.0f]
    NVSDK_NGX_Parameter_SetF(pInParams, NVSDK_NGX_Parameter_Sharpness, pDlispEvalParams->Feature.InSharpness);
    NVSDK_NGX_Parameter_SetF(pInParams, NVSDK_NGX_Parameter_Denoise, pDlispEvalParams->InDenoise);
    // If input is atlas - use RECT to upscale only the required area
    if (pDlispEvalParams->InRectW)
    {
        NVSDK_NGX_Parameter_SetUI(pInParams, NVSDK_NGX_Parameter_Rect_X, pDlispEvalParams->InRectX);
        NVSDK_NGX_Parameter_SetUI(pInParams, NVSDK_NGX_Parameter_Rect_Y, pDlispEvalParams->InRectY);
        NVSDK_NGX_Parameter_SetUI(pInParams, NVSDK_NGX_Parameter_Rect_W, pDlispEvalParams->InRectW);
        NVSDK_NGX_Parameter_SetUI(pInParams, NVSDK_NGX_Parameter_Rect_H, pDlispEvalParams->InRectH);
    }
    return NVSDK_NGX_D3D12_EvaluateFeature_C(pInCmdList, pInHandle, pInParams, NULL);
}
#endif // NVSDK_NGX_HELPERS_D3D_H
