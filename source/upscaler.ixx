module;

#include <common.hxx>
#include <d3d12.h>
#include <d3d9on12.h>
#include <dxvk_interop.hpp>
#include <upscaler_protocol.hpp>

export module upscaler;

import common;
import comvars;

namespace Protocol = UpscalerProtocol;

// NVIDIA DLSS and AMD FSR, run by GTAIV.EFLC.FusionFix.exe (x64) next to the plugin.
//
// The helper creates shared D3D12 textures and a shared fence. The plugin copies the frame's color, depth,
// motion vectors and reactive mask into them on the GPU, the helper waits for that on its queue, runs the
// upscaler and signals the fence, and the plugin copies the result back, all without the CPU waiting for
// the GPU. The copies run where the game renders:
// - DXVK: the shared textures and the fence are imported into DXVK's Vulkan device, the copies are
//   submitted to DXVK's queue.
// - D3D9on12 (Graphics API "DirectX 12"): the shared textures and the fence are opened on the D3D12 device
//   of D3D9on12, the game's textures are unwrapped to their D3D12 resources and copied on a queue of the
//   plugin.

namespace
{
    constexpr size_t TextureCount = static_cast<size_t>(Protocol::Texture::Count);
    constexpr size_t InputCount = Protocol::InputCount;
    constexpr size_t OutputIndex = static_cast<size_t>(Protocol::Texture::Output);

    // The helper duplicated its handles into this process, they are closed once imported or not
    void CloseSharedHandles(Protocol::Shared& shared)
    {
        for (auto& handle : shared.TextureHandles)
        {
            if (handle)
                CloseHandle(reinterpret_cast<HANDLE>(handle));
            handle = 0;
        }
        if (shared.FenceHandle)
            CloseHandle(reinterpret_cast<HANDLE>(shared.FenceHandle));
        shared.FenceHandle = 0;
    }

    uint32_t AdapterVendor(IDirect3DDevice9* device)
    {
        uint32_t vendor = 0;
        IDirect3D9* d3d = nullptr;
        if (SUCCEEDED(device->GetDirect3D(&d3d)) && d3d)
        {
            D3DDEVICE_CREATION_PARAMETERS parameters{};
            D3DADAPTER_IDENTIFIER9 identifier{};
            if (SUCCEEDED(device->GetCreationParameters(&parameters)) && SUCCEEDED(d3d->GetAdapterIdentifier(parameters.AdapterOrdinal, 0, &identifier)))
                vendor = identifier.VendorId;
            d3d->Release();
        }
        return vendor;
    }

    // Exchange of the frames between the game's renderer and the shared textures
    class Bridge
    {
    public:
        LUID luid{};
        uint32_t vendorId = 0;
        uint32_t width = 0;
        uint32_t height = 0;

        virtual ~Bridge() = default;
        // Opens the shared textures and the fence of a configuration, and closes their handles
        virtual bool Import(Protocol::Shared& shared, uint32_t w, uint32_t h) = 0;
        virtual void ReleaseImports() = 0;
        // Game textures -> shared textures, then the fence reaches signalValue. Null inputs are skipped.
        virtual bool SubmitInputs(IDirect3DTexture9* const (&inputs)[InputCount], uint64_t signalValue) = 0;
        // Once the fence reaches waitValue: shared output -> game texture
        virtual bool SubmitOutput(IDirect3DTexture9* target, uint64_t waitValue) = 0;
    };
    struct Vulkan
    {
        PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr = nullptr;
        PFN_vkGetDeviceProcAddr vkGetDeviceProcAddr = nullptr;
        PFN_vkGetPhysicalDeviceProperties2 vkGetPhysicalDeviceProperties2 = nullptr;
        PFN_vkGetPhysicalDeviceMemoryProperties vkGetPhysicalDeviceMemoryProperties = nullptr;
        PFN_vkCreateImage vkCreateImage = nullptr;
        PFN_vkDestroyImage vkDestroyImage = nullptr;
        PFN_vkGetImageMemoryRequirements vkGetImageMemoryRequirements = nullptr;
        PFN_vkAllocateMemory vkAllocateMemory = nullptr;
        PFN_vkFreeMemory vkFreeMemory = nullptr;
        PFN_vkBindImageMemory vkBindImageMemory = nullptr;
        PFN_vkGetMemoryWin32HandlePropertiesKHR vkGetMemoryWin32HandlePropertiesKHR = nullptr;
        PFN_vkCreateSemaphore vkCreateSemaphore = nullptr;
        PFN_vkDestroySemaphore vkDestroySemaphore = nullptr;
        PFN_vkImportSemaphoreWin32HandleKHR vkImportSemaphoreWin32HandleKHR = nullptr;
        PFN_vkCreateCommandPool vkCreateCommandPool = nullptr;
        PFN_vkAllocateCommandBuffers vkAllocateCommandBuffers = nullptr;
        PFN_vkBeginCommandBuffer vkBeginCommandBuffer = nullptr;
        PFN_vkEndCommandBuffer vkEndCommandBuffer = nullptr;
        PFN_vkResetCommandBuffer vkResetCommandBuffer = nullptr;
        PFN_vkCmdPipelineBarrier vkCmdPipelineBarrier = nullptr;
        PFN_vkCmdCopyImage vkCmdCopyImage = nullptr;
        PFN_vkQueueSubmit vkQueueSubmit = nullptr;
        PFN_vkCreateFence vkCreateFence = nullptr;
        PFN_vkWaitForFences vkWaitForFences = nullptr;
        PFN_vkResetFences vkResetFences = nullptr;

