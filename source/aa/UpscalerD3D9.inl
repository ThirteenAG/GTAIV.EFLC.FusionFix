// DLAA (NVIDIA DLSS at the render size) and AMD FSR native anti-aliasing for Direct3D 9 games, run by FusionFixHub.exe
// (x64) in the game's folder through the hub's upscaler bridge (upscaler_bridge_protocol.h, upscaler_bridge_client.h).
// Included by Upscaler.hpp after the bridge options and logging helpers.
//
// The hub creates shared D3D12 textures and a shared fence. The plugin copies the frame's color, depth, motion vectors
// and reactive mask into them on the GPU, the hub waits for that on its queue, runs the upscaler and signals the fence,
// and the plugin copies the result back, all without the CPU waiting for the GPU. The copies run where the game
// renders:
// - DXVK: the shared textures and the fence are imported into DXVK's Vulkan device, the copies are submitted to DXVK's
//   queue.
// - D3D9on12 (Graphics API "DirectX 12"): the shared textures and the fence are opened on the D3D12 device of
//   D3D9on12. With the optional D3D9On12 queued-interop interface the copies and the fence signal and wait are queued
//   into the driver's own command stream (no drain of the render thread per frame, DXVK-like cost); the hub's answer
//   is collected the frame after. On Windows' driver, or an older build, the game's textures are unwrapped to their
//   D3D12 resources and copied on a queue of the plugin (only when Options::D3D9On12Interop allows it: it's slow).

#include <d3d12.h>
#include <d3d9on12.h>
#include <deque>
#include <unordered_map>

namespace Antialiasing::detail
{
    namespace Protocol = UpscalerBridge;

    inline constexpr size_t TextureCount = static_cast<size_t>(Protocol::Texture::Count);
    inline constexpr size_t InputCount = Protocol::InputCount;
    inline constexpr size_t OutputIndex = static_cast<size_t>(Protocol::Texture::Output);

    // The hub duplicated its handles into this process, they are closed once imported or not
    inline void CloseSharedHandles(Protocol::Shared& shared)
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
        // The output copy waits for the hub on the GPU: the render thread needn't wait for the hub's answer
        virtual bool AsynchronousEvaluate() const { return false; }
        // Lets a GPU wait on the hub's fence pass when the hub failed to signal it
        virtual void ReleaseWait(uint64_t) {}
        // Everything submitted so far has been executed (before a device reset)
        virtual void Flush() {}
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

    inline bool GetGameImage(IDirect3DTexture9* texture, GameImage& out)
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

