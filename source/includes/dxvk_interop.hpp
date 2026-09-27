#pragma once

// Public DXVK D3D9 interfaces, declared as in src/d3d9/d3d9_interfaces.h of DXVK (zlib license).
// Only the COM ABI matters: the IIDs and the order of the methods.

#include <d3d9.h>

#ifndef VK_NO_PROTOTYPES
#define VK_NO_PROTOTYPES
#endif
#ifndef VK_USE_PLATFORM_WIN32_KHR
#define VK_USE_PLATFORM_WIN32_KHR
#endif
#include <vulkan/vulkan.h>

MIDL_INTERFACE("3461a81b-ce41-485b-b6b5-fcf08ba6a6bd")
ID3D9VkInteropInterface : public IUnknown
{
    virtual void STDMETHODCALLTYPE GetInstanceHandle(VkInstance* pInstance) = 0;
    virtual void STDMETHODCALLTYPE GetPhysicalDeviceHandle(UINT Adapter, VkPhysicalDevice* pPhysicalDevice) = 0;
};

MIDL_INTERFACE("d56344f5-8d35-46fd-806d-94c351b472c1")
ID3D9VkInteropTexture : public IUnknown
{
    virtual HRESULT STDMETHODCALLTYPE GetVulkanImageInfo(VkImage* pHandle, VkImageLayout* pLayout, VkImageCreateInfo* pInfo) = 0;
};

struct D3D9VkExtImageDesc
{
    D3DRESOURCETYPE Type;
    UINT Width;
    UINT Height;
    UINT Depth;
    UINT MipLevels;
    DWORD Usage;
    D3DFORMAT Format;
    D3DPOOL Pool;
    D3DMULTISAMPLE_TYPE MultiSample;
    DWORD MultiSampleQuality;
    bool Discard;
    bool IsAttachmentOnly;
    bool IsLockable;
    VkImageUsageFlags ImageUsage;
};

MIDL_INTERFACE("2eaa4b89-0107-4bdb-87f7-0f541c493ce0")
ID3D9VkInteropDevice : public IUnknown
{
    virtual void STDMETHODCALLTYPE GetVulkanHandles(VkInstance* pInstance, VkPhysicalDevice* pPhysDev, VkDevice* pDevice) = 0;
    virtual void STDMETHODCALLTYPE GetSubmissionQueue(VkQueue* pQueue, uint32_t* pQueueIndex, uint32_t* pQueueFamilyIndex) = 0;
    virtual void STDMETHODCALLTYPE TransitionTextureLayout(ID3D9VkInteropTexture* pTexture, const VkImageSubresourceRange* pSubresources, VkImageLayout OldLayout, VkImageLayout NewLayout) = 0;
    // Submits all pending rendering and waits for DXVK's command stream thread
    virtual void STDMETHODCALLTYPE FlushRenderingCommands() = 0;
    // Waits for pending submissions and locks the queue returned by GetSubmissionQueue
    virtual void STDMETHODCALLTYPE LockSubmissionQueue() = 0;
    virtual void STDMETHODCALLTYPE ReleaseSubmissionQueue() = 0;
    virtual void STDMETHODCALLTYPE LockDevice() = 0;
    virtual void STDMETHODCALLTYPE UnlockDevice() = 0;
    virtual bool STDMETHODCALLTYPE WaitForResource(IDirect3DResource9* pResource, DWORD MapFlags) = 0;
    virtual HRESULT STDMETHODCALLTYPE CreateImage(const D3D9VkExtImageDesc* desc, IDirect3DResource9** ppResult) = 0;
};

struct D3D9VkExtOutputMetadata
{
    float RedPrimary[2];
    float GreenPrimary[2];
    float BluePrimary[2];
    float WhitePoint[2];
    float MinLuminance;
    float MaxLuminance;
    float MaxFullFrameLuminance;
};

MIDL_INTERFACE("13776e93-4aa9-430a-a4ec-fe9e281181d5")
ID3D9VkExtSwapchain : public IUnknown
{
    virtual BOOL STDMETHODCALLTYPE CheckColorSpaceSupport(VkColorSpaceKHR ColorSpace) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetColorSpace(VkColorSpaceKHR ColorSpace) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetHDRMetaData(const VkHdrMetadataEXT* pHDRMetadata) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetCurrentOutputDesc(D3D9VkExtOutputMetadata* pOutputDesc) = 0;
    virtual void STDMETHODCALLTYPE UnlockAdditionalFormats() = 0;
};

MIDL_INTERFACE("65b55086-e3e3-4c3e-b3a0-86815cce2c4c")
ID3D9VkExtInterface : public IUnknown
{
    virtual void STDMETHODCALLTYPE UnlockAdditionalFormats() = 0;
};
