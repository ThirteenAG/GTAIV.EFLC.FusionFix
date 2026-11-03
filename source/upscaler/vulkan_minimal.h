#pragma once

// The part of the Vulkan API the DXVK interop of AntiAliasing.hpp uses: importing the bridge's shared D3D12 textures
// and fence into DXVK's device and copying to and from them on DXVK's queue. Declared as in the Khronos headers
// (vulkan_core.h, vulkan_win32.h; Apache-2.0 OR MIT), so that the fixes don't need the whole Vulkan SDK. Only the
// ABI matters: the values, the layout of the structures and the calling convention.

#include <windows.h>
#include <cstdint>

#define VKAPI_PTR __stdcall

using VkFlags = uint32_t;
// The flag types by name (all VkFlags), for code written against the Khronos headers
using VkAccessFlags = VkFlags;
using VkPipelineStageFlags = VkFlags;
using VkImageUsageFlags = VkFlags;
using VkImageAspectFlags = VkFlags;
using VkImageCreateFlags = VkFlags;
using VkMemoryPropertyFlags = VkFlags;
using VkCommandBufferUsageFlags = VkFlags;
using VkCommandPoolCreateFlags = VkFlags;
using VkExternalMemoryHandleTypeFlags = VkFlags;
using VkExternalSemaphoreHandleTypeFlags = VkFlags;
using VkBool32 = uint32_t;
using VkDeviceSize = uint64_t;

// Dispatchable handles are pointers, the others 64-bit values on every platform
using VkInstance = struct VkInstance_T*;
using VkPhysicalDevice = struct VkPhysicalDevice_T*;
using VkDevice = struct VkDevice_T*;
using VkQueue = struct VkQueue_T*;
using VkCommandBuffer = struct VkCommandBuffer_T*;
using VkImage = uint64_t;
using VkDeviceMemory = uint64_t;
using VkSemaphore = uint64_t;
using VkFence = uint64_t;
using VkCommandPool = uint64_t;
using VkBuffer = uint64_t;

#define VK_NULL_HANDLE 0
#define VK_QUEUE_FAMILY_IGNORED (~0U)
#define VK_QUEUE_FAMILY_EXTERNAL (~1U)
#define VK_TRUE 1u

enum VkResult : int32_t
{
    VK_SUCCESS = 0,
};

enum VkStructureType : int32_t
{
    VK_STRUCTURE_TYPE_SUBMIT_INFO = 4,
    VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO = 5,
    VK_STRUCTURE_TYPE_FENCE_CREATE_INFO = 8,
    VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO = 9,
    VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO = 14,
    VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO = 39,
    VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO = 40,
    VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO = 42,
    VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER = 45,
    VK_STRUCTURE_TYPE_HDR_METADATA_EXT = 1000105000,
    VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2 = 1000059001,
    VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ID_PROPERTIES = 1000071004,
    VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO = 1000072001,
    VK_STRUCTURE_TYPE_IMPORT_MEMORY_WIN32_HANDLE_INFO_KHR = 1000073000,
    VK_STRUCTURE_TYPE_MEMORY_WIN32_HANDLE_PROPERTIES_KHR = 1000073002,
    VK_STRUCTURE_TYPE_IMPORT_SEMAPHORE_WIN32_HANDLE_INFO_KHR = 1000078000,
    VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO = 1000127001,
    VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO = 1000207002,
    VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO = 1000207003,
};

enum VkFormat : int32_t
{
    VK_FORMAT_UNDEFINED = 0,
    VK_FORMAT_R16_SFLOAT = 76,
    VK_FORMAT_R16G16_SFLOAT = 83,
    VK_FORMAT_R16G16B16A16_SFLOAT = 97,
    VK_FORMAT_R32_SFLOAT = 100,
};

enum VkImageLayout : int32_t
{
    VK_IMAGE_LAYOUT_UNDEFINED = 0,
    VK_IMAGE_LAYOUT_GENERAL = 1,
    VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL = 6,
    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL = 7,
};

