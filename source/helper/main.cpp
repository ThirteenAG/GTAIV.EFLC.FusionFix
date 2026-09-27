// GTAIV.EFLC.FusionFix.exe
//
// 64-bit helper of the plugin: NVIDIA DLSS (NGX) and AMD FSR only exist for 64-bit processes. It runs on
// its own D3D12 device, on the adapter the game renders with, and exchanges the frames with the game
// through shared textures and a shared fence. See upscaler_protocol.hpp for the protocol.
//
// This software contains source code provided by NVIDIA Corporation.

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <shellapi.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>

#include <nvsdk_ngx.h>
#include <nvsdk_ngx_helpers.h>

#include <ffx_api.h>
#include <ffx_api_loader.h>
#include <ffx_upscale.h>
#include <dx12/ffx_api_dx12.h>

#include "upscaler_protocol.hpp"

using Microsoft::WRL::ComPtr;
namespace Protocol = UpscalerProtocol;

namespace
{
    // -----------------------------------------------------------------------------------------------
    // Log

    FILE* gLog = nullptr;

    void Log(const char* format, ...)
    {
        if (!gLog)
            return;

        SYSTEMTIME time;
        GetLocalTime(&time);
        fprintf(gLog, "%02d:%02d:%02d.%03d ", time.wHour, time.wMinute, time.wSecond, time.wMilliseconds);

        va_list args;
        va_start(args, format);
        vfprintf(gLog, format, args);
        va_end(args);

        fputc('\n', gLog);
        fflush(gLog);
    }

    // -----------------------------------------------------------------------------------------------
    // Connection to the game

    struct Connection
    {
        HANDLE mapping = nullptr;
        HANDLE request = nullptr;
        HANDLE response = nullptr;
        HANDLE game = nullptr;
        Protocol::Shared* shared = nullptr;

        bool Open(const std::wstring& name)
        {
            mapping = OpenFileMappingW(FILE_MAP_ALL_ACCESS, FALSE, (name + Protocol::MappingSuffix).c_str());
            request = OpenEventW(SYNCHRONIZE | EVENT_MODIFY_STATE, FALSE, (name + Protocol::RequestSuffix).c_str());
            response = OpenEventW(SYNCHRONIZE | EVENT_MODIFY_STATE, FALSE, (name + Protocol::ResponseSuffix).c_str());
            if (!mapping || !request || !response)
                return false;

            shared = static_cast<Protocol::Shared*>(MapViewOfFile(mapping, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(Protocol::Shared)));
            if (!shared || shared->Version != Protocol::Version)
                return false;

            game = OpenProcess(SYNCHRONIZE | PROCESS_DUP_HANDLE, FALSE, shared->GameProcessId);
            return game != nullptr;
        }

        void Respond(Protocol::Status status)
        {
            shared->ResponseStatus = status;
            MemoryBarrier();
            shared->ResponseSerial = shared->RequestSerial;
            MemoryBarrier();
            SetEvent(response);
        }

        void Message(const char* format, ...)
        {
            va_list args;
            va_start(args, format);
            vsnprintf(shared->Message, sizeof(shared->Message), format, args);
            va_end(args);
            Log("%s", shared->Message);
        }
    };

    // -----------------------------------------------------------------------------------------------
    // D3D12

    struct SharedTexture
    {
        ComPtr<ID3D12Resource> resource;
        HANDLE handle = nullptr;
    };

    class Device
    {
    public:
        ComPtr<IDXGIAdapter1> adapter;
        ComPtr<ID3D12Device> device;
        ComPtr<ID3D12CommandQueue> queue;
        ComPtr<ID3D12Fence> sharedFence;
        ComPtr<ID3D12Fence> localFence;
        uint64_t localValue = 0;
        HANDLE fenceEvent = nullptr;

        static constexpr uint32_t Frames = 3;
        std::array<ComPtr<ID3D12CommandAllocator>, Frames> allocators;
        std::array<uint64_t, Frames> allocatorValues{};
        ComPtr<ID3D12GraphicsCommandList> list;
        uint32_t frame = 0;

        std::array<SharedTexture, static_cast<size_t>(Protocol::Texture::Count)> textures;
        HANDLE sharedFenceHandle = nullptr;

