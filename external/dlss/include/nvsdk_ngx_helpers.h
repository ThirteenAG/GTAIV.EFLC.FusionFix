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

#ifndef NVSDK_NGX_HELPERS_H
#define NVSDK_NGX_HELPERS_H
#pragma once

#include "nvsdk_ngx.h"
#include "nvsdk_ngx_defs.h"

static inline NVSDK_NGX_Result NGX_DLSS_GET_STATS_2(
    NVSDK_NGX_Parameter *pInParams,
    unsigned long long *pVRAMAllocatedBytes,
    unsigned int *pOptLevel, unsigned int *IsDevSnippetBranch)
{
    void *Callback = NULL;
    NVSDK_NGX_Parameter_GetVoidPointer(pInParams, NVSDK_NGX_Parameter_DLSSGetStatsCallback, &Callback);
    if (!Callback)
    {
        // Possible reasons for this:
        // - Installed DLSS is out of date and does not support the feature we need
        // - You used NVSDK_NGX_AllocateParameters() for creating InParams. Try using NVSDK_NGX_GetCapabilityParameters() instead
        return NVSDK_NGX_Result_FAIL_OutOfDate;
    }

    NVSDK_NGX_Result Res = NVSDK_NGX_Result_Success;
    PFN_NVSDK_NGX_DLSS_GetStatsCallback PFNCallback = (PFN_NVSDK_NGX_DLSS_GetStatsCallback)Callback;
    Res = PFNCallback(pInParams);
    if (NVSDK_NGX_FAILED(Res))
    {
        return Res;
    }
    NVSDK_NGX_Parameter_GetULL(pInParams, NVSDK_NGX_Parameter_SizeInBytes, pVRAMAllocatedBytes);
    NVSDK_NGX_Parameter_GetUI(pInParams, NVSDK_NGX_EParameter_OptLevel, pOptLevel);
    NVSDK_NGX_Parameter_GetUI(pInParams, NVSDK_NGX_EParameter_IsDevSnippetBranch, IsDevSnippetBranch);
    return Res;
}

static inline NVSDK_NGX_Result NGX_DLSS_GET_STATS_1(
    NVSDK_NGX_Parameter *pInParams,
    unsigned long long *pVRAMAllocatedBytes,
    unsigned int *pOptLevel)
{
    unsigned int dummy = 0;
    return NGX_DLSS_GET_STATS_2(pInParams, pVRAMAllocatedBytes, pOptLevel, &dummy);
}

static inline NVSDK_NGX_Result NGX_DLSS_GET_STATS(
    NVSDK_NGX_Parameter *pInParams,
    unsigned long long *pVRAMAllocatedBytes)
{
    unsigned int dummy = 0;
    return NGX_DLSS_GET_STATS_2(pInParams, pVRAMAllocatedBytes, &dummy, &dummy);
}

static inline NVSDK_NGX_Result NGX_DLSS_GET_OPTIMAL_SETTINGS(
    NVSDK_NGX_Parameter *pInParams,
    unsigned int InUserSelectedWidth,
    unsigned int InUserSelectedHeight,
    NVSDK_NGX_PerfQuality_Value InPerfQualityValue,
    unsigned int *pOutRenderOptimalWidth,
    unsigned int *pOutRenderOptimalHeight,
    unsigned int *pOutRenderMaxWidth,
    unsigned int *pOutRenderMaxHeight,
    unsigned int *pOutRenderMinWidth,
    unsigned int *pOutRenderMinHeight,
    float *pOutSharpness)
{
    void *Callback = NULL;
    NVSDK_NGX_Parameter_GetVoidPointer(pInParams, NVSDK_NGX_Parameter_DLSSOptimalSettingsCallback, &Callback);
    if (!Callback)
    {
        // Possible reasons for this:
        // - Installed DLSS is out of date and does not support the feature we need
        // - You used NVSDK_NGX_AllocateParameters() for creating InParams. Try using NVSDK_NGX_GetCapabilityParameters() instead
        return NVSDK_NGX_Result_FAIL_OutOfDate;
    }

    // These are selections made by user in UI
    NVSDK_NGX_Parameter_SetUI(pInParams, NVSDK_NGX_Parameter_Width, InUserSelectedWidth);
    NVSDK_NGX_Parameter_SetUI(pInParams, NVSDK_NGX_Parameter_Height, InUserSelectedHeight);
    NVSDK_NGX_Parameter_SetI(pInParams, NVSDK_NGX_Parameter_PerfQualityValue, InPerfQualityValue);
    NVSDK_NGX_Parameter_SetI(pInParams, NVSDK_NGX_Parameter_RTXValue, false); // Some older DLSS dlls still expect this value to be set

    NVSDK_NGX_Result Res = NVSDK_NGX_Result_Success;
    PFN_NVSDK_NGX_DLSS_GetOptimalSettingsCallback PFNCallback = (PFN_NVSDK_NGX_DLSS_GetOptimalSettingsCallback)Callback;
    Res = PFNCallback(pInParams);
    if (NVSDK_NGX_FAILED(Res))
    {
        return Res;
    }
    NVSDK_NGX_Parameter_GetUI(pInParams, NVSDK_NGX_Parameter_OutWidth, pOutRenderOptimalWidth);
    NVSDK_NGX_Parameter_GetUI(pInParams, NVSDK_NGX_Parameter_OutHeight, pOutRenderOptimalHeight);
    // If we have an older DLSS Dll those might need to be set to the optimal dimensions instead
    *pOutRenderMaxWidth  = *pOutRenderOptimalWidth;
    *pOutRenderMaxHeight = *pOutRenderOptimalHeight;
    *pOutRenderMinWidth  = *pOutRenderOptimalWidth;
    *pOutRenderMinHeight = *pOutRenderOptimalHeight;
    NVSDK_NGX_Parameter_GetUI(pInParams, NVSDK_NGX_Parameter_DLSS_Get_Dynamic_Max_Render_Width,  pOutRenderMaxWidth);
    NVSDK_NGX_Parameter_GetUI(pInParams, NVSDK_NGX_Parameter_DLSS_Get_Dynamic_Max_Render_Height, pOutRenderMaxHeight);
    NVSDK_NGX_Parameter_GetUI(pInParams, NVSDK_NGX_Parameter_DLSS_Get_Dynamic_Min_Render_Width,  pOutRenderMinWidth);
    NVSDK_NGX_Parameter_GetUI(pInParams, NVSDK_NGX_Parameter_DLSS_Get_Dynamic_Min_Render_Height, pOutRenderMinHeight);
    NVSDK_NGX_Parameter_GetF(pInParams, NVSDK_NGX_Parameter_Sharpness, pOutSharpness);
    return Res;
}

#include "nvsdk_ngx_helpers_d3d.h"
#include "nvsdk_ngx_helpers_cuda.h"

#endif // NVSDK_NGX_HELPERS_H