        bool Load(VkInstance instance, VkDevice device)
        {
            HMODULE loader = GetModuleHandleW(L"vulkan-1.dll");
            if (!loader)
                loader = GetModuleHandleW(L"winevulkan.dll");
            if (!loader)
                return false;

            vkGetInstanceProcAddr = reinterpret_cast<PFN_vkGetInstanceProcAddr>(GetProcAddress(loader, "vkGetInstanceProcAddr"));
            if (!vkGetInstanceProcAddr)
                return false;

#define LOAD_INSTANCE(name) name = reinterpret_cast<PFN_##name>(vkGetInstanceProcAddr(instance, #name))
#define LOAD_DEVICE(name) name = reinterpret_cast<PFN_##name>(vkGetDeviceProcAddr(device, #name))
            LOAD_INSTANCE(vkGetDeviceProcAddr);
            LOAD_INSTANCE(vkGetPhysicalDeviceProperties2);
            LOAD_INSTANCE(vkGetPhysicalDeviceMemoryProperties);
            if (!vkGetDeviceProcAddr || !vkGetPhysicalDeviceProperties2 || !vkGetPhysicalDeviceMemoryProperties)
                return false;

            LOAD_DEVICE(vkCreateImage);
            LOAD_DEVICE(vkDestroyImage);
            LOAD_DEVICE(vkGetImageMemoryRequirements);
            LOAD_DEVICE(vkAllocateMemory);
            LOAD_DEVICE(vkFreeMemory);
            LOAD_DEVICE(vkBindImageMemory);
            LOAD_DEVICE(vkGetMemoryWin32HandlePropertiesKHR);
            LOAD_DEVICE(vkCreateSemaphore);
            LOAD_DEVICE(vkDestroySemaphore);
            LOAD_DEVICE(vkImportSemaphoreWin32HandleKHR);
            LOAD_DEVICE(vkCreateCommandPool);
            LOAD_DEVICE(vkAllocateCommandBuffers);
            LOAD_DEVICE(vkBeginCommandBuffer);
            LOAD_DEVICE(vkEndCommandBuffer);
            LOAD_DEVICE(vkResetCommandBuffer);
            LOAD_DEVICE(vkCmdPipelineBarrier);
            LOAD_DEVICE(vkCmdCopyImage);
            LOAD_DEVICE(vkQueueSubmit);
            LOAD_DEVICE(vkCreateFence);
            LOAD_DEVICE(vkWaitForFences);
            LOAD_DEVICE(vkResetFences);
#undef LOAD_INSTANCE
#undef LOAD_DEVICE

            // The external memory and semaphore functions only exist when DXVK enabled the extensions
            return vkCreateImage && vkDestroyImage && vkGetImageMemoryRequirements && vkAllocateMemory && vkFreeMemory && vkBindImageMemory &&
                vkGetMemoryWin32HandlePropertiesKHR && vkCreateSemaphore && vkDestroySemaphore && vkImportSemaphoreWin32HandleKHR &&
                vkCreateCommandPool && vkAllocateCommandBuffers && vkBeginCommandBuffer && vkEndCommandBuffer && vkResetCommandBuffer &&
                vkCmdPipelineBarrier && vkCmdCopyImage && vkQueueSubmit && vkCreateFence && vkWaitForFences && vkResetFences;
        }
    };

    struct GameImage
    {
        VkImage image = VK_NULL_HANDLE;
        VkImageLayout layout = VK_IMAGE_LAYOUT_UNDEFINED;
        VkFormat format = VK_FORMAT_UNDEFINED;
        VkExtent3D extent{};
    };

