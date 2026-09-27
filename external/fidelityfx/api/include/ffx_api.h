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

#if defined(__cplusplus)
extern "C" {
#endif  // #if defined(__cplusplus)

#define FFX_API_ENTRY __declspec(dllexport)

#include <stdint.h>

enum FfxApiReturnCodes
{
    FFX_API_RETURN_OK                     = 0, ///< The oparation was successful.
    FFX_API_RETURN_ERROR                  = 1, ///< An error occurred that is not further specified.
    FFX_API_RETURN_ERROR_UNKNOWN_DESCTYPE = 2, ///< The structure type given was not recognized for the function or context with which it was used. This is likely a programming error.
    FFX_API_RETURN_ERROR_RUNTIME_ERROR    = 3, ///< The underlying runtime (e.g. D3D12, Vulkan) or effect returned an error code.
    FFX_API_RETURN_NO_PROVIDER            = 4, ///< No provider was found for the given structure type. This is likely a programming error.
    FFX_API_RETURN_ERROR_MEMORY           = 5, ///< A memory allocation failed.
    FFX_API_RETURN_ERROR_PARAMETER        = 6, ///< A parameter was invalid, e.g. a null pointer, empty resource or out-of-bounds enum value.
    FFX_API_RETURN_PROVIDER_NO_SUPPORT_NEW_DESCTYPE = 7, ///< The structure type given is new and not supported in the old provider. This is likely fixed with driver upgrade or effect DLL upgrade.
};

typedef void* ffxContext;
typedef uint32_t ffxReturnCode_t;

#define FFX_API_EFFECT_MASK         0x00ff0000u
#define FFX_API_BACKEND_MASK        0xff000000u
#define FFX_API_EFFECT_ID_GENERAL   0x00000000u

// Base Descriptor types
typedef uint64_t ffxStructType_t;
typedef struct ffxApiHeader
{
    ffxStructType_t      type;  ///< The structure type. Must always be set to the corresponding value for any structure (found nearby with a similar name).
    struct ffxApiHeader* pNext; ///< Pointer to next structure, used for optional parameters and extensions. Can be null.
} ffxApiHeader;

typedef ffxApiHeader ffxCreateContextDescHeader;
typedef ffxApiHeader ffxConfigureDescHeader;
typedef ffxApiHeader ffxQueryDescHeader;
typedef ffxApiHeader ffxDispatchDescHeader;

// Extensions for global debug
#define FFX_API_CONFIGURE_GLOBALDEBUG_LEVEL_SILENCE  0x0000000u
#define FFX_API_CONFIGURE_GLOBALDEBUG_LEVEL_ERRORS   0x0000001u
#define FFX_API_CONFIGURE_GLOBALDEBUG_LEVEL_WARNINGS 0x0000002u
#define FFX_API_CONFIGURE_GLOBALDEBUG_LEVEL_VERBOSE  0xfffffffu

enum FfxApiMsgType
{
    FFX_API_MESSAGE_TYPE_ERROR   = 0,
    FFX_API_MESSAGE_TYPE_WARNING = 1,
    FFX_API_MESSAGE_TYPE_COUNT
};

typedef void (*ffxApiMessage)(uint32_t type, const wchar_t* message);

#define FFX_API_CONFIGURE_DESC_TYPE_GLOBALDEBUG1 0x0000001u
struct ffxConfigureDescGlobalDebug1
{
    ffxConfigureDescHeader header;
    ffxApiMessage          fpMessage;
    uint32_t               debugLevel;
};

#define FFX_API_CONFIGURE_DESC_TYPE_GLOBALDEBUG 7u
struct ffxConfigureDescGlobalDebug
{
    ffxConfigureDescHeader header;
    uint64_t               effectId;
    ffxApiMessage          fpMessage;  ///< A pointer to a function that can receive messages from the runtime. May be null.
    uint32_t               debugLevel;
};

#define FFX_API_QUERY_DESC_TYPE_GET_VERSIONS 4u
struct ffxQueryDescGetVersions
{
    ffxQueryDescHeader header;
    uint64_t createDescType;    ///< Create description for the effect whose versions should be enumerated.
    void* device;               ///< For DX12: pointer to ID3D12Device.
    uint64_t *outputCount;      ///< Input capacity of id and name arrays. Output number of returned versions. If initially zero, output is number of available versions.
    uint64_t *versionIds;       ///< Output array of version ids to be used as version overrides. If null, only names and count are returned.
    const char** versionNames;  ///< Output array of version names for display. If null, only ids and count are returned. If both this and versionIds are null, only count is returned.
};

#define FFX_API_DESC_TYPE_OVERRIDE_VERSION 5u
struct ffxOverrideVersion
{
    ffxApiHeader header;
    uint64_t versionId;  ///< Id of version to use. Must be a value returned from a query in ffxQueryDescGetVersions.versionIds array.
};

#define FFX_API_QUERY_DESC_TYPE_GET_PROVIDER_VERSION 6u
struct ffxQueryGetProviderVersion
{
    ffxQueryDescHeader header;
    uint64_t versionId;      ///< Id of provider being used for queried context. 0 if invalid.
    const char* versionName; ///< Version name for display. If nullptr, the query was invalid.
};

// Memory allocation function. Must return a valid pointer to at least size bytes of memory aligned to hold any type.
// May return null to indicate failure. Standard library malloc fulfills this requirement.
typedef void* (*ffxAlloc)(void* pUserData, uint64_t size);

// Memory deallocation function. May be called with null pointer as second argument.
typedef void (*ffxDealloc)(void* pUserData, void* pMem);

typedef struct ffxAllocationCallbacks
{
    void* pUserData;
    ffxAlloc alloc;
    ffxDealloc dealloc;
} ffxAllocationCallbacks;

// Creates a FFX object context.
// Depending on the desc structures provided to this function, the context will be created with the desired version and attributes.
// Non-zero return indicates error code.
// Pointers passed in desc must remain live until ffxDestroyContext is called on the context.
// MemCb may be null; the system allocator (malloc/free) will be used in this case.
FFX_API_ENTRY ffxReturnCode_t ffxCreateContext(ffxContext* context, ffxCreateContextDescHeader* desc, const ffxAllocationCallbacks* memCb);
typedef ffxReturnCode_t (*PfnFfxCreateContext)(ffxContext* context, ffxCreateContextDescHeader* desc, const ffxAllocationCallbacks* memCb);

// Destroys an FFX object context.
// Non-zero return indicates error code.
// MemCb must be compatible with the callbacks passed into ffxCreateContext.
FFX_API_ENTRY ffxReturnCode_t ffxDestroyContext(ffxContext* context, const ffxAllocationCallbacks* memCb);
typedef ffxReturnCode_t (*PfnFfxDestroyContext)(ffxContext* context, const ffxAllocationCallbacks* memCb);

// Configures the provided FFX object context.
// If context is null, configure operates on any global state.
// Non-zero return indicates error code.
FFX_API_ENTRY ffxReturnCode_t ffxConfigure(ffxContext* context, const ffxConfigureDescHeader* desc);
typedef ffxReturnCode_t (*PfnFfxConfigure)(ffxContext* context, const ffxConfigureDescHeader* desc);

// Queries the provided FFX object context.
// If context is null, query operates on any global state.
// Non-zero return indicates error code.
FFX_API_ENTRY ffxReturnCode_t ffxQuery(ffxContext* context, ffxQueryDescHeader* desc);
typedef ffxReturnCode_t (*PfnFfxQuery)(ffxContext* context, ffxQueryDescHeader* desc);

// Dispatches work on the given FFX object context defined by the dispatch descriptor.
// Non-zero return indicates error code.
FFX_API_ENTRY ffxReturnCode_t ffxDispatch(ffxContext* context, const ffxDispatchDescHeader* desc);
typedef ffxReturnCode_t (*PfnFfxDispatch)(ffxContext* context, const ffxDispatchDescHeader* desc);

// FFX_API_EFFECT_IDs
#define FFX_API_EFFECT_ID_UPSCALE                       	0x00010000u
#define FFX_API_EFFECT_ID_FRAMEGENERATION               	0x00020000u
#define FFX_API_EFFECT_ID_FRAMEGENERATIONSWAPCHAIN      	0x00030000u
// Need to keep this ID around for the deprecated VK frame gen swapchain
#define FFX_API_EFFECT_ID_FRAMEGENERATIONSWAPCHAIN_VK       0x00040000u
// Need to keep this ID around for the deprecated VK frame gen swapchain
#define FFX_API_EFFECT_ID_DENOISER                      	0x00050000u
#define FFX_API_EFFECT_ID_RADIANCECACHE                   	0x00060000u

#define FFX_API_MAKE_EFFECT_SUB_ID(effectId, subversion) ((effectId & FFX_API_EFFECT_MASK) | (subversion & ~FFX_API_EFFECT_MASK))

// FFX_APID_BACKEND_IDs
#define FFX_API_BACKEND_ID_DX12               0x00000000u
#define FFX_API_BACKEND_ID_XBOX               0x01000000u
#define FFX_API_BACKEND_ID_VK                 0x02000000u // For new effects going forward, please use this backend ID for vulkan specifics

#define FFX_API_MAKE_BACKEND_SUB_ID(backendId, subversion) ((backendId & FFX_API_BACKEND_MASK) | (subversion & ~FFX_API_BACKEND_MASK))

// Combiner for BACKEND-specific EFFECT sub-Ids
#define FFX_API_MAKE_BACKEND_EFFECT_SUB_ID(backendId, effectId, subversion) ((subversion & ~FFX_API_EFFECT_MASK) | (effectId & FFX_API_EFFECT_MASK) | (backendId & FFX_API_BACKEND_MASK) | (subversion & ~(FFX_API_BACKEND_MASK | FFX_API_EFFECT_MASK)))

// Pragma macros for controlling warnings so that deprecations take affect externally but can be suppressed internally.
// This is so we can maintain the API/ABI until we are ready to make the breaking change.
// These are sadly compiler specific.
#if defined(_MSC_VER) && !defined(__clang__) && !defined(__GNUC__) && !defined(__INTEL_COMPILER)
#define FFX_PRAGMA_WARNING_PUSH warning( push )
#define FFX_PRAGMA_WARNING_POP warning( pop )
#define FFX_PRAGMA_WARNING_DISABLE_DEPRECATIONS warning( disable: 4996 )
#define FFX_PRAGMA_WARNING_WARN_DEPRECATIONS warning( default: 4996 )
#elif defined(__clang__) || defined(__GNUC__)
#define FFX_PRAGMA_WARNING_PUSH GCC diagnostic push
#define FFX_PRAGMA_WARNING_POP GCC diagnostic pop
#define FFX_PRAGMA_WARNING_DISABLE_DEPRECATIONS GCC diagnostic ignored "-Wdeprecated"
#define FFX_PRAGMA_WARNING_WARN_DEPRECATIONS GCC diagnostic warning "-Wdeprecated"
#else
#define FFX_PRAGMA_WARNING_PUSH 
#define FFX_PRAGMA_WARNING_POP 
#define FFX_PRAGMA_WARNING_DISABLE_DEPRECATIONS 
#endif

#define FFX_DEPRECATION(message) [[deprecated(message)]]

#if defined(__cplusplus)
}
#endif  // #if defined(__cplusplus)
