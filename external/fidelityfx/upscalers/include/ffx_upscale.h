// This file is part of the FidelityFX SDK.
//
// Copyright (C) 2026 Advanced Micro Devices, Inc.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files(the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and /or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions :
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.

#pragma once
#include "../../api/include/ffx_api.h"
#include "../../api/include/ffx_api_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define FFX_UPSCALER_VERSION_MAJOR 4
#define FFX_UPSCALER_VERSION_MINOR 1
#define FFX_UPSCALER_VERSION_PATCH 1

#define FFX_UPSCALER_MAKE_VERSION(major, minor, patch) (((major) << 22) | ((minor) << 12) | (patch))
#define FFX_UPSCALER_VERSION                           FFX_UPSCALER_MAKE_VERSION(FFX_UPSCALER_VERSION_MAJOR, FFX_UPSCALER_VERSION_MINOR, FFX_UPSCALER_VERSION_PATCH)

enum FfxApiUpscaleQualityMode
{
    FFX_UPSCALE_QUALITY_MODE_NATIVEAA          = 0,  ///< Perform upscaling with a per-dimension upscaling ratio of 1.0x.
    FFX_UPSCALE_QUALITY_MODE_QUALITY           = 1,  ///< Perform upscaling with a per-dimension upscaling ratio of 1.5x.
    FFX_UPSCALE_QUALITY_MODE_BALANCED          = 2,  ///< Perform upscaling with a per-dimension upscaling ratio of 1.7x.
    FFX_UPSCALE_QUALITY_MODE_PERFORMANCE       = 3,  ///< Perform upscaling with a per-dimension upscaling ratio of 2.0x.
    FFX_UPSCALE_QUALITY_MODE_ULTRA_PERFORMANCE = 4   ///< Perform upscaling with a per-dimension upscaling ratio of 3.0x.
};

enum FfxApiCreateContextUpscaleFlags
{
    FFX_UPSCALE_ENABLE_HIGH_DYNAMIC_RANGE                 = (1 << 0),  ///< A bit indicating if the input color data provided is using a high-dynamic range.
    FFX_UPSCALE_ENABLE_DISPLAY_RESOLUTION_MOTION_VECTORS  = (1 << 1),  ///< A bit indicating if the motion vectors are rendered at display resolution.
    FFX_UPSCALE_ENABLE_MOTION_VECTORS_JITTER_CANCELLATION = (1 << 2),  ///< A bit indicating that the motion vectors have the jittering pattern applied to them.
    FFX_UPSCALE_ENABLE_DEPTH_INVERTED                     = (1 << 3),  ///< A bit indicating that the input depth buffer data provided is inverted [1..0].
    FFX_UPSCALE_ENABLE_DEPTH_INFINITE                     = (1 << 4),  ///< A bit indicating that the input depth buffer data provided is using an infinite far plane.
    FFX_UPSCALE_ENABLE_AUTO_EXPOSURE                      = (1 << 5),  ///< A bit indicating if automatic exposure should be applied to input color data.
    FFX_UPSCALE_ENABLE_DYNAMIC_RESOLUTION                 = (1 << 6),  ///< A bit indicating that the application uses dynamic resolution scaling.
    FFX_UPSCALE_ENABLE_DEBUG_CHECKING                     = (1 << 7),  ///< A bit indicating that the runtime should check some API values and report issues.
    FFX_UPSCALE_ENABLE_NON_LINEAR_COLORSPACE              = (1 << 8),  ///< A bit indicating that the color resource contains perceptual (gamma corrected) colors
    FFX_UPSCALE_ENABLE_DEBUG_VISUALIZATION                = (1 << 9),  ///< A bit indicating if debug visualization is allowed. (memory consumption could increase)
};

enum FfxApiDispatchFsrUpscaleFlags
{
    FFX_UPSCALE_FLAG_DRAW_DEBUG_VIEW       = (1 << 0),  ///< A bit indicating that the output resource will contain debug views with relevant information.
    FFX_UPSCALE_FLAG_NON_LINEAR_COLOR_SRGB = (1 << 1),  ///< A bit indicating that the input color resource contains perceptual sRGB colors
    FFX_UPSCALE_FLAG_NON_LINEAR_COLOR_PQ   = (1 << 2),  ///< A bit indicating that the input color resource contains perceptual PQ colors
};