    bool GetGameImage(IDirect3DTexture9* texture, GameImage& out)
    {
        if (!texture)
            return false;

        ID3D9VkInteropTexture* interop = nullptr;
        if (FAILED(texture->QueryInterface(__uuidof(ID3D9VkInteropTexture), reinterpret_cast<void**>(&interop))) || !interop)
            return false;

        VkImageCreateInfo info{ VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
        auto hr = interop->GetVulkanImageInfo(&out.image, &out.layout, &info);
        interop->Release();
        out.format = info.format;
        out.extent = info.extent;
        return SUCCEEDED(hr) && out.image != VK_NULL_HANDLE;
    }

    VkImageMemoryBarrier ImageBarrier(VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout, VkAccessFlags srcAccess, VkAccessFlags dstAccess,
        uint32_t srcQueueFamily = VK_QUEUE_FAMILY_IGNORED, uint32_t dstQueueFamily = VK_QUEUE_FAMILY_IGNORED)
    {
        VkImageMemoryBarrier barrier{ VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
        barrier.srcAccessMask = srcAccess;
        barrier.dstAccessMask = dstAccess;
        barrier.oldLayout = oldLayout;
        barrier.newLayout = newLayout;
        barrier.srcQueueFamilyIndex = srcQueueFamily;
        barrier.dstQueueFamilyIndex = dstQueueFamily;
        barrier.image = image;
        barrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
        return barrier;
    }

    VkImageCopy FullCopy(uint32_t width, uint32_t height)
    {
        VkImageCopy copy{};
        copy.srcSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
        copy.dstSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
        copy.extent = { width, height, 1 };
        return copy;
    }

    // ---------------------------------------------------------------------------------------------
    // DXVK

    class DXVKBridge : public Bridge
    {
    public:
        ID3D9VkInteropDevice* interop = nullptr;
        VkInstance instance = VK_NULL_HANDLE;
        VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
        VkDevice device = VK_NULL_HANDLE;
        VkQueue queue = VK_NULL_HANDLE;
        uint32_t queueFamily = 0;
        Vulkan vk;

        VkCommandPool pool = VK_NULL_HANDLE;
        struct Slot
        {
            VkCommandBuffer inputs = VK_NULL_HANDLE;
            VkCommandBuffer output = VK_NULL_HANDLE;
            VkFence fence = VK_NULL_HANDLE;
            bool submitted = false;
        };
        std::array<Slot, 4> slots{};
        uint32_t slot = 0;

        struct SharedImage
        {
            VkImage image = VK_NULL_HANDLE;
            VkDeviceMemory memory = VK_NULL_HANDLE;
            VkFormat format = VK_FORMAT_UNDEFINED;
        };
        std::array<SharedImage, TextureCount> images{};
        VkSemaphore semaphore = VK_NULL_HANDLE;

        // Vulkan formats of the shared textures and of the game textures copied from and to them
        static constexpr VkFormat Formats[TextureCount] =
        {
            VK_FORMAT_R16G16B16A16_SFLOAT, VK_FORMAT_R32_SFLOAT, VK_FORMAT_R16G16_SFLOAT, VK_FORMAT_R16_SFLOAT, VK_FORMAT_R16G16B16A16_SFLOAT
        };

        bool Init(IDirect3DDevice9* realDevice)
        {
            if (FAILED(realDevice->QueryInterface(__uuidof(ID3D9VkInteropDevice), reinterpret_cast<void**>(&interop))) || !interop)
                return false;

            interop->GetVulkanHandles(&instance, &physicalDevice, &device);
            uint32_t queueIndex = 0;
            interop->GetSubmissionQueue(&queue, &queueIndex, &queueFamily);
            if (!instance || !physicalDevice || !device || !queue || !vk.Load(instance, device))
                return false;

            VkPhysicalDeviceIDProperties id{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ID_PROPERTIES };
            VkPhysicalDeviceProperties2 properties{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2 };
            properties.pNext = &id;
            vk.vkGetPhysicalDeviceProperties2(physicalDevice, &properties);
            if (!id.deviceLUIDValid)
                return false;
            std::memcpy(&luid, id.deviceLUID, sizeof(luid));
            vendorId = properties.properties.vendorID;

            VkCommandPoolCreateInfo poolInfo{ VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
            poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
            poolInfo.queueFamilyIndex = queueFamily;
            if (vk.vkCreateCommandPool(device, &poolInfo, nullptr, &pool) != VK_SUCCESS)
                return false;

            for (auto& s : slots)
            {
                VkCommandBuffer buffers[2]{};
                VkCommandBufferAllocateInfo allocate{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
                allocate.commandPool = pool;
                allocate.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
                allocate.commandBufferCount = 2;
                if (vk.vkAllocateCommandBuffers(device, &allocate, buffers) != VK_SUCCESS)
                    return false;
                s.inputs = buffers[0];
                s.output = buffers[1];

                VkFenceCreateInfo fenceInfo{ VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
                if (vk.vkCreateFence(device, &fenceInfo, nullptr, &s.fence) != VK_SUCCESS)
                    return false;
            }
            return true;
        }

        void ReleaseImports() override
        {
            // Nothing may still be using the images: wait for the last submissions
            for (auto& s : slots)
            {
                if (s.submitted)
                {
                    vk.vkWaitForFences(device, 1, &s.fence, VK_TRUE, 2000000000ull);
                    vk.vkResetFences(device, 1, &s.fence);
                    s.submitted = false;
                }
            }
            for (auto& image : images)
            {
                if (image.image)
                    vk.vkDestroyImage(device, image.image, nullptr);
                if (image.memory)
                    vk.vkFreeMemory(device, image.memory, nullptr);
                image = {};
            }
            if (semaphore)
                vk.vkDestroySemaphore(device, semaphore, nullptr);
            semaphore = VK_NULL_HANDLE;
            width = height = 0;
        }

        bool ImportImage(SharedImage& target, HANDLE handle, VkFormat format, uint32_t w, uint32_t h)
        {
            VkExternalMemoryImageCreateInfo external{ VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO };
            external.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D12_RESOURCE_BIT;

            VkImageCreateInfo info{ VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
            info.pNext = &external;
            info.imageType = VK_IMAGE_TYPE_2D;
            info.format = format;
            info.extent = { w, h, 1 };
            info.mipLevels = 1;
            info.arrayLayers = 1;
            info.samples = VK_SAMPLE_COUNT_1_BIT;
            info.tiling = VK_IMAGE_TILING_OPTIMAL;
            info.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
            info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
            info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            if (vk.vkCreateImage(device, &info, nullptr, &target.image) != VK_SUCCESS)
                return false;
            target.format = format;

            VkMemoryRequirements requirements{};
            vk.vkGetImageMemoryRequirements(device, target.image, &requirements);

            VkMemoryWin32HandlePropertiesKHR handleProperties{ VK_STRUCTURE_TYPE_MEMORY_WIN32_HANDLE_PROPERTIES_KHR };
            if (vk.vkGetMemoryWin32HandlePropertiesKHR(device, VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D12_RESOURCE_BIT, handle, &handleProperties) != VK_SUCCESS)
                return false;

            VkPhysicalDeviceMemoryProperties memoryProperties{};
            vk.vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memoryProperties);
            uint32_t typeIndex = UINT32_MAX;
            auto types = requirements.memoryTypeBits & handleProperties.memoryTypeBits;
            for (uint32_t i = 0; i < memoryProperties.memoryTypeCount; ++i)
            {
                if ((types & (1u << i)) && (memoryProperties.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT))
                {
                    typeIndex = i;
                    break;
                }
            }
            if (typeIndex == UINT32_MAX)
                return false;

            VkMemoryDedicatedAllocateInfo dedicated{ VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO };
            dedicated.image = target.image;

            VkImportMemoryWin32HandleInfoKHR import{ VK_STRUCTURE_TYPE_IMPORT_MEMORY_WIN32_HANDLE_INFO_KHR };
            import.pNext = &dedicated;
            import.handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D12_RESOURCE_BIT;
            import.handle = handle;

            VkMemoryAllocateInfo allocate{ VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
            allocate.pNext = &import;
            allocate.allocationSize = requirements.size;
            allocate.memoryTypeIndex = typeIndex;
            if (vk.vkAllocateMemory(device, &allocate, nullptr, &target.memory) != VK_SUCCESS)
                return false;

            return vk.vkBindImageMemory(device, target.image, target.memory, 0) == VK_SUCCESS;
        }

        bool Import(Protocol::Shared& shared, uint32_t w, uint32_t h) override
        {
            ReleaseImports();

            bool ok = true;
            for (size_t i = 0; i < images.size(); ++i)
            {
                auto handle = reinterpret_cast<HANDLE>(shared.TextureHandles[i]);
                ok = ok && handle && ImportImage(images[i], handle, Formats[i], w, h);
            }

            auto fenceHandle = reinterpret_cast<HANDLE>(shared.FenceHandle);
            if (ok && fenceHandle)
            {
                VkSemaphoreTypeCreateInfo type{ VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO };
                type.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
                VkSemaphoreCreateInfo info{ VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
                info.pNext = &type;
                ok = vk.vkCreateSemaphore(device, &info, nullptr, &semaphore) == VK_SUCCESS;

                if (ok)
                {
                    VkImportSemaphoreWin32HandleInfoKHR import{ VK_STRUCTURE_TYPE_IMPORT_SEMAPHORE_WIN32_HANDLE_INFO_KHR };
                    import.semaphore = semaphore;
                    import.handleType = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_D3D12_FENCE_BIT;
                    import.handle = fenceHandle;
                    ok = vk.vkImportSemaphoreWin32HandleKHR(device, &import) == VK_SUCCESS;
                }
            }
            else
            {
                ok = false;
            }
            CloseSharedHandles(shared);

            if (!ok)
            {
                ReleaseImports();
                return false;
            }

            width = w;
            height = h;
            return true;
        }

        Slot& NextSlot()
        {
            slot = (slot + 1) % slots.size();
            auto& s = slots[slot];
            if (s.submitted)
            {
                vk.vkWaitForFences(device, 1, &s.fence, VK_TRUE, 2000000000ull);
                vk.vkResetFences(device, 1, &s.fence);
                s.submitted = false;
            }
            return s;
        }

        bool Submit(VkCommandBuffer buffer, VkSemaphore waitSemaphore, uint64_t waitValue, uint64_t signalValue, VkFence fence)
        {
            VkTimelineSemaphoreSubmitInfo timeline{ VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO };
            VkSubmitInfo submit{ VK_STRUCTURE_TYPE_SUBMIT_INFO };
            submit.pNext = &timeline;
            submit.commandBufferCount = 1;
            submit.pCommandBuffers = &buffer;

            VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
            if (waitValue)
            {
                submit.waitSemaphoreCount = 1;
                submit.pWaitSemaphores = &waitSemaphore;
                submit.pWaitDstStageMask = &waitStage;
                timeline.waitSemaphoreValueCount = 1;
                timeline.pWaitSemaphoreValues = &waitValue;
            }
            if (signalValue)
            {
                submit.signalSemaphoreCount = 1;
                submit.pSignalSemaphores = &semaphore;
                timeline.signalSemaphoreValueCount = 1;
                timeline.pSignalSemaphoreValues = &signalValue;
            }

            interop->LockSubmissionQueue();
            auto result = vk.vkQueueSubmit(queue, 1, &submit, fence);
            interop->ReleaseSubmissionQueue();
            return result == VK_SUCCESS;
        }

        bool SubmitInputs(IDirect3DTexture9* const (&inputs)[InputCount], uint64_t signalValue) override
        {
            GameImage sources[InputCount];
            for (size_t i = 0; i < InputCount; ++i)
            {
                if (!inputs[i])
                    continue;
                if (!GetGameImage(inputs[i], sources[i]) || sources[i].format != Formats[i] || sources[i].extent.width != width || sources[i].extent.height != height)
                    return false;
            }

            auto& s = NextSlot();
            auto cmd = s.inputs;
            vk.vkResetCommandBuffer(cmd, 0);
            VkCommandBufferBeginInfo begin{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
            begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
            vk.vkBeginCommandBuffer(cmd, &begin);

            VkImageMemoryBarrier before[InputCount * 2];
            VkImageMemoryBarrier after[InputCount * 2];
            uint32_t barriers = 0;
            for (size_t i = 0; i < InputCount; ++i)
            {
                if (!inputs[i])
                    continue;
                auto shared = images[i].image;
                before[barriers] = ImageBarrier(sources[i].image, sources[i].layout, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_ACCESS_MEMORY_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT);
                after[barriers++] = ImageBarrier(sources[i].image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, sources[i].layout, VK_ACCESS_TRANSFER_READ_BIT, VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT);
                before[barriers] = ImageBarrier(shared, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 0, VK_ACCESS_TRANSFER_WRITE_BIT);
                // Handed over to D3D12, where it is in the common state
                after[barriers++] = ImageBarrier(shared, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_TRANSFER_WRITE_BIT, 0, queueFamily, VK_QUEUE_FAMILY_EXTERNAL);
            }

            vk.vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, barriers, before);
            for (size_t i = 0; i < InputCount; ++i)
            {
                if (!inputs[i])
                    continue;
                auto copy = FullCopy(width, height);
                vk.vkCmdCopyImage(cmd, sources[i].image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, images[i].image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);
            }
            vk.vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0, nullptr, barriers, after);
            vk.vkEndCommandBuffer(cmd);

            // Everything the game rendered so far must reach the queue first
            interop->FlushRenderingCommands();
            return Submit(cmd, VK_NULL_HANDLE, 0, signalValue, VK_NULL_HANDLE);
        }

        bool SubmitOutput(IDirect3DTexture9* target, uint64_t waitValue) override
        {
            GameImage destination;
            if (!GetGameImage(target, destination) || destination.format != Formats[OutputIndex] ||
                destination.extent.width != width || destination.extent.height != height)
                return false;

            auto& s = slots[slot];
            auto output = images[OutputIndex].image;
            auto cmd = s.output;
            vk.vkResetCommandBuffer(cmd, 0);
            VkCommandBufferBeginInfo begin{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
            begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
            vk.vkBeginCommandBuffer(cmd, &begin);

            VkImageMemoryBarrier before[2] =
            {
                ImageBarrier(output, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, 0, VK_ACCESS_TRANSFER_READ_BIT, VK_QUEUE_FAMILY_EXTERNAL, queueFamily),
                ImageBarrier(destination.image, destination.layout, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT, VK_ACCESS_TRANSFER_WRITE_BIT),
            };
            VkImageMemoryBarrier after[2] =
            {
                ImageBarrier(output, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_TRANSFER_READ_BIT, 0, queueFamily, VK_QUEUE_FAMILY_EXTERNAL),
                ImageBarrier(destination.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, destination.layout, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT),
            };

            vk.vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 2, before);
            auto copy = FullCopy(width, height);
            vk.vkCmdCopyImage(cmd, output, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, destination.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);
            vk.vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0, nullptr, 2, after);
            vk.vkEndCommandBuffer(cmd);

            if (!Submit(cmd, semaphore, waitValue, 0, s.fence))
                return false;
            s.submitted = true;
            return true;
        }
    };

    // ---------------------------------------------------------------------------------------------
    // D3D9on12

    DXGI_FORMAT TypelessFormat(DXGI_FORMAT format)
    {
        switch (format)
        {
        case DXGI_FORMAT_R16G16B16A16_FLOAT: case DXGI_FORMAT_R16G16B16A16_UNORM: case DXGI_FORMAT_R16G16B16A16_UINT:
        case DXGI_FORMAT_R16G16B16A16_SNORM: case DXGI_FORMAT_R16G16B16A16_SINT:
            return DXGI_FORMAT_R16G16B16A16_TYPELESS;
        case DXGI_FORMAT_R32_FLOAT: case DXGI_FORMAT_R32_UINT: case DXGI_FORMAT_R32_SINT:
            return DXGI_FORMAT_R32_TYPELESS;
        case DXGI_FORMAT_R16G16_FLOAT: case DXGI_FORMAT_R16G16_UNORM: case DXGI_FORMAT_R16G16_UINT:
        case DXGI_FORMAT_R16G16_SNORM: case DXGI_FORMAT_R16G16_SINT:
            return DXGI_FORMAT_R16G16_TYPELESS;
        case DXGI_FORMAT_R16_FLOAT: case DXGI_FORMAT_R16_UNORM: case DXGI_FORMAT_R16_UINT:
        case DXGI_FORMAT_R16_SNORM: case DXGI_FORMAT_R16_SINT:
            return DXGI_FORMAT_R16_TYPELESS;
        default:
            return format;
        }
    }

    D3D12_RESOURCE_BARRIER Transition(ID3D12Resource* resource, D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after)
    {
        D3D12_RESOURCE_BARRIER barrier{};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Transition.pResource = resource;
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        barrier.Transition.StateBefore = before;
        barrier.Transition.StateAfter = after;
        return barrier;
    }

    class D3D12Bridge : public Bridge
    {
    public:
        IDirect3DDevice9* device9 = nullptr;
        IDirect3DDevice9On12* on12 = nullptr;
        ID3D12Device* device = nullptr;
        ID3D12CommandQueue* queue = nullptr;
        ID3D12Fence* fence = nullptr;          // completion of the copies, for D3D9on12 and the command allocators
        uint64_t fenceValue = 0;
        HANDLE event = nullptr;

        struct Slot
        {
            ID3D12CommandAllocator* allocator = nullptr;
            ID3D12GraphicsCommandList* list = nullptr;
            uint64_t value = 0;               // fence value the GPU is done with the slot at
        };
        std::array<Slot, 8> slots{};
        uint32_t slot = 0;

        std::array<ID3D12Resource*, TextureCount> images{};
        ID3D12Fence* sharedFence = nullptr;

        bool Init(IDirect3DDevice9* realDevice)
        {
            if (FAILED(realDevice->QueryInterface(__uuidof(IDirect3DDevice9On12), reinterpret_cast<void**>(&on12))) || !on12)
                return false;
            if (FAILED(on12->GetD3D12Device(IID_PPV_ARGS(&device))) || !device)
                return false;

            device9 = realDevice;
            luid = device->GetAdapterLuid();
            vendorId = AdapterVendor(realDevice);

            D3D12_COMMAND_QUEUE_DESC queueDesc{};
            queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
            if (FAILED(device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&queue))))
                return false;
            if (FAILED(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence))))
                return false;

            for (auto& s : slots)
            {
                if (FAILED(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&s.allocator))))
                    return false;
                if (FAILED(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, s.allocator, nullptr, IID_PPV_ARGS(&s.list))))
                    return false;
                s.list->Close();
            }

