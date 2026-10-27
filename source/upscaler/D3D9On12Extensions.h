// The optional interface of the Fusion Fix D3D9On12 driver (ThirteenAG/D3D9On12): a copy of the driver's
// include/D3D9On12Extensions.h, with the HRESULT guard below. Its license: D3D9On12-LICENSE.txt.
// Licensed under the MIT license.
#pragma once
#include <stdint.h>

// Usable without windows.h (same guard as winnt.h)
#ifndef _HRESULT_DEFINED
#define _HRESULT_DEFINED
typedef long HRESULT;
#endif

// Optional D3D9On12 extension interfaces. Windows/upstream drivers do not export these
// functions. Query the loaded driver module with GetProcAddress after device creation, outside
// DllMain, and keep the ordinary paths when an export or capability bit is absent.
#define D3D9ON12_CAPABILITIES_ABI 3u
#define D3D9ON12_CAP_SAMPLER_ADDRESS_VALIDATION (1u << 0)   // ABI 1 and later
#define D3D9ON12_CAP_QUEUED_INTEROP             (1u << 1)   // ABI 2 and later: D3D9On12QueueInterop
// ABI 2: the driver presents from an exclusive fullscreen device (primaries through the D3D12 runtime's private
// present interfaces). Reported only when the installed D3D12 runtime offers those interfaces;
// clients can create a windowed device when exclusive presentation is unavailable.
#define D3D9ON12_CAP_EXCLUSIVE_FULLSCREEN       (1u << 2)
// ABI 2: event queries complete when polled (the batch holding their End is submitted), as D3DGETDATA_FLUSH asks;
// drivers without it can leave an application that waits on one (ReShade on a reset) polling for ever.
#define D3D9ON12_CAP_EVENT_QUERY_FLUSH          (1u << 3)
// ABI 3: HDR output (D3D9On12SetHdrOutput, D3D9On12QueryHdrOutput below).
#define D3D9ON12_CAP_HDR_OUTPUT                 (1u << 4)
#define D3D9ON12_CAPABILITIES_EXPORT "D3D9On12Capabilities"
#define D3D9ON12_QUEUE_INTEROP_EXPORT "D3D9On12QueueInterop"
#define D3D9ON12_SET_HDR_OUTPUT_EXPORT "D3D9On12SetHdrOutput"
#define D3D9ON12_QUERY_HDR_OUTPUT_EXPORT "D3D9On12QueryHdrOutput"

// ABI 1 returns the ABI-1 bits, ABI 2 the ABI-1 and ABI-2 bits, ABI 3 all bits above; other versions return 0.
// A client built for ABI 3 that gets 0 can ask again for ABI 2 (an older driver).
typedef uint32_t (__stdcall* PFN_D3D9On12Capabilities)(uint32_t abiVersion);

// Queued interop: copies between D3D9 resources of this device and foreign D3D12 resources (for
// example textures shared by another process), with a fence signal or wait, recorded in order with
// the D3D9 command stream and executed by the driver without draining it on the calling thread.
//
// Usage: fill the request, call D3D9On12QueueInterop to receive a token, then issue
//   IDirect3DDevice9::SetRenderState((D3DRENDERSTATETYPE)D3D9ON12_INTEROP_MARKER_RENDERSTATE, token)
// on the same device. The runtime forwards that render state to the driver in command-stream order;
// the driver consumes the token there and performs the operation at that point of the stream. Each
// token is consumed once; an unconsumed token is dropped when the device is destroyed. The marker
// state itself is not applied as a render state.
//
// Game resources are identified by the ID3D12Resource obtained from
// IDirect3DDevice9On12::UnwrapUnderlyingResource (followed by ReturnUnderlyingResource); that call
// registers the resource with the driver. Foreign resources must be in D3D12_RESOURCE_STATE_COMMON
// when the operation executes and are left in COMMON. Only subresource 0 is copied, whole.
//
// EXPORT_COPY: after all D3D9 work recorded so far, copy GameResources[i] -> ForeignResources[i],
//   submit, then Signal(Fence, FenceValue) on the driver's command queue.
// IMPORT_COPY: submit the work recorded so far, Wait(Fence, FenceValue) on the driver's command
//   queue, then copy ForeignResources[i] -> GameResources[i]; later D3D9 use of the game resources
//   is ordered after the copy.
#define D3D9ON12_INTEROP_MARKER_RENDERSTATE 183u   // D3DRS_ADAPTIVETESS_W
#define D3D9ON12_INTEROP_EXPORT_COPY 1u
#define D3D9ON12_INTEROP_IMPORT_COPY 2u

struct ID3D12Device;
struct ID3D12Resource;
struct ID3D12Fence;

typedef struct D3D9ON12_INTEROP_REQUEST
{
    uint32_t StructSize;                      // sizeof(D3D9ON12_INTEROP_REQUEST)
    uint32_t Operation;                       // D3D9ON12_INTEROP_*_COPY
    ID3D12Device* Device;                     // from IDirect3DDevice9On12::GetD3D12Device, identifies the D3D9On12 device
    uint32_t Count;                           // number of resource pairs (1..8)
    uint32_t Reserved;
    ID3D12Resource* const* GameResources;     // Count entries, from UnwrapUnderlyingResource; null entries are skipped
    ID3D12Resource* const* ForeignResources;  // Count entries
    ID3D12Fence* Fence;
    uint64_t FenceValue;
} D3D9ON12_INTEROP_REQUEST;