enum VkImageType : int32_t { VK_IMAGE_TYPE_2D = 1 };
enum VkColorSpaceKHR : int32_t
{
    VK_COLOR_SPACE_SRGB_NONLINEAR_KHR = 0,
    VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT = 1000104002,
};
enum VkImageTiling : int32_t { VK_IMAGE_TILING_OPTIMAL = 0 };
enum VkSharingMode : int32_t { VK_SHARING_MODE_EXCLUSIVE = 0 };
enum VkSampleCountFlagBits : int32_t { VK_SAMPLE_COUNT_1_BIT = 1 };
enum VkSemaphoreType : int32_t { VK_SEMAPHORE_TYPE_TIMELINE = 1 };
enum VkCommandBufferLevel : int32_t { VK_COMMAND_BUFFER_LEVEL_PRIMARY = 0 };

enum VkExternalMemoryHandleTypeFlagBits : int32_t
{
    VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D12_RESOURCE_BIT = 0x00000040,
};

enum VkExternalSemaphoreHandleTypeFlagBits : int32_t
{
    VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_D3D12_FENCE_BIT = 0x00000008,
};

constexpr VkFlags VK_IMAGE_USAGE_TRANSFER_SRC_BIT = 0x00000001;
constexpr VkFlags VK_IMAGE_USAGE_TRANSFER_DST_BIT = 0x00000002;
constexpr VkFlags VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT = 0x00000001;
constexpr VkFlags VK_PIPELINE_STAGE_TRANSFER_BIT = 0x00001000;
constexpr VkFlags VK_PIPELINE_STAGE_ALL_COMMANDS_BIT = 0x00010000;
constexpr VkFlags VK_IMAGE_ASPECT_COLOR_BIT = 0x00000001;
constexpr VkFlags VK_ACCESS_TRANSFER_READ_BIT = 0x00000800;
constexpr VkFlags VK_ACCESS_TRANSFER_WRITE_BIT = 0x00001000;
constexpr VkFlags VK_ACCESS_MEMORY_READ_BIT = 0x00008000;
constexpr VkFlags VK_ACCESS_MEMORY_WRITE_BIT = 0x00010000;
constexpr VkFlags VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT = 0x00000002;
constexpr VkFlags VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT = 0x00000001;

struct VkExtent3D { uint32_t width, height, depth; };
struct VkXYColorEXT { float x, y; };

// VK_EXT_hdr_metadata (SetHDRMetaData of DXVK's swap chain)
struct VkHdrMetadataEXT
{
    VkStructureType sType;
    const void* pNext;
    VkXYColorEXT displayPrimaryRed;
    VkXYColorEXT displayPrimaryGreen;
    VkXYColorEXT displayPrimaryBlue;
    VkXYColorEXT whitePoint;
    float maxLuminance;
    float minLuminance;
    float maxContentLightLevel;
    float maxFrameAverageLightLevel;
};
struct VkOffset3D { int32_t x, y, z; };

struct VkImageCreateInfo
{
    VkStructureType sType;
    const void* pNext;
    VkFlags flags;
    VkImageType imageType;
    VkFormat format;
    VkExtent3D extent;
    uint32_t mipLevels;
    uint32_t arrayLayers;
    VkSampleCountFlagBits samples;
    VkImageTiling tiling;
    VkFlags usage;
    VkSharingMode sharingMode;
    uint32_t queueFamilyIndexCount;
    const uint32_t* pQueueFamilyIndices;
    VkImageLayout initialLayout;
};

struct VkMemoryRequirements
{
    VkDeviceSize size;
    VkDeviceSize alignment;
    uint32_t memoryTypeBits;
};

struct VkMemoryType { VkFlags propertyFlags; uint32_t heapIndex; };
struct VkMemoryHeap { VkDeviceSize size; VkFlags flags; };

struct VkPhysicalDeviceMemoryProperties
{
    uint32_t memoryTypeCount;
    VkMemoryType memoryTypes[32];
    uint32_t memoryHeapCount;
    VkMemoryHeap memoryHeaps[16];
};

struct VkPhysicalDeviceIDProperties
{
    VkStructureType sType;
    void* pNext;
    uint8_t deviceUUID[16];
    uint8_t driverUUID[16];
    uint8_t deviceLUID[8];
    uint32_t deviceNodeMask;
    VkBool32 deviceLUIDValid;
};

// VkPhysicalDeviceProperties (824 bytes, 8-byte aligned) is kept as bytes: only vendorID (offset 8) is read
struct VkPhysicalDeviceProperties2
{
    VkStructureType sType;
    void* pNext;
    alignas(8) uint8_t properties[1024];
};

