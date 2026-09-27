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

#ifndef NVSDK_NGX_HELPERS_CUDA_H
#define NVSDK_NGX_HELPERS_CUDA_H
#pragma once

#include "nvsdk_ngx.h"
#include "nvsdk_ngx_defs.h"

typedef struct NVSDK_NGX_CUDA_Feature_Eval_Params
{
    CUtexObject* pInColor;
    CUtexObject* pInOutput;
    /*** OPTIONAL for DLSS ***/
    float       InSharpness;
} NVSDK_NGX_CUDA_Feature_Eval_Params;

typedef struct NVSDK_NGX_CUDA_DLISP_Eval_Params
{
    NVSDK_NGX_CUDA_Feature_Eval_Params Feature;
    /*** OPTIONAL - leave to 0/0.0f if unused  ***/
    unsigned int                    InRectX;
    unsigned int                    InRectY;
    unsigned int                    InRectW;
    unsigned int                    InRectH;
    float                           InDenoise;
} NVSDK_NGX_CUDA_DLISP_Eval_Params;
static inline NVSDK_NGX_Result NGX_CUDA_CREATE_DLISP_EXT(
    NVSDK_NGX_Handle** ppOutHandle,
    NVSDK_NGX_Parameter* pInParams,
    NVSDK_NGX_Feature_Create_Params* pDlispCreateParams)
{
    NVSDK_NGX_Parameter_SetUI(pInParams, NVSDK_NGX_Parameter_Width, pDlispCreateParams->InWidth);
    NVSDK_NGX_Parameter_SetUI(pInParams, NVSDK_NGX_Parameter_Height, pDlispCreateParams->InHeight);
    NVSDK_NGX_Parameter_SetUI(pInParams, NVSDK_NGX_Parameter_OutWidth, pDlispCreateParams->InTargetWidth);
    NVSDK_NGX_Parameter_SetUI(pInParams, NVSDK_NGX_Parameter_OutHeight, pDlispCreateParams->InTargetHeight);

    return NVSDK_NGX_CUDA_CreateFeature(NVSDK_NGX_Feature_ImageSignalProcessing, pInParams, ppOutHandle);
}
static inline NVSDK_NGX_Result NGX_CUDA_EVALUATE_DLISP_EXT(
    NVSDK_NGX_Handle *pInHandle,
    NVSDK_NGX_Parameter *pInParams,
    NVSDK_NGX_CUDA_DLISP_Eval_Params *pDlispEvalParams)
{
    if (pDlispEvalParams->Feature.InSharpness < 0.0f || pDlispEvalParams->Feature.InSharpness > 1.0f || pDlispEvalParams->InDenoise < 0.0f || pDlispEvalParams->InDenoise > 1.0f)
    {
        return NVSDK_NGX_Result_FAIL_InvalidParameter;
    }
    NVSDK_NGX_Parameter_SetVoidPointer(pInParams, NVSDK_NGX_Parameter_Color, pDlispEvalParams->Feature.pInColor);
    NVSDK_NGX_Parameter_SetVoidPointer(pInParams, NVSDK_NGX_Parameter_Output, pDlispEvalParams->Feature.pInOutput);
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

    return NVSDK_NGX_CUDA_EvaluateFeature_C(pInHandle, pInParams, NULL);
}
#endif // NVSDK_NGX_HELPERS_CUDA_H