            event = CreateEventW(nullptr, FALSE, FALSE, nullptr);
            return event != nullptr;
        }

        void WaitFor(uint64_t value)
        {
            if (value && fence->GetCompletedValue() < value && SUCCEEDED(fence->SetEventOnCompletion(value, event)))
                WaitForSingleObject(event, 2000);
        }

        Slot& NextSlot()
        {
            slot = (slot + 1) % slots.size();
            auto& s = slots[slot];
            WaitFor(s.value);
            s.allocator->Reset();
            s.list->Reset(s.allocator, nullptr);
            return s;
        }

        // Advances the fence after the work submitted so far: D3D9on12 waits for it before it uses a returned
        // resource again
        uint64_t SignalCopies()
        {
            queue->Signal(fence, ++fenceValue);
            return fenceValue;
        }

        void Return(IDirect3DTexture9* texture, ID3D12Resource*& resource)
        {
            ID3D12Fence* fences[] = { fence };
            UINT64 values[] = { fenceValue };
            on12->ReturnUnderlyingResource(texture, 1, values, fences);
            resource->Release();
            resource = nullptr;
        }

        bool Matches(ID3D12Resource* gameResource, ID3D12Resource* shared)
        {
            auto a = gameResource->GetDesc();
            auto b = shared->GetDesc();
            return a.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE2D && a.Width == b.Width && a.Height == b.Height &&
                TypelessFormat(a.Format) == TypelessFormat(b.Format);
        }

        void Copy(ID3D12GraphicsCommandList* list, ID3D12Resource* destination, ID3D12Resource* source)
        {
            D3D12_TEXTURE_COPY_LOCATION to{};
            to.pResource = destination;
            to.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
            to.SubresourceIndex = 0;
            D3D12_TEXTURE_COPY_LOCATION from{};
            from.pResource = source;
            from.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
            from.SubresourceIndex = 0;
            D3D12_BOX box{ 0, 0, 0, width, height, 1 };
            list->CopyTextureRegion(&to, 0, 0, 0, &from, &box);
        }

        void ReleaseImports() override
        {
            WaitFor(fenceValue);
            for (auto& image : images)
            {
                if (image)
                    image->Release();
                image = nullptr;
            }
            if (sharedFence)
                sharedFence->Release();
            sharedFence = nullptr;
            width = height = 0;
        }

        bool Import(Protocol::Shared& shared, uint32_t w, uint32_t h) override
        {
            ReleaseImports();

            bool ok = true;
            for (size_t i = 0; i < images.size(); ++i)
            {
                auto handle = reinterpret_cast<HANDLE>(shared.TextureHandles[i]);
                ok = ok && handle && SUCCEEDED(device->OpenSharedHandle(handle, IID_PPV_ARGS(&images[i])));
            }
            auto fenceHandle = reinterpret_cast<HANDLE>(shared.FenceHandle);
            ok = ok && fenceHandle && SUCCEEDED(device->OpenSharedHandle(fenceHandle, IID_PPV_ARGS(&sharedFence)));
            CloseSharedHandles(shared);

            if (!ok)
            {
                ReleaseImports();
                return false;
            }

            width = w;
            height = h;
            return true;
        }

        bool SubmitInputs(IDirect3DTexture9* const (&inputs)[InputCount], uint64_t signalValue) override
        {
            // D3D9on12 records into command lists of its own, which have to reach the GPU before this queue can
            // wait for them. An event query is the D3D9 way to submit them.
            IDirect3DQuery9* query = nullptr;
            if (SUCCEEDED(device9->CreateQuery(D3DQUERYTYPE_EVENT, &query)) && query)
            {
                query->Issue(D3DISSUE_END);
                query->GetData(nullptr, 0, D3DGETDATA_FLUSH);
                query->Release();
            }

            // Unwrapping makes the queue wait for the D3D9 work on the resource, and leaves it in the common state
            ID3D12Resource* sources[InputCount]{};
            bool ok = true;
            for (size_t i = 0; i < InputCount && ok; ++i)
            {
                if (!inputs[i])
                    continue;
                ok = SUCCEEDED(on12->UnwrapUnderlyingResource(inputs[i], queue, IID_PPV_ARGS(&sources[i]))) && sources[i] &&
                    Matches(sources[i], images[i]);
            }

            if (ok)
            {
                auto& s = NextSlot();
                D3D12_RESOURCE_BARRIER before[InputCount * 2];
                D3D12_RESOURCE_BARRIER after[InputCount * 2];
                UINT barriers = 0;
                for (size_t i = 0; i < InputCount; ++i)
                {
                    if (!sources[i])
                        continue;
                    before[barriers] = Transition(sources[i], D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_SOURCE);
                    after[barriers++] = Transition(sources[i], D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_COMMON);
                    before[barriers] = Transition(images[i], D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST);
                    after[barriers++] = Transition(images[i], D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_COMMON);
                }
                s.list->ResourceBarrier(barriers, before);
                for (size_t i = 0; i < InputCount; ++i)
                    if (sources[i])
                        Copy(s.list, images[i], sources[i]);
                s.list->ResourceBarrier(barriers, after);
                ok = SUCCEEDED(s.list->Close());

                if (ok)
                {
                    ID3D12CommandList* lists[] = { s.list };
                    queue->ExecuteCommandLists(1, lists);
                    queue->Signal(sharedFence, signalValue);
                }
                s.value = SignalCopies();
            }
            else
            {
                SignalCopies();
            }

            for (size_t i = 0; i < InputCount; ++i)
                if (sources[i])
                    Return(inputs[i], sources[i]);
            return ok;
        }

        bool SubmitOutput(IDirect3DTexture9* target, uint64_t waitValue) override
        {
            ID3D12Resource* destination = nullptr;
            if (FAILED(on12->UnwrapUnderlyingResource(target, queue, IID_PPV_ARGS(&destination))) || !destination)
                return false;

            bool ok = Matches(destination, images[OutputIndex]);
            if (ok)
            {
                auto& s = NextSlot();
                D3D12_RESOURCE_BARRIER before[2] =
                {
                    Transition(images[OutputIndex], D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_SOURCE),
                    Transition(destination, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST),
                };
                D3D12_RESOURCE_BARRIER after[2] =
                {
                    Transition(images[OutputIndex], D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_COMMON),
                    Transition(destination, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_COMMON),
                };
                s.list->ResourceBarrier(2, before);
                Copy(s.list, destination, images[OutputIndex]);
                s.list->ResourceBarrier(2, after);
                ok = SUCCEEDED(s.list->Close());

                if (ok)
                {
                    queue->Wait(sharedFence, waitValue);
                    ID3D12CommandList* lists[] = { s.list };
                    queue->ExecuteCommandLists(1, lists);
                }
                s.value = SignalCopies();
            }
            else
            {
                SignalCopies();
            }

            Return(target, destination);
            return ok;
        }
    };

    // ---------------------------------------------------------------------------------------------
    // Helper process

    class HelperProcess
    {
    public:
        HANDLE process = nullptr;
        HANDLE job = nullptr;
        HANDLE mapping = nullptr;
        HANDLE request = nullptr;
        HANDLE response = nullptr;
        Protocol::Shared* shared = nullptr;
        uint32_t serial = 0;

        bool Start(const std::filesystem::path& exe, const LUID& luid)
        {
            auto name = std::wstring(L"Local\\GTAIV.EFLC.FusionFix.Upscaler.") + std::to_wstring(GetCurrentProcessId()) + L"." + std::to_wstring(GetTickCount64());

            mapping = CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0, sizeof(Protocol::Shared), (name + Protocol::MappingSuffix).c_str());
            request = CreateEventW(nullptr, FALSE, FALSE, (name + Protocol::RequestSuffix).c_str());
            response = CreateEventW(nullptr, FALSE, FALSE, (name + Protocol::ResponseSuffix).c_str());
            if (!mapping || !request || !response)
                return false;

            shared = static_cast<Protocol::Shared*>(MapViewOfFile(mapping, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(Protocol::Shared)));
            if (!shared)
                return false;

            std::memset(shared, 0, sizeof(Protocol::Shared));
            shared->Version = Protocol::Version;
            shared->GameProcessId = GetCurrentProcessId();
            shared->AdapterLuidLow = luid.LowPart;
            shared->AdapterLuidHigh = luid.HighPart;
            wcsncpy_s(shared->GameDirectory, GetExeModulePath().wstring().c_str(), _TRUNCATE);
            wcsncpy_s(shared->PluginsDirectory, exe.parent_path().wstring().c_str(), _TRUNCATE);
            wcsncpy_s(shared->LogPath, (exe.parent_path() / L"GTAIV.EFLC.FusionFix.Upscaler.log").wstring().c_str(), _TRUNCATE);

            // The helper ends with the game
            job = CreateJobObjectW(nullptr, nullptr);
            if (job)
            {
                JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
                limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
                SetInformationJobObject(job, JobObjectExtendedLimitInformation, &limits, sizeof(limits));
            }

            auto commandLine = L"\"" + exe.wstring() + L"\" " + Protocol::ArgumentName + L" " + name;
            STARTUPINFOW startup{ sizeof(startup) };
            PROCESS_INFORMATION info{};
            if (!CreateProcessW(exe.c_str(), commandLine.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW | CREATE_SUSPENDED, nullptr,
                exe.parent_path().c_str(), &startup, &info))
                return false;

            if (job)
                AssignProcessToJobObject(job, info.hProcess);
            ResumeThread(info.hThread);
            CloseHandle(info.hThread);
            process = info.hProcess;
            return true;
        }

        // Returns true once the helper answered its start
        bool PollStarted(bool& ok)
        {
            if (WaitForSingleObject(response, 0) != WAIT_OBJECT_0)
            {
                if (WaitForSingleObject(process, 0) == WAIT_OBJECT_0)
                {
                    ok = false;
                    return true;
                }
                return false;
            }
            MemoryBarrier();
            ok = shared->ResponseStatus == Protocol::Status::Ok;
            return true;
        }

        bool Request(Protocol::Command command, DWORD timeout)
        {
            shared->RequestCommand = command;
            shared->RequestSerial = ++serial;
            MemoryBarrier();
            SetEvent(request);

            auto start = GetTickCount64();
            while (true)
            {
                auto elapsed = static_cast<DWORD>(GetTickCount64() - start);
                if (elapsed >= timeout)
                    return false;

                HANDLE handles[] = { response, process };
                auto wait = WaitForMultipleObjects(2, handles, FALSE, timeout - elapsed);
                if (wait != WAIT_OBJECT_0)
                    return false;

                MemoryBarrier();
                // A late answer to an earlier request is skipped
                if (shared->ResponseSerial == serial)
                    return shared->ResponseStatus == Protocol::Status::Ok;
            }
        }

        // graceful: ask the helper to release everything first, not while the game is being unloaded
        void Stop(bool graceful)
        {
            if (graceful && shared && process && WaitForSingleObject(process, 0) == WAIT_TIMEOUT)
                Request(Protocol::Command::Shutdown, 500);
            if (process)
                CloseHandle(process);
            if (job)
                CloseHandle(job);
            process = job = nullptr;
        }
    };

    // ---------------------------------------------------------------------------------------------

    enum class State
    {
        Idle, Starting, Ready, Failed
    };

    State state = State::Idle;
    DXVKBridge dxvkBridge;
    D3D12Bridge d3d12Bridge;
    Bridge* bridge = nullptr;
    HelperProcess helper;
    std::atomic<bool> dlssAvailable = false;
    std::atomic<bool> fsrAvailable = false;
    std::atomic<uint32_t> generation = 0;

    uint32_t configuredBackend = 0;
    uint32_t configuredWidth = 0;
    uint32_t configuredHeight = 0;
    uint32_t configuredPreset = 0;
    uint32_t configuredFlags = 0;
    bool configureFailed = false;
    uint64_t fenceValue = 0;

    std::filesystem::path HelperPath()
    {
        return GetThisModulePath() / L"GTAIV.EFLC.FusionFix.exe";
    }

    bool FidelityFXPresent()
    {
        for (auto dir : { GetThisModulePath(), GetExeModulePath() })
            for (auto name : { L"amd_fidelityfx_loader_dx12.dll", L"amd_fidelityfx_dx12.dll" })
                if (std::filesystem::exists(dir / name))
                    return true;
        return false;
    }

    void Fail()
    {
        state = State::Failed;
        dlssAvailable = false;
        fsrAvailable = false;
        ++generation;
        helper.Stop(true);
    }
}