enum FfxApiDispatchUpscaleAutoreactiveFlags
{
    FFX_UPSCALE_AUTOREACTIVEFLAGS_APPLY_TONEMAP        = (1 << 0),  ///< Apply tonemapping function prior to calculating reactivity.
    FFX_UPSCALE_AUTOREACTIVEFLAGS_APPLY_INVERSETONEMAP = (1 << 1),  ///< Apply inverse tonemapping function prior to calculating reactivity.
    FFX_UPSCALE_AUTOREACTIVEFLAGS_APPLY_THRESHOLD      = (1 << 2),  ///< Compare to a threshold value and only record reactivity above the threshold.
    FFX_UPSCALE_AUTOREACTIVEFLAGS_USE_COMPONENTS_MAX   = (1 << 3),  ///< When enabled use the max. component value, otherwise use the length to calculate reactivity.
};

#define FFX_API_CREATE_CONTEXT_DESC_TYPE_UPSCALE FFX_API_MAKE_EFFECT_SUB_ID(FFX_API_EFFECT_ID_UPSCALE, 0x00)
struct ffxCreateContextDescUpscale
{
    ffxCreateContextDescHeader header;          ///< Header descriptor, use type FFX_API_CREATE_CONTEXT_DESC_TYPE_UPSCALE.
    uint32_t                   flags;           ///< Zero or a combination of values from FfxApiCreateContextUpscaleFlags.
    struct FfxApiDimensions2D  maxRenderSize;   ///< The maximum size that rendering will be performed at.
    struct FfxApiDimensions2D  maxUpscaleSize;  ///< The size of the presentation resolution targeted by the upscaling process.
    ffxApiMessage              fpMessage;       ///< A pointer to a function that can receive messages from the runtime. May be null.
};

#define FFX_API_DISPATCH_DESC_TYPE_UPSCALE FFX_API_MAKE_EFFECT_SUB_ID(FFX_API_EFFECT_ID_UPSCALE, 0x01)
struct ffxDispatchDescUpscale
{
    ffxDispatchDescHeader      header;                      ///< Header descriptor, use type FFX_API_DISPATCH_DESC_TYPE_UPSCALE.
    void*                      commandList;                 ///< Command list to record upscaling rendering commands into.
    struct FfxApiResource      color;                       ///< Color buffer for the current frame (at render resolution).
    struct FfxApiResource      depth;                       ///< 32bit depth values for the current frame (at render resolution).
    struct FfxApiResource      motionVectors;               ///< 2-dimensional motion vectors (at render resolution if <c><i>FFX_FSR_ENABLE_DISPLAY_RESOLUTION_MOTION_VECTORS</i></c> is not set).
    struct FfxApiResource      exposure;                    ///< Optional resource containing a 1x1 exposure value.
    struct FfxApiResource      reactive;                    ///< Optional resource containing alpha value of reactive objects in the scene.
    struct FfxApiResource      transparencyAndComposition;  ///< Optional resource containing alpha value of special objects in the scene.
    struct FfxApiResource      output;                      ///< Output color buffer for the current frame (at presentation resolution).
    struct FfxApiFloatCoords2D jitterOffset;                ///< The subpixel jitter offset applied to the camera.
    struct FfxApiFloatCoords2D motionVectorScale;           ///< The scale factor to apply to motion vectors.
    struct FfxApiDimensions2D  renderSize;                  ///< The resolution that was used for rendering the input resources.
    struct FfxApiDimensions2D  upscaleSize;                 ///< The resolution that the upscaler will upscale to (optional, assumed maxUpscaleSize otherwise).
    bool                       enableSharpening;            ///< Enable an additional sharpening pass.
    float                      sharpness;                   ///< The sharpness value between 0 and 1, where 0 is no additional sharpness and 1 is maximum additional sharpness.
    float                      frameTimeDelta;              ///< The time elapsed since the last frame (expressed in milliseconds).
    float                      preExposure;                 ///< The pre exposure value (must be > 0.0f)
    bool                       reset;                       ///< A boolean value which when set to true, indicates the camera has moved discontinuously.
    float                      cameraNear;                  ///< The distance to the near plane of the camera.
    float                      cameraFar;                   ///< The distance to the far plane of the camera.
    float                      cameraFovAngleVertical;      ///< The camera angle field of view in the vertical direction (expressed in radians).
    float                      viewSpaceToMetersFactor;     ///< The scale factor to convert view space units to meters
    uint32_t                   flags;                       ///< Zero or a combination of values from FfxApiDispatchFsrUpscaleFlags.
};