        bool Create(LUID luid)
        {
            ComPtr<IDXGIFactory4> factory;
            if (FAILED(CreateDXGIFactory2(0, IID_PPV_ARGS(&factory))))
                return false;
            if (FAILED(factory->EnumAdapterByLuid(luid, IID_PPV_ARGS(&adapter))))
                return false;
            if (FAILED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&device))))
                return false;

            D3D12_COMMAND_QUEUE_DESC queueDesc{};
            queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
            if (FAILED(device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&queue))))
                return false;

            for (auto& allocator : allocators)
                if (FAILED(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&allocator))))
                    return false;

            if (FAILED(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, allocators[0].Get(), nullptr, IID_PPV_ARGS(&list))))
                return false;
            list->Close();

            if (FAILED(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&localFence))))
                return false;

            fenceEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);
            return fenceEvent != nullptr;
        }

        bool WaitFence(ID3D12Fence* fence, uint64_t value, DWORD timeout)
        {
            if (fence->GetCompletedValue() >= value)
                return true;
            fence->SetEventOnCompletion(value, fenceEvent);
            return WaitForSingleObject(fenceEvent, timeout) == WAIT_OBJECT_0;
        }

        // Command list for immediate work, e.g. feature creation, waited for on the CPU
        ID3D12GraphicsCommandList* BeginImmediate()
        {
            WaitFence(localFence.Get(), localValue, 5000);
            allocators[0]->Reset();
            list->Reset(allocators[0].Get(), nullptr);
            return list.Get();
        }

        bool SubmitImmediate()
        {
            if (FAILED(list->Close()))
                return false;
            ID3D12CommandList* lists[] = { list.Get() };
            queue->ExecuteCommandLists(1, lists);
            queue->Signal(localFence.Get(), ++localValue);
            return WaitFence(localFence.Get(), localValue, 10000);
        }

        // Command list of one frame; allocators are reused once the GPU finished with them
        ID3D12GraphicsCommandList* BeginFrame()
        {
            frame = (frame + 1) % Frames;
            if (sharedFence)
                WaitFence(sharedFence.Get(), allocatorValues[frame], 1000);
            allocators[frame]->Reset();
            list->Reset(allocators[frame].Get(), nullptr);
            return list.Get();
        }

        void SubmitFrame(uint64_t waitValue, uint64_t signalValue, bool execute)
        {
            list->Close();
            queue->Wait(sharedFence.Get(), waitValue);
            if (execute)
            {
                ID3D12CommandList* lists[] = { list.Get() };
                queue->ExecuteCommandLists(1, lists);
            }
            queue->Signal(sharedFence.Get(), signalValue);
            allocatorValues[frame] = signalValue;
        }

        void ReleaseTextures()
        {
            // The game has already imported or dropped its duplicates
            if (sharedFence)
                WaitFence(sharedFence.Get(), *std::max_element(allocatorValues.begin(), allocatorValues.end()), 1000);
            for (auto& texture : textures)
            {
                if (texture.handle)
                    CloseHandle(texture.handle);
                texture = {};
            }
            if (sharedFenceHandle)
                CloseHandle(sharedFenceHandle);
            sharedFenceHandle = nullptr;
            sharedFence.Reset();
            allocatorValues.fill(0);
        }

        bool CreateTexture(Protocol::Texture index, uint32_t width, uint32_t height, DXGI_FORMAT format, bool unorderedAccess)
        {
            D3D12_HEAP_PROPERTIES heap{};
            heap.Type = D3D12_HEAP_TYPE_DEFAULT;

            D3D12_RESOURCE_DESC desc{};
            desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
            desc.Width = width;
            desc.Height = height;
            desc.DepthOrArraySize = 1;
            desc.MipLevels = 1;
            desc.Format = format;
            desc.SampleDesc.Count = 1;
            desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
            desc.Flags = unorderedAccess ? D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS : D3D12_RESOURCE_FLAG_NONE;

            auto& texture = textures[static_cast<size_t>(index)];
            if (FAILED(device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_SHARED, &desc, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&texture.resource))))
                return false;
            return SUCCEEDED(device->CreateSharedHandle(texture.resource.Get(), nullptr, GENERIC_ALL, nullptr, &texture.handle));
        }

        uint64_t AllocationSize(Protocol::Texture index)
        {
            auto desc = textures[static_cast<size_t>(index)].resource->GetDesc();
            return device->GetResourceAllocationInfo(0, 1, &desc).SizeInBytes;
        }

        bool CreateSharedFence()
        {
            if (FAILED(device->CreateFence(0, D3D12_FENCE_FLAG_SHARED, IID_PPV_ARGS(&sharedFence))))
                return false;
            return SUCCEEDED(device->CreateSharedHandle(sharedFence.Get(), nullptr, GENERIC_ALL, nullptr, &sharedFenceHandle));
        }

        ID3D12Resource* Texture(Protocol::Texture index)
        {
            return textures[static_cast<size_t>(index)].resource.Get();
        }

        void Transition(ID3D12GraphicsCommandList* cmd, bool toUse)
        {
            D3D12_RESOURCE_BARRIER barriers[static_cast<size_t>(Protocol::Texture::Count)]{};
            for (size_t i = 0; i < std::size(barriers); ++i)
            {
                auto output = i == static_cast<size_t>(Protocol::Texture::Output);
                auto use = output ? D3D12_RESOURCE_STATE_UNORDERED_ACCESS : D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
                barriers[i].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
                barriers[i].Transition.pResource = textures[i].resource.Get();
                barriers[i].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
                barriers[i].Transition.StateBefore = toUse ? D3D12_RESOURCE_STATE_COMMON : use;
                barriers[i].Transition.StateAfter = toUse ? use : D3D12_RESOURCE_STATE_COMMON;
            }
            cmd->ResourceBarrier(static_cast<UINT>(std::size(barriers)), barriers);
        }
    };

    // -----------------------------------------------------------------------------------------------
    // Frame parameters

    struct FrameParams
    {
        uint32_t width = 0;
        uint32_t height = 0;
        float jitterX = 0.0f;
        float jitterY = 0.0f;
        float motionScaleX = 1.0f;
        float motionScaleY = 1.0f;
        float cameraNear = 0.1f;
        float cameraFar = 1000.0f;
        float cameraFovY = 1.0f;
        float frameTimeMs = 16.6f;
        float sharpness = 0.0f;
        bool reset = false;
        bool reactive = false;
    };

    // -----------------------------------------------------------------------------------------------
    // NVIDIA DLSS

    class DLSS
    {
        ID3D12Device* device = nullptr;
        NVSDK_NGX_Parameter* capabilities = nullptr;
        NVSDK_NGX_Parameter* parameters = nullptr;
        NVSDK_NGX_Handle* feature = nullptr;
        bool initialized = false;

        // NGX keeps using the feature search paths after initialization
        std::vector<std::wstring> paths;
        std::vector<const wchar_t*> pathPointers;

    public:
        bool available = false;

        bool Init(ID3D12Device* d3d, const std::vector<std::wstring>& searchPaths, const std::wstring& dataPath, std::string& message)
        {
            device = d3d;

            paths = searchPaths;
            pathPointers.clear();
            for (auto& path : paths)
                pathPointers.push_back(path.c_str());

            NVSDK_NGX_FeatureCommonInfo info{};
            info.PathListInfo.Path = pathPointers.data();
            info.PathListInfo.Length = static_cast<unsigned int>(pathPointers.size());

            // Identifies this integration to NGX, a project id is only needed for NVIDIA-registered titles
            auto result = NVSDK_NGX_D3D12_Init_with_ProjectID("1b4f6c2e-9a3d-4e57-8c21-6f0d3b7a9e14", NVSDK_NGX_ENGINE_TYPE_CUSTOM, "1.0",
                dataPath.c_str(), device, &info, NVSDK_NGX_Version_API);
            if (NVSDK_NGX_FAILED(result))
            {
                message = "NGX initialization failed: " + std::to_string(static_cast<uint32_t>(result));
                return false;
            }
            initialized = true;

            if (NVSDK_NGX_FAILED(NVSDK_NGX_D3D12_GetCapabilityParameters(&capabilities)) || !capabilities)
            {
                message = "NGX capability parameters are not available";
                return false;
            }

            int supported = 0;
            int needsUpdatedDriver = 0;
            NVSDK_NGX_Parameter_GetI(capabilities, NVSDK_NGX_Parameter_SuperSampling_NeedsUpdatedDriver, &needsUpdatedDriver);
            auto queried = NVSDK_NGX_Parameter_GetI(capabilities, NVSDK_NGX_Parameter_SuperSampling_Available, &supported);
            if (NVSDK_NGX_FAILED(queried) || !supported)
            {
                message = needsUpdatedDriver ? "DLSS needs a newer NVIDIA driver" : "DLSS is not supported: no RTX GPU or nvngx_dlss.dll not found";
                return false;
            }

            if (NVSDK_NGX_FAILED(NVSDK_NGX_D3D12_AllocateParameters(&parameters)) || !parameters)
            {
                message = "NGX parameters could not be allocated";
                return false;
            }

            available = true;
            message = "DLSS available";
            return true;
        }

        bool Create(ID3D12GraphicsCommandList* cmd, uint32_t width, uint32_t height, uint32_t preset)
        {
            Release();

            NVSDK_NGX_Parameter_SetUI(parameters, NVSDK_NGX_Parameter_DLSS_Hint_Render_Preset_DLAA, preset);

            NVSDK_NGX_DLSS_Create_Params create{};
            create.Feature.InWidth = width;
            create.Feature.InHeight = height;
            create.Feature.InTargetWidth = width;
            create.Feature.InTargetHeight = height;
            create.Feature.InPerfQualityValue = NVSDK_NGX_PerfQuality_Value_DLAA;
            // HDR scene color, motion vectors at render resolution without jitter, standard depth
            create.InFeatureCreateFlags = NVSDK_NGX_DLSS_Feature_Flags_IsHDR | NVSDK_NGX_DLSS_Feature_Flags_MVLowRes | NVSDK_NGX_DLSS_Feature_Flags_AutoExposure;
            create.InEnableOutputSubrects = false;

            auto result = NGX_D3D12_CREATE_DLSS_EXT(cmd, 1, 1, &feature, parameters, &create);
            if (NVSDK_NGX_FAILED(result))
            {
                Log("DLSS feature creation failed: 0x%08X", static_cast<uint32_t>(result));
                feature = nullptr;
                return false;
            }
            return true;
        }

        bool Evaluate(ID3D12GraphicsCommandList* cmd, Device& d, const FrameParams& frame)
        {
            if (!feature)
                return false;

            NVSDK_NGX_D3D12_DLSS_Eval_Params eval{};
            eval.Feature.pInColor = d.Texture(Protocol::Texture::Color);
            eval.Feature.pInOutput = d.Texture(Protocol::Texture::Output);
            eval.pInDepth = d.Texture(Protocol::Texture::Depth);
            eval.pInMotionVectors = d.Texture(Protocol::Texture::Motion);
            eval.InJitterOffsetX = frame.jitterX;
            eval.InJitterOffsetY = frame.jitterY;
            eval.InRenderSubrectDimensions = { frame.width, frame.height };
            eval.InReset = frame.reset ? 1 : 0;
            eval.InMVScaleX = frame.motionScaleX;
            eval.InMVScaleY = frame.motionScaleY;
            eval.InPreExposure = 1.0f;
            eval.InExposureScale = 1.0f;
            eval.InFrameTimeDeltaInMsec = frame.frameTimeMs;

            auto result = NGX_D3D12_EVALUATE_DLSS_EXT(cmd, feature, parameters, &eval);
            if (NVSDK_NGX_FAILED(result))
            {
                Log("DLSS evaluation failed: 0x%08X", static_cast<uint32_t>(result));
                return false;
            }
            return true;
        }

        void Release()
        {
            if (feature)
                NVSDK_NGX_D3D12_ReleaseFeature(feature);
            feature = nullptr;
        }

        void Shutdown()
        {
            Release();
            if (parameters)
                NVSDK_NGX_D3D12_DestroyParameters(parameters);
            if (capabilities)
                NVSDK_NGX_D3D12_DestroyParameters(capabilities);
            parameters = nullptr;
            capabilities = nullptr;
            if (initialized)
                NVSDK_NGX_D3D12_Shutdown1(device);
            initialized = false;
        }
    };

    // -----------------------------------------------------------------------------------------------
    // AMD FSR, through the FidelityFX API runtime supplied by the user

    class FSR
    {
        HMODULE module = nullptr;
        ffxFunctions functions{};
        ffxContext context = nullptr;
        ID3D12Device* device = nullptr;

    public:
        bool available = false;

        bool Init(ID3D12Device* d3d, const std::vector<std::wstring>& searchPaths, std::string& message)
        {
            device = d3d;

            // FidelityFX SDK 2.x ships a small loader next to the effect libraries, 1.1 a single library
            for (auto& dir : searchPaths)
            {
                for (auto name : { L"amd_fidelityfx_loader_dx12.dll", L"amd_fidelityfx_dx12.dll" })
                {
                    auto path = std::filesystem::path(dir) / name;
                    if (!std::filesystem::exists(path))
                        continue;
                    // The loader loads amd_fidelityfx_upscaler_dx12.dll by name later, which has to find it next to the
                    // loader and not only in the helper's folder
                    SetDllDirectoryW(dir.c_str());
                    AddDllDirectory(dir.c_str());
                    module = LoadLibraryExW(path.c_str(), nullptr, LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
                    if (module)
                    {
                        Log("FidelityFX runtime: %ls", path.c_str());
                        break;
                    }
                }
                if (module)
                    break;
            }

            if (!module)
            {
                message = "FSR is not available: amd_fidelityfx_loader_dx12.dll not found";
                return false;
            }

            ffxLoadFunctions(&functions, module);
            if (!functions.CreateContext || !functions.DestroyContext || !functions.Query || !functions.Dispatch)
            {
                message = "FSR is not available: the FidelityFX runtime has no FidelityFX API exports";
                return false;
            }

            uint64_t count = 0;
            ffxQueryDescGetVersions versions{};
            versions.header.type = FFX_API_QUERY_DESC_TYPE_GET_VERSIONS;
            versions.createDescType = FFX_API_CREATE_CONTEXT_DESC_TYPE_UPSCALE;
            versions.device = device;
            versions.outputCount = &count;
            if (functions.Query(nullptr, &versions.header) != FFX_API_RETURN_OK || count == 0)
            {
                message = "FSR is not available: the FidelityFX runtime has no upscaler for this GPU";
                return false;
            }

            std::vector<uint64_t> ids(count);
            std::vector<const char*> names(count);
            versions.versionIds = ids.data();
            versions.versionNames = names.data();
            if (functions.Query(nullptr, &versions.header) == FFX_API_RETURN_OK)
                for (uint64_t i = 0; i < count; ++i)
                    Log("FSR version available: %s", names[i] ? names[i] : "?");

            available = true;
            message = "FSR available";
            return true;
        }

        bool Create(uint32_t width, uint32_t height)
        {
            Release();

            ffxCreateBackendDX12Desc backend{};
            backend.header.type = FFX_API_CREATE_CONTEXT_DESC_TYPE_BACKEND_DX12;
            backend.device = device;

            ffxCreateContextDescUpscale create{};
            create.header.type = FFX_API_CREATE_CONTEXT_DESC_TYPE_UPSCALE;
            create.header.pNext = &backend.header;
            create.flags = FFX_UPSCALE_ENABLE_HIGH_DYNAMIC_RANGE | FFX_UPSCALE_ENABLE_AUTO_EXPOSURE;
            create.maxRenderSize = { width, height };
            create.maxUpscaleSize = { width, height };

            auto result = functions.CreateContext(&context, &create.header, nullptr);
            if (result != FFX_API_RETURN_OK)
            {
                Log("FSR context creation failed: %u", result);
                context = nullptr;
                return false;
            }

            ffxQueryGetProviderVersion provider{};
            provider.header.type = FFX_API_QUERY_DESC_TYPE_GET_PROVIDER_VERSION;
            if (functions.Query(&context, &provider.header) == FFX_API_RETURN_OK && provider.versionName)
                Log("FSR provider: %s", provider.versionName);
            return true;
        }

        bool Evaluate(ID3D12GraphicsCommandList* cmd, Device& d, const FrameParams& frame)
        {
            if (!context)
                return false;

            ffxDispatchDescUpscale dispatch{};
            dispatch.header.type = FFX_API_DISPATCH_DESC_TYPE_UPSCALE;
            dispatch.commandList = cmd;
            dispatch.color = ffxApiGetResourceDX12(d.Texture(Protocol::Texture::Color), FFX_API_RESOURCE_STATE_COMPUTE_READ);
            dispatch.depth = ffxApiGetResourceDX12(d.Texture(Protocol::Texture::Depth), FFX_API_RESOURCE_STATE_COMPUTE_READ);
            dispatch.motionVectors = ffxApiGetResourceDX12(d.Texture(Protocol::Texture::Motion), FFX_API_RESOURCE_STATE_COMPUTE_READ);
            if (frame.reactive)
                dispatch.reactive = ffxApiGetResourceDX12(d.Texture(Protocol::Texture::Reactive), FFX_API_RESOURCE_STATE_COMPUTE_READ);
            dispatch.output = ffxApiGetResourceDX12(d.Texture(Protocol::Texture::Output), FFX_API_RESOURCE_STATE_UNORDERED_ACCESS);
            dispatch.jitterOffset = { frame.jitterX, frame.jitterY };
            dispatch.motionVectorScale = { frame.motionScaleX, frame.motionScaleY };
            dispatch.renderSize = { frame.width, frame.height };
            dispatch.upscaleSize = { frame.width, frame.height };
            dispatch.enableSharpening = frame.sharpness > 0.0f;
            dispatch.sharpness = frame.sharpness;
            dispatch.frameTimeDelta = frame.frameTimeMs;
            dispatch.preExposure = 1.0f;
            dispatch.reset = frame.reset;
            dispatch.cameraNear = frame.cameraNear;
            dispatch.cameraFar = frame.cameraFar;
            dispatch.cameraFovAngleVertical = frame.cameraFovY;
            dispatch.viewSpaceToMetersFactor = 1.0f;

            auto result = functions.Dispatch(&context, &dispatch.header);
            if (result != FFX_API_RETURN_OK)
            {
                Log("FSR dispatch failed: %u", result);
                return false;
            }
            return true;
        }

        void Release()
        {
            if (context)
                functions.DestroyContext(&context, nullptr);
            context = nullptr;
        }

        void Shutdown()
        {
            Release();
            if (module)
                FreeLibrary(module);
            module = nullptr;
        }
    };

    // -----------------------------------------------------------------------------------------------

    class Helper
    {
        Connection connection;
        Device device;
        DLSS dlss;
        FSR fsr;
        Protocol::Backend backend = Protocol::Backend::None;
        uint32_t width = 0;
        uint32_t height = 0;
        uint32_t flags = 0;

        bool Duplicate(HANDLE source, uint64_t& target)
        {
            HANDLE duplicate = nullptr;
            if (!DuplicateHandle(GetCurrentProcess(), source, connection.game, &duplicate, 0, FALSE, DUPLICATE_SAME_ACCESS))
                return false;
            target = reinterpret_cast<uint64_t>(duplicate);
            return true;
        }

        bool Configure()
        {
            auto& shared = *connection.shared;

            dlss.Release();
            fsr.Release();
            device.ReleaseTextures();
            backend = Protocol::Backend::None;

            width = shared.Width;
            height = shared.Height;
            flags = shared.Flags;
            if (width == 0 || height == 0 || width > 16384 || height > 16384)
                return false;

            using T = Protocol::Texture;
            if (!device.CreateTexture(T::Color, width, height, DXGI_FORMAT_R16G16B16A16_FLOAT, false) ||
                !device.CreateTexture(T::Depth, width, height, DXGI_FORMAT_R32_FLOAT, false) ||
                !device.CreateTexture(T::Motion, width, height, DXGI_FORMAT_R16G16_FLOAT, false) ||
                !device.CreateTexture(T::Reactive, width, height, DXGI_FORMAT_R16_FLOAT, false) ||
                !device.CreateTexture(T::Output, width, height, DXGI_FORMAT_R16G16B16A16_FLOAT, true) ||
                !device.CreateSharedFence())
            {
                connection.Message("Shared textures could not be created");
                return false;
            }

            bool created = false;
            if (shared.ConfigureBackend == Protocol::Backend::DLSS && dlss.available)
            {
                auto cmd = device.BeginImmediate();
                created = dlss.Create(cmd, width, height, shared.DLSSPreset);
                created = device.SubmitImmediate() && created;
            }
            else if (shared.ConfigureBackend == Protocol::Backend::FSR && fsr.available)
            {
                created = fsr.Create(width, height);
            }

            if (!created)
            {
                connection.Message("The upscaler could not be created at %ux%u", width, height);
                return false;
            }

            for (size_t i = 0; i < static_cast<size_t>(T::Count); ++i)
            {
                shared.TextureSizes[i] = device.AllocationSize(static_cast<T>(i));
                if (!Duplicate(device.textures[i].handle, shared.TextureHandles[i]))
                    return false;
            }
            if (!Duplicate(device.sharedFenceHandle, shared.FenceHandle))
                return false;

            backend = shared.ConfigureBackend;
            connection.Message("%s ready at %ux%u", backend == Protocol::Backend::DLSS ? "DLSS" : "FSR", width, height);
            return true;
        }

        bool Evaluate()
        {
            auto& shared = *connection.shared;
            if (backend == Protocol::Backend::None || !device.sharedFence)
                return false;

            FrameParams frame;
            frame.width = width;
            frame.height = height;
            frame.jitterX = shared.JitterX;
            frame.jitterY = shared.JitterY;
            frame.motionScaleX = shared.MotionScaleX;
            frame.motionScaleY = shared.MotionScaleY;
            frame.cameraNear = shared.CameraNear;
            frame.cameraFar = shared.CameraFar;
            frame.cameraFovY = shared.CameraFovY;
            frame.frameTimeMs = shared.FrameTimeMs;
            frame.sharpness = shared.Sharpness;
            frame.reset = shared.Reset != 0;
            frame.reactive = (flags & Protocol::ConfigureFlags::ReactiveMask) != 0;

            auto cmd = device.BeginFrame();
            device.Transition(cmd, true);
            bool evaluated = backend == Protocol::Backend::DLSS ? dlss.Evaluate(cmd, device, frame) : fsr.Evaluate(cmd, device, frame);
            device.Transition(cmd, false);

            // The fence always advances, so the game can rely on the values it waits for
            device.SubmitFrame(shared.WaitValue, shared.SignalValue, evaluated);
            return evaluated;
        }

    public:
        int Run(const std::wstring& name)
        {
            if (!connection.Open(name))
                return 1;

            auto& shared = *connection.shared;
            gLog = _wfopen(shared.LogPath, L"w");
            Log("GTAIV.EFLC.FusionFix upscaler helper started for process %u", shared.GameProcessId);

            LUID luid{ shared.AdapterLuidLow, shared.AdapterLuidHigh };
            if (!device.Create(luid))
            {
                connection.Message("D3D12 device could not be created on the game's adapter");
                connection.Respond(Protocol::Status::Failed);
                return 2;
            }

            DXGI_ADAPTER_DESC1 adapterDesc{};
            device.adapter->GetDesc1(&adapterDesc);
            Log("Adapter: %ls", adapterDesc.Description);

            std::vector<std::wstring> searchPaths = { shared.PluginsDirectory, shared.GameDirectory };
            std::string dlssMessage;
            std::string fsrMessage;
            if (adapterDesc.VendorId == 0x10DE)
                dlss.Init(device.device.Get(), searchPaths, shared.PluginsDirectory, dlssMessage);
            else
                dlssMessage = "DLSS is not available: not an NVIDIA GPU";
            fsr.Init(device.device.Get(), searchPaths, fsrMessage);

            Log("%s", dlssMessage.c_str());
            Log("%s", fsrMessage.c_str());
            shared.DLSSAvailable = dlss.available;
            shared.FSRAvailable = fsr.available;
            connection.Message("%s; %s", dlssMessage.c_str(), fsrMessage.c_str());
            connection.Respond(Protocol::Status::Ok);

            HANDLE handles[] = { connection.request, connection.game };
            while (true)
            {
                auto wait = WaitForMultipleObjects(2, handles, FALSE, INFINITE);
                if (wait != WAIT_OBJECT_0)
                    break;

                MemoryBarrier();
                auto command = shared.RequestCommand;
                if (command == Protocol::Command::Shutdown)
                {
                    connection.Respond(Protocol::Status::Ok);
                    break;
                }

                bool ok = false;
                switch (command)
                {
                case Protocol::Command::Configure: ok = Configure(); break;
                case Protocol::Command::Evaluate: ok = Evaluate(); break;
                default: break;
                }
                connection.Respond(ok ? Protocol::Status::Ok : Protocol::Status::Failed);
            }

            if (device.queue && device.localFence)
            {
                device.queue->Signal(device.localFence.Get(), ++device.localValue);
                device.WaitFence(device.localFence.Get(), device.localValue, 2000);
            }
            dlss.Shutdown();
            fsr.Shutdown();
            device.ReleaseTextures();
            Log("Helper stopped");
            if (gLog)
                fclose(gLog);
            return 0;
        }
    };
}

int WINAPI wWinMain(HINSTANCE, HINSTANCE, LPWSTR, int)
{
    int argc = 0;
    auto argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (!argv || argc < 3 || std::wstring_view(argv[1]) != Protocol::ArgumentName)
        return 0;

    std::wstring name = argv[2];
    LocalFree(argv);

    Helper helper;
    return helper.Run(name);
}