export namespace Upscaler
{
    enum class Backend : uint32_t
    {
        DLSS = static_cast<uint32_t>(Protocol::Backend::DLSS),
        FSR = static_cast<uint32_t>(Protocol::Backend::FSR),
    };

    struct Frame
    {
        IDirect3DTexture9* Color = nullptr;
        IDirect3DTexture9* Depth = nullptr;   // R32F, standard [0, 1] depth
        IDirect3DTexture9* Motion = nullptr;  // G16R16F, previous - current in texture coordinates
        IDirect3DTexture9* Reactive = nullptr; // R16F, optional
        IDirect3DTexture9* Output = nullptr;  // A16B16G16R16F
        uint32_t Width = 0;
        uint32_t Height = 0;
        float JitterX = 0.0f;
        float JitterY = 0.0f;
        float CameraNear = 0.1f;
        float CameraFar = 1000.0f;
        float CameraFovY = 1.0f;
        float FrameTimeMs = 16.6f;
        float Sharpness = 0.0f;
        uint32_t DLSSPreset = 0;
        bool Reset = false;
    };

    bool IsAvailable(Backend backend)
    {
        return backend == Backend::DLSS ? dlssAvailable.load() : fsrAvailable.load();
    }

    // Changes whenever the availability does
    uint32_t Generation()
    {
        return generation.load();
    }