#define FFX_API_QUERY_DESC_TYPE_UPSCALE_GETUPSCALERATIOFROMQUALITYMODE FFX_API_MAKE_EFFECT_SUB_ID(FFX_API_EFFECT_ID_UPSCALE, 0x02)
struct ffxQueryDescUpscaleGetUpscaleRatioFromQualityMode
{
    ffxQueryDescHeader header;            ///< Header descriptor, use type FFX_API_QUERY_DESC_TYPE_UPSCALE_GETUPSCALERATIOFROMQUALITYMODE.
    uint32_t           qualityMode;       ///< The desired quality mode for FSR upscaling.
    float*             pOutUpscaleRatio;  ///< A pointer to a <c>float</c> which will hold the upscaling the per-dimension upscaling ratio.
};

#define FFX_API_QUERY_DESC_TYPE_UPSCALE_GETRENDERRESOLUTIONFROMQUALITYMODE FFX_API_MAKE_EFFECT_SUB_ID(FFX_API_EFFECT_ID_UPSCALE, 0x03)
struct ffxQueryDescUpscaleGetRenderResolutionFromQualityMode
{
    ffxQueryDescHeader header;            ///< Header descriptor, use type FFX_API_QUERY_DESC_TYPE_UPSCALE_GETRENDERRESOLUTIONFROMQUALITYMODE.
    uint32_t           displayWidth;      ///< The target display resolution width.
    uint32_t           displayHeight;     ///< The target display resolution height.
    uint32_t           qualityMode;       ///< The desired quality mode for FSR upscaling.
    uint32_t*          pOutRenderWidth;   ///< A pointer to a <c>uint32_t</c> which will hold the calculated render resolution width.
    uint32_t*          pOutRenderHeight;  ///< A pointer to a <c>uint32_t</c> which will hold the calculated render resolution height.
};

#define FFX_API_QUERY_DESC_TYPE_UPSCALE_GETJITTERPHASECOUNT FFX_API_MAKE_EFFECT_SUB_ID(FFX_API_EFFECT_ID_UPSCALE, 0x04)
struct ffxQueryDescUpscaleGetJitterPhaseCount
{
    ffxQueryDescHeader header;          ///< Header descriptor, use type FFX_API_QUERY_DESC_TYPE_UPSCALE_GETJITTERPHASECOUNT.
    uint32_t           renderWidth;     ///< The render resolution width.
    uint32_t           displayWidth;    ///< The output resolution width.
    int32_t*           pOutPhaseCount;  ///< A pointer to a <c>int32_t</c> which will hold the jitter phase count for the scaling factor between <c><i>renderWidth</i></c> and <c><i>displayWidth</i></c>.
};

#define FFX_API_QUERY_DESC_TYPE_UPSCALE_GETJITTEROFFSET FFX_API_MAKE_EFFECT_SUB_ID(FFX_API_EFFECT_ID_UPSCALE, 0x05)
struct ffxQueryDescUpscaleGetJitterOffset
{
    ffxQueryDescHeader header;      ///< Header descriptor, use type FFX_API_QUERY_DESC_TYPE_UPSCALE_GETJITTEROFFSET.
    int32_t            index;       ///< The index within the jitter sequence.
    int32_t            phaseCount;  ///< The length of jitter phase. See <c><i>ffxQueryDescFsrGetJitterPhaseCount</i></c>.
    float*             pOutX;       ///< A pointer to a <c>float</c> which will contain the subpixel jitter offset for the x dimension.
    float*             pOutY;       ///< A pointer to a <c>float</c> which will contain the subpixel jitter offset for the y dimension.
};