    inline VkImageMemoryBarrier ImageBarrier(VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout, VkAccessFlags srcAccess, VkAccessFlags dstAccess,
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

    inline VkImageCopy FullCopy(uint32_t width, uint32_t height)
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
            // VkPhysicalDeviceProperties::vendorID (the minimal Vulkan header keeps the properties opaque)
            std::memcpy(&vendorId, properties.properties + 8, sizeof(vendorId));

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

    inline DXGI_FORMAT TypelessFormat(DXGI_FORMAT format)
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

    inline D3D12_RESOURCE_BARRIER Transition(ID3D12Resource* resource, D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after)
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

        // Queued interop (Fusion Fix's D3D9On12 build): a request gets a token, a marker render state puts it into
        // the D3D9 command stream, the driver copies and signals or waits there. Game textures are unwrapped once,
        // which registers them with the driver, and remembered.
        PFN_D3D9On12QueueInterop queueInterop = nullptr;
        std::unordered_map<IDirect3DTexture9*, ID3D12Resource*> unwrapped;
        uint64_t queuedFenceValue = 0;          // the highest fence value a queued request signals or waits for
        bool queuedWork = false;                // requests queued since the last Drain

        // The loaded driver's queued interop, when it's Fusion Fix's build with that capability (graphics_api.h)
        static PFN_D3D9On12QueueInterop QueuedInterop()
        {
            return FusionFixGraphicsApi::QueuedInterop();
        }

        bool Queued() const { return queueInterop != nullptr; }

        ID3D12Resource* Registered(IDirect3DTexture9* texture)
        {
            if (auto found = unwrapped.find(texture); found != unwrapped.end())
                return found->second;
            ID3D12Resource* resource = nullptr;
            if (FAILED(on12->UnwrapUnderlyingResource(texture, queue, IID_PPV_ARGS(&resource))) || !resource)
                return nullptr;
            on12->ReturnUnderlyingResource(texture, 0, nullptr, nullptr);
            unwrapped[texture] = resource;      // the reference Unwrap gave is kept
            return resource;
        }

        void ForgetUnwrapped()
        {
            for (auto& [texture, resource] : unwrapped)
                resource->Release();
            unwrapped.clear();
        }

        // Before anything a queued request refers to is released (the shared images, the fence, an unwrapped game
        // texture): the requests still in the D3D9 command stream are executed and done. The game's work is flushed
        // and finished through an event query; a wait on the hub's fence passes once the hub signalled it, or is let
        // through after a timeout (the hub failed).
        void Drain()
        {
            if (!queuedWork)
                return;
            queuedWork = false;
            // The requests are submitted first: the hub's work waits for the game's copies, which the driver may still
            // hold in commands it hasn't submitted, and nothing else would submit them while this thread waits
            IDirect3DQuery9* query = nullptr;
            if (SUCCEEDED(device9->CreateQuery(D3DQUERYTYPE_EVENT, &query)) && query)
            {
                query->Issue(D3DISSUE_END);
                query->GetData(nullptr, 0, D3DGETDATA_FLUSH);
            }
            if (sharedFence && sharedFence->GetCompletedValue() < queuedFenceValue)
            {
                if (SUCCEEDED(sharedFence->SetEventOnCompletion(queuedFenceValue, event)) && WaitForSingleObject(event, 2000) != WAIT_OBJECT_0)
                {
                    Log("Upscaler: the hub's fence didn't reach %llu in 2000 ms (at %llu), released", queuedFenceValue,
                        sharedFence->GetCompletedValue());
                    sharedFence->Signal(queuedFenceValue);
                }
            }
            if (query)
            {
                auto start = GetTickCount64();
                while (query->GetData(nullptr, 0, D3DGETDATA_FLUSH) == S_FALSE && GetTickCount64() - start < 2000)
                    Sleep(0);
                query->Release();
            }
        }

        // A GPU wait for the hub's fence that isn't signalled in time (the hub failed, or its work waits for something
        // that never comes) is let through from a thread of its own: the render thread may be the one blocked, in a
        // present that waits for that GPU work, and would never get to release it itself. The output value follows the
        // input value the game's copies signal: the hub gets HubTimeoutMs from then on; a frame whose copies never
        // signal (a request lost on the game's side) is released after LostTimeoutMs.
        struct Watched
        {
            ID3D12Fence* fence = nullptr;
            uint64_t value = 0;
            ULONGLONG since = 0;
            ULONGLONG inputSince = 0;
        };
        std::mutex watchLock;
        std::deque<Watched> watched;
        HANDLE watchWake = nullptr;
        static constexpr ULONGLONG HubTimeoutMs = 500;
        static constexpr ULONGLONG LostTimeoutMs = 3000;

        void Watch(uint64_t value)
        {
            if (!sharedFence)
                return;
            std::lock_guard<std::mutex> lock(watchLock);
            if (!watchWake)
            {
                watchWake = CreateEventW(nullptr, FALSE, FALSE, nullptr);
                if (!watchWake)
                    return;
                auto thread = CreateThread(nullptr, 0, [](void* self) -> DWORD
                {
                    static_cast<D3D12Bridge*>(self)->WatchLoop();
                    return 0;
                }, this, 0, nullptr);
                if (thread)
                    CloseHandle(thread);
            }
            sharedFence->AddRef();
            watched.push_back({ sharedFence, value, GetTickCount64() });
            SetEvent(watchWake);
        }

        void WatchLoop()
        {
            while (true)
            {
                bool waiting = false;
                {
                    std::lock_guard<std::mutex> lock(watchLock);
                    while (!watched.empty())
                    {
                        auto& w = watched.front();
                        auto completed = w.fence->GetCompletedValue();
                        if (completed < w.value)
                        {
                            auto now = GetTickCount64();
                            if (completed + 1 >= w.value && !w.inputSince)
                                w.inputSince = now;
                            bool hubLate = w.inputSince && now - w.inputSince >= HubTimeoutMs;
                            bool lost = now - w.since >= LostTimeoutMs;
                            if (!hubLate && !lost)
                            {
                                waiting = true;
                                break;
                            }
                            Log(hubLate ? "Upscaler: the hub's fence didn't reach %llu %llu ms after the game's copies (at %llu), the game's wait released" :
                                "Upscaler: the hub's fence didn't reach %llu in %llu ms, the game's copies never signalled (at %llu), the game's wait released",
                                w.value, hubLate ? now - w.inputSince : now - w.since, completed);
                            w.fence->Signal(w.value);
                        }
                        w.fence->Release();
                        watched.pop_front();
                    }
                }
                WaitForSingleObject(watchWake, waiting ? 20 : INFINITE);
            }
        }

        // Once the imports go (after a drain): nothing of them is waited for any more
        void Unwatch()
        {
            std::lock_guard<std::mutex> lock(watchLock);
            for (auto& w : watched)
                w.fence->Release();
            watched.clear();
        }

        // A copy of each game texture to or from its shared image at this point of the D3D9 command stream, with the
        // fence signal (export) or wait (import) at fenceValue
        bool Queue(uint32_t operation, IDirect3DTexture9* const* textures, ID3D12Resource* const* foreign, uint32_t count, uint64_t fenceValue)
        {
            ID3D12Resource* game[InputCount]{};
            ID3D12Resource* shared[InputCount]{};
            uint32_t pairs = 0;
            auto collect = [&]
            {
                pairs = 0;
                for (uint32_t i = 0; i < count; ++i)
                {
                    if (!textures[i] || !foreign[i])
                        continue;
                    auto resource = Registered(textures[i]);
                    if (!resource || !Matches(resource, foreign[i]))
                        return false;
                    game[pairs] = resource;
                    shared[pairs++] = foreign[i];
                }
                return pairs > 0;
            };
            if (!collect())
                return false;
            D3D9ON12_INTEROP_REQUEST request{ sizeof(request), operation, device, pairs, 0, game, shared, sharedFence, fenceValue };
            uint32_t token = 0;
            auto hr = queueInterop(&request, &token);
            if (hr == E_FAIL)
            {
                // A game texture was created anew at an address seen before: unwrap again, once
                Drain();
                ForgetUnwrapped();
                if (!collect())
                    return false;
                request.Count = pairs;
                hr = queueInterop(&request, &token);
            }
            if (FAILED(hr))
                return false;
            queuedWork = true;
            queuedFenceValue = (std::max)(queuedFenceValue, fenceValue);
            return SUCCEEDED(device9->SetRenderState(static_cast<D3DRENDERSTATETYPE>(D3D9ON12_INTEROP_MARKER_RENDERSTATE), token));
        }

        bool Init(IDirect3DDevice9* realDevice)
        {
            if (FAILED(realDevice->QueryInterface(__uuidof(IDirect3DDevice9On12), reinterpret_cast<void**>(&on12))) || !on12)
                return false;
            if (FAILED(on12->GetD3D12Device(IID_PPV_ARGS(&device))) || !device)
                return false;
            queueInterop = QueuedInterop();

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

        bool AsynchronousEvaluate() const override { return Queued(); }

        void ReleaseWait(uint64_t value) override
        {
            if (sharedFence && sharedFence->GetCompletedValue() < value)
                sharedFence->Signal(value);
        }

        // The device is destroyed or reset (the game's mode change): nothing queued may be left behind
        void Flush() override { Drain(); }

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
            Drain();
            Unwatch();
            ForgetUnwrapped();
            queuedFenceValue = 0;
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
            if (Queued())
                return Queue(D3D9ON12_INTEROP_EXPORT_COPY, inputs, images.data(), InputCount, signalValue);

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
            if (Queued())
            {
                IDirect3DTexture9* const targets[1] = { target };
                ID3D12Resource* const foreign[1] = { images[OutputIndex] };
                if (!Queue(D3D9ON12_INTEROP_IMPORT_COPY, targets, foreign, 1, waitValue))
                    return false;
                Watch(waitValue);
                return true;
            }

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


    // ---------------------------------------------------------------------------------------------------------------
    // The hub: started once the device is known, then Configure and Evaluate per frame

    struct UpscalerFrame
    {
        IDirect3DTexture9* Color = nullptr;     // A16B16G16R16F
        IDirect3DTexture9* Depth = nullptr;     // R32F, standard [0, 1] depth
        IDirect3DTexture9* Motion = nullptr;    // G16R16F, previous - current in texture coordinates
        IDirect3DTexture9* Reactive = nullptr;  // R16F, optional
        IDirect3DTexture9* Output = nullptr;    // A16B16G16R16F
        uint32_t Width = 0;
        uint32_t Height = 0;
        Jitter JitterPixels;
        float CameraNear = 0.1f;
        float CameraFar = 1000.0f;
        float CameraFovY = 1.0f;
        float FrameTimeMs = 16.6f;
        float Sharpness = 0.0f;
        uint32_t DLSSPreset = 0;
        bool Reset = false;
    };

    class Upscaler
    {
    public:

        enum class State
        {
            Idle, Starting, Ready, Failed
        };

        State state = State::Idle;
        DXVKBridge dxvkBridge;
        D3D12Bridge d3d12Bridge;
        Bridge* bridge = nullptr;
        Protocol::Client hub;
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
        bool pendingEvaluate = false;           // an Evaluate was posted, its answer is collected the frame after
        uint64_t pendingOutputValue = 0;        // the fence value that frame's output copy waits for

        // D3D9on12 as the interop path, when the game runs on DirectX 12 (Fusion Fix's driver): with the driver's queued
        // interop always, without it only when asked for (Options::D3D9On12Interop)
        bool D3D9On12Usable()
        {
            return FusionFixGraphicsApi::Running() == FusionFixGraphicsApi::Api::D3D9On12 && (Config().D3D9On12Interop || D3D12Bridge::QueuedInterop() != nullptr);
        }

        void Fail()
        {
            state = State::Failed;
            dlssAvailable = false;
            fsrAvailable = false;
            ++generation;
            hub.Stop(true);
        }

        // The hub's answer to the frame before normally is in at once: slow answers are logged (graphics log), at most
        // once a second with the number since the last line
        static constexpr double SlowAnswerMs = 20.0;
        uint32_t slowAnswers = 0;
        double slowestAnswerMs = 0.0;
        ULONGLONG lastSlowAnswerLog = 0;

        static double NowMs()
        {
            static const double frequency = []
            {
                LARGE_INTEGER f{};
                QueryPerformanceFrequency(&f);
                return static_cast<double>(f.QuadPart);
            }();
            LARGE_INTEGER counter{};
            QueryPerformanceCounter(&counter);
            return static_cast<double>(counter.QuadPart) * 1000.0 / frequency;
        }

        void NoteAnswerTime(double ms, bool answered)
        {
            if (answered && ms < SlowAnswerMs)
                return;
            ++slowAnswers;
            slowestAnswerMs = (std::max)(slowestAnswerMs, ms);
            auto now = GetTickCount64();
            if (!answered || now - lastSlowAnswerLog >= 1000)
            {
                if (answered)
                    Log("Upscaler: slow answers from the hub: %u since the last line, the slowest %.0f ms", slowAnswers, slowestAnswerMs);
                else
                    Log("Upscaler: no answer from the hub in %.0f ms; anti-aliasing is off until the mode or size changes", ms);
                lastSlowAnswerLog = now;
                slowAnswers = 0;
                slowestAnswerMs = 0.0;
            }
        }

        // The game runs on DXVK (or D3D9on12 with an interop path), and FusionFixHub.exe and a vendor runtime the GPU can
        // use are in the game folder. DLAA and FSR are offered once the hub reports them usable (IsAvailable), which needs
        // the device; this tells whether that can happen at all, e.g. for a hint that the hub's download is missing.
        bool BridgeAvailable(IDirect3DDevice9* device)
        {
            bool dxvk = FusionFixGraphicsApi::Running() == FusionFixGraphicsApi::Api::Vulkan;
            return (dxvk || D3D9On12Usable()) && Protocol::Present(Protocol::GameRoot(), !device || AdapterVendor(device) == 0x10DE);
        }

        bool IsAvailable(Backend backend)
        {
            return backend == Backend::DLSS ? dlssAvailable.load() : fsrAvailable.load();
        }

        // Changes whenever the availability does
        uint32_t Generation()
        {
            return generation.load();
        }

        // Render thread, every frame: starts the hub once the device is known
        void Update(IDirect3DDevice9* device)
        {
            if (state == State::Idle)
            {
                if (!device)
                    return;

                state = State::Failed;

                // DXVK: the D3D9 runtime itself can't share its textures with D3D12. D3D9on12 can: the queued-interop interface at
                // DXVK-like cost, Windows' slowly, so that one only when asked for (D3D9on12Interop).
                if (dxvkBridge.Init(device))
                    bridge = &dxvkBridge;
                else if (D3D9On12Usable() && d3d12Bridge.Init(device))
                    bridge = &d3d12Bridge;
                else
                {
                    Log("Bridge: no interop path (the game runs neither on DXVK nor on D3D9on12 with one), DLAA and FSR are off");
                    return;
                }

                // Nothing to offer: no FusionFixHub.exe, or no runtime for this GPU (DLSS needs an NVIDIA GPU)
                auto root = Protocol::GameRoot();
                if (!Protocol::Present(root, bridge->vendorId == 0x10DE))
                {
                    Log("Bridge: FusionFixHub.exe or the vendor runtimes in the game's folder are missing");
                    return;
                }

                auto& config = Config();
                if (!hub.Start(root, bridge->luid, config.BridgeName, config.ClientName.c_str(), config.HubLog))
                {
                    Log("Bridge: FusionFixHub.exe didn't start (%u)", GetLastError());
                    hub.Stop(false);
                    return;
                }
                Log("Bridge: FusionFixHub.exe started");
                state = State::Starting;
            }

            if (state == State::Starting)
            {
                bool ok = false;
                if (!hub.PollStarted(ok))
                    return;
                if (!ok)
                {
                    Log("Bridge: %s, DLAA and FSR are off", hub.shared && hub.shared->Message[0] ? hub.shared->Message : "the hub didn't start");
                    Fail();
                    return;
                }
                dlssAvailable = hub.IsAvailable(Protocol::Backend::DLSS);
                fsrAvailable = hub.IsAvailable(Protocol::Backend::FSR);
                Log("Bridge: ready, DLSS %s, FSR %s (%s)", dlssAvailable ? "yes" : "no", fsrAvailable ? "yes" : "no", hub.shared->Message);
                state = State::Ready;
                ++generation;
            }
        }

        // Render thread: upscales Frame.Color into Frame.Output, false leaves Output untouched
        bool Evaluate(Backend backend, const UpscalerFrame& frame)
        {
            if (state != State::Ready || !IsAvailable(backend) || !frame.Color || !frame.Depth || !frame.Motion || !frame.Output)
                return false;

            // An asynchronous bridge: the answer to the frame before comes first (normally long in). A missing or failed
            // answer leaves the GPU waiting on the hub's fence for that frame's output: the wait is let through.
            if (pendingEvaluate)
            {
                bool ok = false;
                auto start = NowMs();
                auto answered = hub.PollResponse(ok, 500);
                NoteAnswerTime(NowMs() - start, answered);
                pendingEvaluate = false;
                if (!answered || !ok)
                {
                    if (answered)
                        Log("Upscaler: the hub failed the frame (fence value %llu); anti-aliasing is off until the mode or size changes",
                            pendingOutputValue);
                    bridge->ReleaseWait(pendingOutputValue);
                    if (hub.Exited())
                        Fail();
                    else
                        configureFailed = true;
                    return false;
                }
            }

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

                auto& shared = *hub.shared;
                shared.ConfigureBackend = static_cast<Protocol::Backend>(backendId);
                shared.Width = frame.Width;
                shared.Height = frame.Height;
                // DLAA: the output has the render size
                shared.OutputWidth = frame.Width;
                shared.OutputHeight = frame.Height;
                shared.DLSSPreset = frame.DLSSPreset;
                shared.Flags = flags;
                auto start = NowMs();
                if (!hub.Request(Protocol::Command::Configure, 15000))
                {
                    Log("Upscaler: configuration at %ux%u failed after %.0f ms", frame.Width, frame.Height, NowMs() - start);
                    if (hub.Exited())
                        Fail();
                    return false;
                }
                if (!bridge->Import(shared, frame.Width, frame.Height))
                {
                    Log("Upscaler: the shared images at %ux%u could not be opened", frame.Width, frame.Height);
                    return false;
                }
                Log("Upscaler: configured at %ux%u in %.0f ms", frame.Width, frame.Height, NowMs() - start);

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
                // Nothing was submitted, the hub was not asked for this frame. Retried when the mode or size changes.
                configureFailed = true;
                return false;
            }

            auto& shared = *hub.shared;
            shared.WaitValue = inputValue;
            shared.SignalValue = outputValue;
            shared.JitterX = frame.JitterPixels.X;
            shared.JitterY = frame.JitterPixels.Y;
            shared.MotionScaleX = static_cast<float>(frame.Width);
            shared.MotionScaleY = static_cast<float>(frame.Height);
            shared.CameraNear = frame.CameraNear;
            shared.CameraFar = frame.CameraFar;
            shared.CameraFovY = frame.CameraFovY;
            shared.FrameTimeMs = frame.FrameTimeMs;
            shared.Sharpness = frame.Sharpness;
            // Reconfiguration creates a different temporal history, even without a camera cut.
            shared.Reset = (frame.Reset || reconfigure) ? 1 : 0;

            if (bridge->AsynchronousEvaluate())
            {
                // The output copy waits for outputValue on the GPU, the hub's answer is collected the frame after
                hub.Post(Protocol::Command::Evaluate);
                pendingEvaluate = true;
                pendingOutputValue = outputValue;
                return bridge->SubmitOutput(frame.Output, outputValue);
            }

            // The hub answers once its GPU work, which signals outputValue, has been submitted
            if (!hub.Request(Protocol::Command::Evaluate, 500))
            {
                if (hub.Exited())
                    Fail();
                else
                    configureFailed = true;
                return false;
            }

            return bridge->SubmitOutput(frame.Output, outputValue);
        }

        // Before the game resets its device (a mode change): what the bridge queued has been executed
        void BeforeReset()
        {
            if (bridge)
                bridge->Flush();
        }

        // The job object ends the hub with the game, and the hub also watches the game process
        void Shutdown()
        {
            hub.Stop(false);
        }
    };

    inline Upscaler& TheUpscaler()
    {
        static Upscaler upscaler;
        return upscaler;
    }
}