    // Render thread, every frame: starts the helper once the device is known
    void Update()
    {
        if (state == State::Idle)
        {
            auto device = RageDirect3DDevice9::m_pRealDevice ? *RageDirect3DDevice9::m_pRealDevice : nullptr;
            if (!device)
                return;

            state = State::Failed;
            if (!std::filesystem::exists(HelperPath()))
                return;

            // DXVK or D3D9on12: the D3D9 runtime itself can't share its textures with D3D12
            if (dxvkBridge.Init(device))
                bridge = &dxvkBridge;
            else if (d3d12Bridge.Init(device))
                bridge = &d3d12Bridge;
            else
                return;

            // Nothing to offer: neither an NVIDIA GPU nor AMD's FidelityFX runtime
            if (bridge->vendorId != 0x10DE && !FidelityFXPresent())
                return;

            if (!helper.Start(HelperPath(), bridge->luid))
            {
                helper.Stop(false);
                return;
            }
            state = State::Starting;
        }

        if (state == State::Starting)
        {
            bool ok = false;
            if (!helper.PollStarted(ok))
                return;
            if (!ok)
            {
                Fail();
                return;
            }
            dlssAvailable = helper.shared->DLSSAvailable != 0;
            fsrAvailable = helper.shared->FSRAvailable != 0;
            state = State::Ready;
            ++generation;
        }
    }