#define FFX_API_DISPATCH_DESC_TYPE_UPSCALE_GENERATEREACTIVEMASK FFX_API_MAKE_EFFECT_SUB_ID(FFX_API_EFFECT_ID_UPSCALE, 0x06)
struct ffxDispatchDescUpscaleGenerateReactiveMask
{
    ffxDispatchDescHeader     header;           ///< Header descriptor, use type FFX_API_DISPATCH_DESC_TYPE_UPSCALE_GENERATEREACTIVEMASK.
    void*                     commandList;      ///< The <c><i>FfxCommandList</i></c> to record FSRUPSCALE rendering commands into.
    struct FfxApiResource     colorOpaqueOnly;  ///< A <c><i>FfxApiResource</i></c> containing the opaque only color buffer for the current frame (at render resolution).
    struct FfxApiResource     colorPreUpscale;  ///< A <c><i>FfxApiResource</i></c> containing the opaque+translucent color buffer for the current frame (at render resolution).
    struct FfxApiResource     outReactive;      ///< A <c><i>FfxApiResource</i></c> containing the surface to generate the reactive mask into.
    struct FfxApiDimensions2D renderSize;       ///< The resolution that was used for rendering the input resources.
    float                     scale;            ///< A value to scale the output
    float                     cutoffThreshold;  ///< A threshold value to generate a binary reactive mask
    float                     binaryValue;      ///< A value to set for the binary reactive mask
    uint32_t                  flags;            ///< Flags to determine how to generate the reactive mask
};

#define FFX_API_CONFIGURE_DESC_TYPE_UPSCALE_KEYVALUE FFX_API_MAKE_EFFECT_SUB_ID(FFX_API_EFFECT_ID_UPSCALE, 0x07)
struct ffxConfigureDescUpscaleKeyValue
{
    ffxConfigureDescHeader header;  ///< Header descriptor, use type FFX_API_CONFIGURE_DESC_TYPE_UPSCALE_KEYVALUE.
    uint64_t               key;     ///< Configuration key, member of the FfxApiConfigureUpscaleKey enumeration.
    uint64_t               u64;     ///< Integer value or enum value to set.
    void*                  ptr;     ///< Pointer to set or pointer to value to set.
};

enum FfxApiConfigureUpscaleKey
{
    FFX_API_CONFIGURE_UPSCALE_KEY_FVELOCITYFACTOR              = 0,  ///< Override constant buffer fVelocityFactor. The float value is casted from void * ptr. Value of 0.0f can improve temporal stability of bright pixels. Default value is 1.0f. Value is clamped to [0.0f, 1.0f].
    FFX_API_CONFIGURE_UPSCALE_KEY_FREACTIVENESSSCALE           = 1,  ///< Override constant buffer fReactivenessScale. The float value is casted from void * ptr. Meant for development purpose to test if writing a larger value to reactive mask, reduces ghosting. Default value is 1.0f. Value is clamped to [0.0f, +infinity].
    FFX_API_CONFIGURE_UPSCALE_KEY_FSHADINGCHANGESCALE          = 2,  ///< Override fShadingChangeScale. Increasing this scales fsr3.1 computed shading change value at read to have higher reactiveness. Default value is 1.0f. Value is clamped to [0.0f, +infinity].
    FFX_API_CONFIGURE_UPSCALE_KEY_FACCUMULATIONADDEDPERFRAME   = 3,  ///< Override constant buffer fAccumulationAddedPerFrame. Corresponds to amount of accumulation added per frame at pixel coordinate where disocclusion occured or when reactive mask value is > 0.0f. Decreasing this and drawing the ghosting object (IE no mv) to reactive mask with value close to 1.0f can decrease temporal ghosting. Decreasing this value could result in more thin feature pixels flickering. Default value is 0.333. Value is clamped to [0.0f, 1.0f].
    FFX_API_CONFIGURE_UPSCALE_KEY_FMINDISOCCLUSIONACCUMULATION = 4,  ///< Override constant buffer fMinDisocclusionAccumulation. Increasing this value may reduce white pixel temporal flickering around swaying thin objects that are disoccluding one another often. Too high value may increase ghosting. A sufficiently negative value means for pixel coordinate at frame N that is disoccluded, add fAccumulationAddedPerFrame starting at frame N+2. Default value is -0.333. Value is clamped to [-1.0f, 1.0f].
};