// Returns S_OK and a token, E_INVALIDARG for a malformed request, E_NOINTERFACE when Device is not
// a D3D9On12 device of this driver, E_FAIL when a game resource was never unwrapped.
typedef HRESULT (__stdcall* PFN_D3D9On12QueueInterop)(const D3D9ON12_INTEROP_REQUEST* request, uint32_t* token);

// HDR output. The D3D9 runtime accepts no 16-bit float back buffer and has no color space, so the application keeps
// its 8-bit back buffer format (A8R8G8B8 or X8R8G8B8) and the driver changes the storage:
// - While HDR output is enabled for a device, the swap chain buffers the runtime creates for it afterwards (at device
//   creation and at Reset; not lockable, not multisampled) are stored as 16-bit float RGBA. The application and the
//   runtime keep seeing the 8-bit format; everything rendered into the buffers keeps values above 1.0 and below 0.0,
//   and copies (StretchRect) to and from 16-bit float render targets keep them as well. Enabling or disabling HDR
//   output takes effect on those buffers with the next Reset (or a new device).
// - Presenting float buffers in a window (windowed or borderless): the window's DXGI flip swap chain is
//   R16G16B16A16_FLOAT in DXGI_COLOR_SPACE_RGB_FULL_G10_NONE_P709 (scRGB: linear, BT.709 primaries, 1.0 = 80 nits),
//   with the HDR10 metadata given here. Presenting 8-bit buffers keeps DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709.
// - Exclusive fullscreen: the runtime presents its primaries with the kernel's legacy present, which carries no color
//   space (a float primary flips, but is scanned out as SDR, clipped at 1.0). The driver keeps them 8-bit and reports
//   D3D9ON12_HDR_STATUS_EXCLUSIVE; clients wanting HDR there create the device windowed over the monitor instead.
// Enabling does not check the display: D3D9On12QueryHdrOutput tells whether the window's display is in an HDR mode
// (Windows HDR on), the condition under which an application should offer HDR output.
#define D3D9ON12_HDR_OUTPUT_ENABLE              (1u << 0)
#define D3D9ON12_HDR_COLOR_SPACE_SCRGB_LINEAR   1u      // the only one so far

typedef struct D3D9ON12_HDR_OUTPUT
{
    uint32_t StructSize;                      // sizeof(D3D9ON12_HDR_OUTPUT)
    uint32_t Flags;                           // D3D9ON12_HDR_OUTPUT_*
    uint32_t ColorSpace;                      // D3D9ON12_HDR_COLOR_SPACE_*
    uint32_t Reserved;
    float MaxLuminance;                       // HDR10 metadata in nits: the display's peak the content is graded for
    float MinLuminance;
    float MaxContentLightLevel;               // MaxCLL
    float MaxFrameAverageLightLevel;          // MaxFALL
} D3D9ON12_HDR_OUTPUT;

// Device: from IDirect3DDevice9On12::GetD3D12Device, or null for every device of the process and the devices created
// later (call it before IDirect3D9::CreateDevice to create the first swap chain buffers as float). Returns S_OK,
// E_INVALIDARG for a malformed description, E_NOINTERFACE when Device is not a D3D9On12 device of this driver.
typedef HRESULT (__stdcall* PFN_D3D9On12SetHdrOutput)(ID3D12Device* device, const D3D9ON12_HDR_OUTPUT* output);

#define D3D9ON12_HDR_STATUS_DISPLAY_HDR         (1u << 0)   // the window's display is in an HDR mode (Windows HDR on)
#define D3D9ON12_HDR_STATUS_ENABLED             (1u << 1)   // HDR output is enabled for the device
#define D3D9ON12_HDR_STATUS_FLOAT_BUFFERS       (1u << 2)   // the device's swap chain buffers are stored as float
#define D3D9ON12_HDR_STATUS_ACTIVE              (1u << 3)   // the last present went to a window swap chain in scRGB
#define D3D9ON12_HDR_STATUS_EXCLUSIVE           (1u << 4)   // the device's swap chain is exclusive fullscreen (no HDR)

typedef struct D3D9ON12_HDR_OUTPUT_STATUS
{
    uint32_t StructSize;                      // sizeof(D3D9ON12_HDR_OUTPUT_STATUS)
    uint32_t Flags;                           // D3D9ON12_HDR_STATUS_*
    uint32_t ColorSpace;                      // DXGI_COLOR_SPACE_TYPE of the last windowed present
    uint32_t Reserved;
    float DisplayMaxLuminance;                // of the window's display in nits (DXGI_OUTPUT_DESC1), 0 when unknown
    float DisplayMinLuminance;
    float DisplayMaxFullFrameLuminance;
    float Reserved2;
} D3D9ON12_HDR_OUTPUT_STATUS;

// Device: as above, or null for the display only. Window (an HWND): the display is the one the window is on mostly;
// with a null window nothing about the display is filled in (the device state is cheap to query every frame, the
// display less so). Returns S_OK, E_INVALIDARG, or E_NOINTERFACE (the device flags are then 0).
typedef HRESULT (__stdcall* PFN_D3D9On12QueryHdrOutput)(ID3D12Device* device, void* window,
    D3D9ON12_HDR_OUTPUT_STATUS* status);