struct VkMemoryWin32HandlePropertiesKHR
{
    VkStructureType sType;
    void* pNext;
    uint32_t memoryTypeBits;
};

struct VkExternalMemoryImageCreateInfo
{
    VkStructureType sType;
    const void* pNext;
    VkFlags handleTypes;
};

struct VkMemoryDedicatedAllocateInfo
{
    VkStructureType sType;
    const void* pNext;
    VkImage image;
    VkBuffer buffer;
};

struct VkImportMemoryWin32HandleInfoKHR
{
    VkStructureType sType;
    const void* pNext;
    VkExternalMemoryHandleTypeFlagBits handleType;
    HANDLE handle;
    LPCWSTR name;
};

struct VkMemoryAllocateInfo
{
    VkStructureType sType;
    const void* pNext;
    VkDeviceSize allocationSize;
    uint32_t memoryTypeIndex;
};

struct VkSemaphoreTypeCreateInfo
{
    VkStructureType sType;
    const void* pNext;
    VkSemaphoreType semaphoreType;
    uint64_t initialValue;
};

struct VkSemaphoreCreateInfo
{
    VkStructureType sType;
    const void* pNext;
    VkFlags flags;
};

struct VkImportSemaphoreWin32HandleInfoKHR
{
    VkStructureType sType;
    const void* pNext;
    VkSemaphore semaphore;
    VkFlags flags;
    VkExternalSemaphoreHandleTypeFlagBits handleType;
    HANDLE handle;
    LPCWSTR name;
};

struct VkCommandPoolCreateInfo
{
    VkStructureType sType;
    const void* pNext;
    VkFlags flags;
    uint32_t queueFamilyIndex;
};

struct VkCommandBufferAllocateInfo
{
    VkStructureType sType;
    const void* pNext;
    VkCommandPool commandPool;
    VkCommandBufferLevel level;
    uint32_t commandBufferCount;
};

struct VkCommandBufferBeginInfo
{
    VkStructureType sType;
    const void* pNext;
    VkFlags flags;
    const void* pInheritanceInfo;
};

struct VkFenceCreateInfo
{
    VkStructureType sType;
    const void* pNext;
    VkFlags flags;
};

struct VkImageSubresourceRange
{
    VkFlags aspectMask;
    uint32_t baseMipLevel;
    uint32_t levelCount;
    uint32_t baseArrayLayer;
    uint32_t layerCount;
};

struct VkImageMemoryBarrier
{
    VkStructureType sType;
    const void* pNext;
    VkFlags srcAccessMask;
    VkFlags dstAccessMask;
    VkImageLayout oldLayout;
    VkImageLayout newLayout;
    uint32_t srcQueueFamilyIndex;
    uint32_t dstQueueFamilyIndex;
    VkImage image;
    VkImageSubresourceRange subresourceRange;
};

struct VkImageSubresourceLayers
{
    VkFlags aspectMask;
    uint32_t mipLevel;
    uint32_t baseArrayLayer;
    uint32_t layerCount;
};

struct VkImageCopy
{
    VkImageSubresourceLayers srcSubresource;
    VkOffset3D srcOffset;
    VkImageSubresourceLayers dstSubresource;
    VkOffset3D dstOffset;
    VkExtent3D extent;
};

struct VkSubmitInfo
{
    VkStructureType sType;
    const void* pNext;
    uint32_t waitSemaphoreCount;
    const VkSemaphore* pWaitSemaphores;
    const VkFlags* pWaitDstStageMask;
    uint32_t commandBufferCount;
    const VkCommandBuffer* pCommandBuffers;
    uint32_t signalSemaphoreCount;
    const VkSemaphore* pSignalSemaphores;
};

struct VkTimelineSemaphoreSubmitInfo
{
    VkStructureType sType;
    const void* pNext;
    uint32_t waitSemaphoreValueCount;
    const uint64_t* pWaitSemaphoreValues;
    uint32_t signalSemaphoreValueCount;
    const uint64_t* pSignalSemaphoreValues;
};

static_assert(sizeof(VkImageCreateInfo) == (sizeof(void*) == 4 ? 68 : 88));
static_assert(sizeof(VkImageMemoryBarrier) == (sizeof(void*) == 4 ? 64 : 72));
static_assert(sizeof(VkImportSemaphoreWin32HandleInfoKHR) == (sizeof(void*) == 4 ? 32 : 48));