    // Render thread: upscales Frame.Color into Frame.Output, false leaves Output untouched
    bool Evaluate(Backend backend, const Frame& frame)
    {
        if (state != State::Ready || !IsAvailable(backend) || !frame.Color || !frame.Depth || !frame.Motion || !frame.Output)
            return false;

        auto backendId = static_cast<uint32_t>(backend);
        auto flags = frame.Reactive ? Protocol::ConfigureFlags::ReactiveMask : 0u;
        bool reconfigure = configuredBackend != backendId || configuredWidth != frame.Width || configuredHeight != frame.Height ||
            configuredPreset != frame.DLSSPreset || configuredFlags != flags;
        if (reconfigure)
        {
            bridge->ReleaseImports();
            configuredBackend = backendId;
            configuredWidth = frame.Width;
            configuredHeight = frame.Height;
            configuredPreset = frame.DLSSPreset;
            configuredFlags = flags;
            configureFailed = true;

            auto& shared = *helper.shared;
            shared.ConfigureBackend = static_cast<Protocol::Backend>(backendId);
            shared.Width = frame.Width;
            shared.Height = frame.Height;
            shared.DLSSPreset = frame.DLSSPreset;
            shared.Flags = flags;
            if (!helper.Request(Protocol::Command::Configure, 15000))
            {
                if (WaitForSingleObject(helper.process, 0) == WAIT_OBJECT_0)
                    Fail();
                return false;
            }
            if (!bridge->Import(shared, frame.Width, frame.Height))
                return false;

            configureFailed = false;
            fenceValue = 0;
        }
        else if (configureFailed)
        {
            // A failed configuration is not retried every frame, only when something changes
            return false;
        }

        auto inputValue = ++fenceValue;
        auto outputValue = ++fenceValue;

        IDirect3DTexture9* const inputs[InputCount] = { frame.Color, frame.Depth, frame.Motion, frame.Reactive };
        if (!bridge->SubmitInputs(inputs, inputValue))
        {
            // Nothing was submitted, the helper was not asked for this frame. Retried when the mode or size changes.
            configureFailed = true;
            return false;
        }

        auto& shared = *helper.shared;
        shared.WaitValue = inputValue;
        shared.SignalValue = outputValue;
        shared.JitterX = frame.JitterX;
        shared.JitterY = frame.JitterY;
        shared.MotionScaleX = static_cast<float>(frame.Width);
        shared.MotionScaleY = static_cast<float>(frame.Height);
        shared.CameraNear = frame.CameraNear;
        shared.CameraFar = frame.CameraFar;
        shared.CameraFovY = frame.CameraFovY;
        shared.FrameTimeMs = frame.FrameTimeMs;
        shared.Sharpness = frame.Sharpness;
        shared.Reset = frame.Reset ? 1 : 0;

        // The helper answers once its GPU work, which signals outputValue, has been submitted
        if (!helper.Request(Protocol::Command::Evaluate, 500))
        {
            if (WaitForSingleObject(helper.process, 0) == WAIT_OBJECT_0)
                Fail();
            else
                configureFailed = true;
            return false;
        }

        return bridge->SubmitOutput(frame.Output, outputValue);
    }

    // The job object ends the helper with the game, and the helper also watches the game process
    void Shutdown()
    {
        helper.Stop(false);
    }
}