#define FFX_API_QUERY_DESC_TYPE_UPSCALE_GPU_MEMORY_USAGE FFX_API_MAKE_EFFECT_SUB_ID(FFX_API_EFFECT_ID_UPSCALE, 0x08)
struct ffxQueryDescUpscaleGetGPUMemoryUsage
{
    ffxQueryDescHeader              header;                 ///< Header descriptor, use type FFX_API_QUERY_DESC_TYPE_UPSCALE_GPU_MEMORY_USAGE.
    struct FfxApiEffectMemoryUsage* gpuMemoryUsageUpscaler; ///< Output values by Query() call.
};

#define FFX_API_QUERY_DESC_TYPE_UPSCALE_GPU_MEMORY_USAGE_V2 FFX_API_MAKE_EFFECT_SUB_ID(FFX_API_EFFECT_ID_UPSCALE, 0x09)
struct ffxQueryDescUpscaleGetGPUMemoryUsageV2
{
    ffxQueryDescHeader              header;                  ///< Header descriptor, use type FFX_API_QUERY_DESC_TYPE_UPSCALE_GPU_MEMORY_USAGE_V2.
    void*                           device;                  ///< For DX12: pointer to ID3D12Device. For VK, pointer to VkDeviceContext. App needs to fill out before Query() call.
    struct FfxApiDimensions2D       maxRenderSize;           ///< App needs to fill out before Query() call.
    struct FfxApiDimensions2D       maxUpscaleSize;          ///< App needs to fill out before Query() call.
    uint32_t                        flags;                   ///< Zero or a combination of values from FfxApiCreateContextUpscaleFlags. App needs to fill out before Query() call.
    struct FfxApiEffectMemoryUsage* gpuMemoryUsageUpscaler;  ///< Output values by Query() call.
};

enum FfxApiQueryResourceIdentifiers
{
    FFX_API_QUERY_RESOURCE_INPUT_COLOR                   = (1 << 0),  ///< Color buffer for the current frame (at render resolution).
    FFX_API_QUERY_RESOURCE_INPUT_DEPTH                   = (1 << 1),  ///< 32bit depth values for the current frame (at render resolution).
    FFX_API_QUERY_RESOURCE_INPUT_MV                      = (1 << 2),  ///< 2-dimensional motion vectors (at render resolution if FFX_FSR_ENABLE_DISPLAY_RESOLUTION_MOTION_VECTORS is not set).
    FFX_API_QUERY_RESOURCE_INPUT_EXPOSURE                = (1 << 3),  ///< A 1x1 texture containing exposure value or the FFX_UPSCALE_ENABLE_AUTO_EXPOSURE set at context creation.
    FFX_API_QUERY_RESOURCE_INPUT_REACTIVEMASK            = (1 << 4),  ///< An R8 UNORM texture at render resolution that controls the influence of pixel history on upscaling result.
    FFX_API_QUERY_RESOURCE_INPUT_TRANSPARENCYCOMPOSITION = (1 << 5),  ///< An R8 UNORM texture at render resolution that controls the blending of color in the upscaling result.
};

#define FFX_API_QUERY_DESC_TYPE_UPSCALE_GET_RESOURCE_REQUIREMENTS FFX_API_MAKE_EFFECT_SUB_ID(FFX_API_EFFECT_ID_UPSCALE, 0x0a)
struct ffxQueryDescUpscaleGetResourceRequirements
{
    ffxQueryDescHeader header;              ///< Header descriptor, use type FFX_API_QUERY_DESC_TYPE_UPSCALE_GET_RESOURCE_REQUIREMENTS.
    uint64_t           required_resources;  ///< resources 64b bitfield, that given current context state, are required for effect correctness.
    uint64_t           optional_resources;  ///< resources 64b bitfield, that given current context state, will be consumed if provided, but are optional.
};

#define FFX_API_CREATE_CONTEXT_DESC_TYPE_UPSCALE_VERSION FFX_API_MAKE_EFFECT_SUB_ID(FFX_API_EFFECT_ID_UPSCALE, 0x0b)
struct ffxCreateContextDescUpscaleVersion
{
    ffxCreateContextDescHeader header;   ///< Header descriptor, use type FFX_API_CREATE_CONTEXT_DESC_TYPE_UPSCALE_VERSION.
    uint32_t                   version;  ///< The version of the API the application was built against. This must be set to FFX_UPSCALER_VERSION.
};

#ifdef __cplusplus
}
#endif