using PFN_vkVoidFunction = void(VKAPI_PTR*)();
using PFN_vkGetInstanceProcAddr = PFN_vkVoidFunction(VKAPI_PTR*)(VkInstance, const char*);
using PFN_vkGetDeviceProcAddr = PFN_vkVoidFunction(VKAPI_PTR*)(VkDevice, const char*);
using PFN_vkGetPhysicalDeviceProperties2 = void(VKAPI_PTR*)(VkPhysicalDevice, VkPhysicalDeviceProperties2*);
using PFN_vkGetPhysicalDeviceMemoryProperties = void(VKAPI_PTR*)(VkPhysicalDevice, VkPhysicalDeviceMemoryProperties*);
using PFN_vkCreateImage = VkResult(VKAPI_PTR*)(VkDevice, const VkImageCreateInfo*, const void*, VkImage*);
using PFN_vkDestroyImage = void(VKAPI_PTR*)(VkDevice, VkImage, const void*);
using PFN_vkGetImageMemoryRequirements = void(VKAPI_PTR*)(VkDevice, VkImage, VkMemoryRequirements*);
using PFN_vkAllocateMemory = VkResult(VKAPI_PTR*)(VkDevice, const VkMemoryAllocateInfo*, const void*, VkDeviceMemory*);
using PFN_vkFreeMemory = void(VKAPI_PTR*)(VkDevice, VkDeviceMemory, const void*);
using PFN_vkBindImageMemory = VkResult(VKAPI_PTR*)(VkDevice, VkImage, VkDeviceMemory, VkDeviceSize);
using PFN_vkGetMemoryWin32HandlePropertiesKHR = VkResult(VKAPI_PTR*)(VkDevice, VkExternalMemoryHandleTypeFlagBits, HANDLE, VkMemoryWin32HandlePropertiesKHR*);
using PFN_vkCreateSemaphore = VkResult(VKAPI_PTR*)(VkDevice, const VkSemaphoreCreateInfo*, const void*, VkSemaphore*);
using PFN_vkDestroySemaphore = void(VKAPI_PTR*)(VkDevice, VkSemaphore, const void*);
using PFN_vkImportSemaphoreWin32HandleKHR = VkResult(VKAPI_PTR*)(VkDevice, const VkImportSemaphoreWin32HandleInfoKHR*);
using PFN_vkCreateCommandPool = VkResult(VKAPI_PTR*)(VkDevice, const VkCommandPoolCreateInfo*, const void*, VkCommandPool*);
using PFN_vkAllocateCommandBuffers = VkResult(VKAPI_PTR*)(VkDevice, const VkCommandBufferAllocateInfo*, VkCommandBuffer*);
using PFN_vkBeginCommandBuffer = VkResult(VKAPI_PTR*)(VkCommandBuffer, const VkCommandBufferBeginInfo*);
using PFN_vkEndCommandBuffer = VkResult(VKAPI_PTR*)(VkCommandBuffer);
using PFN_vkResetCommandBuffer = VkResult(VKAPI_PTR*)(VkCommandBuffer, VkFlags);
using PFN_vkCmdPipelineBarrier = void(VKAPI_PTR*)(VkCommandBuffer, VkFlags, VkFlags, VkFlags, uint32_t, const void*, uint32_t, const void*, uint32_t, const VkImageMemoryBarrier*);
using PFN_vkCmdCopyImage = void(VKAPI_PTR*)(VkCommandBuffer, VkImage, VkImageLayout, VkImage, VkImageLayout, uint32_t, const VkImageCopy*);
using PFN_vkQueueSubmit = VkResult(VKAPI_PTR*)(VkQueue, uint32_t, const VkSubmitInfo*, VkFence);
using PFN_vkCreateFence = VkResult(VKAPI_PTR*)(VkDevice, const VkFenceCreateInfo*, const void*, VkFence*);
using PFN_vkWaitForFences = VkResult(VKAPI_PTR*)(VkDevice, uint32_t, const VkFence*, VkBool32, uint64_t);
using PFN_vkResetFences = VkResult(VKAPI_PTR*)(VkDevice, uint32_t, const VkFence*);
