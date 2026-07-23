#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <vector>
#include <string>
#include <fstream>
#include <filesystem>
#include <shlobj.h>

// Vulkan API version macros (used in GPU detection)
#define VK_MAKE_API_VERSION(variant, major, minor, patch) \
    ((((uint32_t)(variant)) << 29) | (((uint32_t)(major)) << 22) | (((uint32_t)(minor)) << 12) | ((uint32_t)(patch)))
#define VK_API_VERSION_1_0 VK_MAKE_API_VERSION(0, 1, 0, 0)
#define VK_API_VERSION_1_2 VK_MAKE_API_VERSION(0, 1, 2, 0)
#define VK_API_VERSION_1_3 VK_MAKE_API_VERSION(0, 1, 3, 0)
#define VK_API_VERSION_MAJOR(version) (((uint32_t)(version)) >> 22)
#define VK_API_VERSION_MINOR(version) ((((uint32_t)(version)) >> 12) & 0x3FF)
#define VK_API_VERSION_PATCH(version) ((uint32_t)(version) & 0xFFF)

// Minimal Vulkan types for GPU capability detection
#define VKAPI_PTR __stdcall
typedef uint32_t VkFlags;
typedef uint32_t VkBool32;
typedef VkFlags VkInstanceCreateFlags;
typedef uint64_t VkDeviceSize;
typedef uint32_t VkMemoryHeapFlags;

typedef enum VkResult
{
    VK_SUCCESS = 0,
    VK_ERROR_INCOMPATIBLE_DRIVER = -9,
    VK_ERROR_OUT_OF_HOST_MEMORY = -5,
    VK_ERROR_OUT_OF_DEVICE_MEMORY = -6,
    VK_ERROR_INITIALIZATION_FAILED = -3,
} VkResult;

typedef enum VkStructureType
{
    VK_STRUCTURE_TYPE_APPLICATION_INFO = 0,
    VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO = 1,
} VkStructureType;

typedef enum VkPhysicalDeviceType
{
    VK_PHYSICAL_DEVICE_TYPE_OTHER = 0,
    VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU = 1,
    VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU = 2,
    VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU = 3,
    VK_PHYSICAL_DEVICE_TYPE_CPU = 4,
} VkPhysicalDeviceType;

typedef struct VkApplicationInfo
{
    VkStructureType sType;
    const void* pNext;
    const char* pApplicationName;
    uint32_t applicationVersion;
    const char* pEngineName;
    uint32_t engineVersion;
    uint32_t apiVersion;
} VkApplicationInfo;

typedef struct VkInstanceCreateInfo
{
    VkStructureType sType;
    const void* pNext;
    VkInstanceCreateFlags flags;
    const VkApplicationInfo* pApplicationInfo;
    uint32_t enabledLayerCount;
    const char* const* ppEnabledLayerNames;
    uint32_t enabledExtensionCount;
    const char* const* ppEnabledExtensionNames;
} VkInstanceCreateInfo;

typedef struct VkPhysicalDeviceProperties
{
    uint32_t apiVersion;
    uint32_t driverVersion;
    uint32_t vendorID;
    uint32_t deviceID;
    VkPhysicalDeviceType deviceType;
    char deviceName[256];
    uint8_t pipelineCacheUUID[16];
    // ...
} VkPhysicalDeviceProperties;

typedef struct VkMemoryHeap
{
    VkMemoryHeapFlags flags;
    VkDeviceSize size;
} VkMemoryHeap;

#define VK_MAX_MEMORY_TYPES 32
#define VK_MAX_MEMORY_HEAPS 16

typedef struct VkPhysicalDeviceMemoryProperties
{
    uint32_t memoryTypeCount;
    uint8_t _padding_types[32 * 8]; // VkMemoryType memoryTypes[VK_MAX_MEMORY_TYPES];
    uint32_t memoryHeapCount;
    VkMemoryHeap memoryHeaps[VK_MAX_MEMORY_HEAPS];
} VkPhysicalDeviceMemoryProperties;

typedef VkResult (VKAPI_PTR* PFN_vkEnumerateInstanceVersion)(uint32_t* pApiVersion);
typedef VkResult (VKAPI_PTR* PFN_vkCreateInstance)(const VkInstanceCreateInfo* pCreateInfo, const void* pAllocator, uintptr_t* pInstance);
typedef void (VKAPI_PTR* PFN_vkDestroyInstance)(uintptr_t instance, const void* pAllocator);
typedef VkResult (VKAPI_PTR* PFN_vkEnumeratePhysicalDevices)(uintptr_t instance, uint32_t* pPhysicalDeviceCount, uintptr_t* pPhysicalDevices);
typedef void (VKAPI_PTR* PFN_vkGetPhysicalDeviceProperties)(uintptr_t physicalDevice, VkPhysicalDeviceProperties* pProperties);
typedef void (VKAPI_PTR* PFN_vkGetPhysicalDeviceMemoryProperties)(uintptr_t physicalDevice, VkPhysicalDeviceMemoryProperties* pMemoryProperties);

// DXVK version selection result
enum class DxvkVersion
{
    None,           // fall back to system d3d9
    Vulkan262,      // vulkan262.dll (DXVK 2.6.2 — safest, for older hardware)
    Vulkan271,      // vulkan271.dll (DXVK 2.7.1 — mid-range)
    VulkanLatest,   // vulkan.dll    (latest DXVK release, v3.x when available)
};

// Which DXVK DLL to load based on API config value:
//   0 = system d3d9 (no DXVK)
//   1 = auto-detect
//   2 = force vulkan262.dll (DXVK 2.6.2)
//   3 = force vulkan271.dll (DXVK 2.7.1)
//   4 = force vulkan.dll    (latest DXVK)
static DxvkVersion GetDxvkVersionFromApiValue(int apiValue)
{
    switch (apiValue)
    {
        case 2: return DxvkVersion::Vulkan262;
        case 3: return DxvkVersion::Vulkan271;
        case 4: return DxvkVersion::VulkanLatest;
        case 1: return DxvkVersion::None; // auto-detect — caller will run detection
        default: return DxvkVersion::None;
    }
}

static const wchar_t* GetDxvkDllName(DxvkVersion ver)
{
    switch (ver)
    {
        case DxvkVersion::Vulkan262:    return L"vulkan262.dll";
        case DxvkVersion::Vulkan271:    return L"vulkan271.dll";
        case DxvkVersion::VulkanLatest: return L"vulkan.dll";
        default: return nullptr;
    }
}

// Auto-detect the best DXVK version for the user's GPU via Vulkan capability queries.
// Returns DxvkVersion::None if Vulkan is unavailable or too old.
static DxvkVersion DetectBestDxvkVersion()
{
    HMODULE hVulkan = LoadLibraryW(L"vulkan-1.dll");
    if (!hVulkan)
        return DxvkVersion::None;

    // Check Vulkan instance version first
    auto _vkEnumerateInstanceVersion = (PFN_vkEnumerateInstanceVersion)GetProcAddress(hVulkan, "vkEnumerateInstanceVersion");
    uint32_t instanceVersion = VK_API_VERSION_1_0;
    if (_vkEnumerateInstanceVersion)
        _vkEnumerateInstanceVersion(&instanceVersion);

    // DXVK requires at least Vulkan 1.2
    if (instanceVersion < VK_API_VERSION_1_2)
    {
        FreeLibrary(hVulkan);
        return DxvkVersion::None;
    }

    auto _vkCreateInstance = (PFN_vkCreateInstance)GetProcAddress(hVulkan, "vkCreateInstance");
    auto _vkDestroyInstance = (PFN_vkDestroyInstance)GetProcAddress(hVulkan, "vkDestroyInstance");
    auto _vkEnumeratePhysicalDevices = (PFN_vkEnumeratePhysicalDevices)GetProcAddress(hVulkan, "vkEnumeratePhysicalDevices");
    auto _vkGetPhysicalDeviceProperties = (PFN_vkGetPhysicalDeviceProperties)GetProcAddress(hVulkan, "vkGetPhysicalDeviceProperties");
    auto _vkGetPhysicalDeviceMemoryProperties = (PFN_vkGetPhysicalDeviceMemoryProperties)GetProcAddress(hVulkan, "vkGetPhysicalDeviceMemoryProperties");

    if (!_vkCreateInstance || !_vkDestroyInstance || !_vkEnumeratePhysicalDevices
        || !_vkGetPhysicalDeviceProperties || !_vkGetPhysicalDeviceMemoryProperties)
    {
        FreeLibrary(hVulkan);
        return DxvkVersion::None;
    }

    // Create a minimal Vulkan instance
    VkApplicationInfo appInfo = {};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.apiVersion = instanceVersion; // use highest available

    VkInstanceCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;

    uintptr_t vkInstance = 0;
    VkResult result = _vkCreateInstance(&createInfo, nullptr, &vkInstance);
    if (result != VK_SUCCESS || vkInstance == 0)
    {
        FreeLibrary(hVulkan);
        return DxvkVersion::None;
    }

    // Enumerate physical devices
    uint32_t deviceCount = 0;
    _vkEnumeratePhysicalDevices(vkInstance, &deviceCount, nullptr);
    if (deviceCount == 0)
    {
        _vkDestroyInstance(vkInstance, nullptr);
        FreeLibrary(hVulkan);
        return DxvkVersion::None;
    }

    std::vector<uintptr_t> physicalDevices(deviceCount);
    _vkEnumeratePhysicalDevices(vkInstance, &deviceCount, physicalDevices.data());

    // Find the best GPU: prefer discrete, then highest VRAM, then highest Vulkan version
    DxvkVersion bestVersion = DxvkVersion::None;
    uint32_t bestDeviceApiVersion = 0;
    VkPhysicalDeviceType bestDeviceType = VK_PHYSICAL_DEVICE_TYPE_OTHER;
    VkDeviceSize bestVram = 0;
    uint32_t bestVendorID = 0;
    uint32_t bestDeviceID = 0;
    char bestDeviceName[256] = {};

    for (uint32_t i = 0; i < deviceCount; ++i)
    {
        VkPhysicalDeviceProperties props = {};
        _vkGetPhysicalDeviceProperties(physicalDevices[i], &props);

        VkPhysicalDeviceMemoryProperties memProps = {};
        _vkGetPhysicalDeviceMemoryProperties(physicalDevices[i], &memProps);

        // Calculate total VRAM from device-local heaps
        VkDeviceSize vram = 0;
        for (uint32_t h = 0; h < memProps.memoryHeapCount; ++h)
        {
            if (memProps.memoryHeaps[h].flags & 0x1) // VK_MEMORY_HEAP_DEVICE_LOCAL_BIT
                vram += memProps.memoryHeaps[h].size;
        }

        // Scoring: discrete > integrated > other; then by VRAM; then by apiVersion
        bool isBetter = false;
        if (bestDeviceType != VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU
            && props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
        {
            isBetter = true;
        }
        else if (bestDeviceType == props.deviceType)
        {
            if (vram > bestVram)
                isBetter = true;
        }

        if (isBetter || i == 0)
        {
            bestDeviceApiVersion = props.apiVersion;
            bestDeviceType = props.deviceType;
            bestVram = vram;
            bestVendorID = props.vendorID;
            bestDeviceID = props.deviceID;
            memcpy(bestDeviceName, props.deviceName, sizeof(bestDeviceName));
            bestDeviceName[255] = '\0';
        }
    }

    // Decide DXVK version based on GPU capabilities
    if (bestDeviceApiVersion >= VK_API_VERSION_1_3)
    {
        // Vulkan 1.3 support — can use latest DXVK (v3.x) on modern hardware
        // Use 2.7.1 for integrated/older GPUs with Vulkan 1.3 but limited VRAM
        if (bestDeviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU && bestVram >= 2ULL * 1024 * 1024 * 1024)
        {
            bestVersion = DxvkVersion::VulkanLatest;
        }
        else
        {
            bestVersion = DxvkVersion::Vulkan271;
        }
    }
    else if (bestDeviceApiVersion >= VK_API_VERSION_1_2)
    {
        // Vulkan 1.2 only — use DXVK 2.x
        // For older 1.2 implementations (pre-2018 GPUs), prefer 2.6.2 for stability
        // Heuristic: integrated GPU or <2GB VRAM → 2.6.2; otherwise → 2.7.1
        if (bestDeviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU
            || bestVram < 2ULL * 1024 * 1024 * 1024)
        {
            bestVersion = DxvkVersion::Vulkan262;
        }
        else
        {
            bestVersion = DxvkVersion::Vulkan271;
        }
    }
    else
    {
        bestVersion = DxvkVersion::None; // Vulkan < 1.2, DXVK won't work
    }

    // Optional: log the detection result via OutputDebugString
    char debugMsg[512];
    snprintf(debugMsg, sizeof(debugMsg),
        "[D3D9 Wrapper] GPU: %s | Vulkan %u.%u.%u | VRAM: %llu MB | Type: %u | Selected DXVK: %d\n",
        bestDeviceName,
        VK_API_VERSION_MAJOR(bestDeviceApiVersion),
        VK_API_VERSION_MINOR(bestDeviceApiVersion),
        VK_API_VERSION_PATCH(bestDeviceApiVersion),
        (unsigned long long)(bestVram / (1024 * 1024)),
        (unsigned)bestDeviceType,
        (int)bestVersion);
    OutputDebugStringA(debugMsg);

    _vkDestroyInstance(vkInstance, nullptr);
    FreeLibrary(hVulkan);
    return bestVersion;
}

struct d3d9_dll
{
    HMODULE dll;
    FARPROC D3DPERF_BeginEvent;
    FARPROC D3DPERF_EndEvent;
    FARPROC D3DPERF_GetStatus;
    FARPROC D3DPERF_QueryRepeatFrame;
    FARPROC D3DPERF_SetMarker;
    FARPROC D3DPERF_SetOptions;
    FARPROC D3DPERF_SetRegion;
    FARPROC DebugSetLevel;
    FARPROC DebugSetMute;
    FARPROC Direct3D9EnableMaximizedWindowedModeShim;
    FARPROC Direct3DCreate9;
    FARPROC Direct3DCreate9Ex;
    FARPROC Direct3DCreate9On12;
    FARPROC Direct3DCreate9On12Ex;
    FARPROC Direct3DShaderValidatorCreate9;
    FARPROC PSGPError;
    FARPROC PSGPSampleTexture;
} d3d9;

__declspec(naked) void _D3DPERF_BeginEvent()
{
    _asm { jmp[d3d9.D3DPERF_BeginEvent] }
}
__declspec(naked) void _D3DPERF_EndEvent()
{
    _asm { jmp[d3d9.D3DPERF_EndEvent] }
}
__declspec(naked) void _D3DPERF_GetStatus()
{
    _asm { jmp[d3d9.D3DPERF_GetStatus] }
}
__declspec(naked) void _D3DPERF_QueryRepeatFrame()
{
    _asm { jmp[d3d9.D3DPERF_QueryRepeatFrame] }
}
__declspec(naked) void _D3DPERF_SetMarker()
{
    _asm { jmp[d3d9.D3DPERF_SetMarker] }
}
__declspec(naked) void _D3DPERF_SetOptions()
{
    _asm { jmp[d3d9.D3DPERF_SetOptions] }
}
__declspec(naked) void _D3DPERF_SetRegion()
{
    _asm { jmp[d3d9.D3DPERF_SetRegion] }
}
__declspec(naked) void _DebugSetLevel()
{
    _asm { jmp[d3d9.DebugSetLevel] }
}
__declspec(naked) void _DebugSetMute()
{
    _asm { jmp[d3d9.DebugSetMute] }
}
__declspec(naked) void _Direct3D9EnableMaximizedWindowedModeShim()
{
    _asm { jmp[d3d9.Direct3D9EnableMaximizedWindowedModeShim] }
}
__declspec(naked) void _Direct3DCreate9()
{
    _asm { jmp[d3d9.Direct3DCreate9] }
}
__declspec(naked) void _Direct3DCreate9Ex()
{
    _asm { jmp[d3d9.Direct3DCreate9Ex] }
}
__declspec(naked) void _Direct3DCreate9On12()
{
    _asm { jmp[d3d9.Direct3DCreate9On12] }
}
__declspec(naked) void _Direct3DCreate9On12Ex()
{
    _asm { jmp[d3d9.Direct3DCreate9On12Ex] }
}
__declspec(naked) void _Direct3DShaderValidatorCreate9()
{
    _asm { jmp[d3d9.Direct3DShaderValidatorCreate9] }
}
__declspec(naked) void _PSGPError()
{
    _asm { jmp[d3d9.PSGPError] }
}
__declspec(naked) void _PSGPSampleTexture()
{
    _asm { jmp[d3d9.PSGPSampleTexture] }
}

void _FusionFixGraphicsApiSwitch()
{
    return;
}

std::filesystem::path GetModulePath(HMODULE hModule)
{
    static constexpr auto INITIAL_BUFFER_SIZE = MAX_PATH;
    static constexpr auto MAX_ITERATIONS = 7;

    std::u16string ret;
    std::filesystem::path pathret;
    auto bufferSize = INITIAL_BUFFER_SIZE;
    for (size_t iterations = 0; iterations < MAX_ITERATIONS; ++iterations)
    {
        ret.resize(bufferSize);
        size_t charsReturned = 0;
        charsReturned = GetModuleFileNameW(hModule, (LPWSTR)&ret[0], bufferSize);
        if (charsReturned < ret.length())
        {
            ret.resize(charsReturned);
            pathret = ret;
            return pathret;
        }
        else
        {
            bufferSize *= 2;
        }
    }

    return {};
}

std::filesystem::path GetExeModulePath()
{
    std::filesystem::path r = GetModulePath(NULL);
    return r.parent_path();
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
    switch (ul_reason_for_call)
    {
        case DLL_PROCESS_ATTACH:
        {
            d3d9.dll = NULL;

            WCHAR path[MAX_PATH];
            int apiValue = 0;
            {
                std::vector<std::filesystem::path> cfgPaths;
                auto cfgName = L"d3d9.cfg";
                cfgPaths.emplace_back(GetExeModulePath() / cfgName);
                if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, path)))
                    cfgPaths.emplace_back(std::filesystem::path(path) / L"Rockstar Games\\GTA IV\\" / cfgName);
                if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, path)))
                    cfgPaths.emplace_back(std::filesystem::path(path) / cfgName);
                if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_MYDOCUMENTS, NULL, 0, path)))
                    cfgPaths.emplace_back(std::filesystem::path(path) / cfgName);

                std::filesystem::path cfgPath;

                // First, try to find an existing readable file
                for (auto& it : cfgPaths)
                {
                    std::error_code ec;
                    if (std::filesystem::exists(it, ec) && !ec)
                    {
                        auto status = std::filesystem::status(it, ec);
                        if (!ec && status.type() == std::filesystem::file_type::regular)
                        {
                            // Check if we can read from this file
                            std::ifstream testFile(it);
                            if (testFile.good())
                            {
                                cfgPath = it;
                                testFile.close();
                                break;
                            }
                        }
                    }
                }

                // If no existing readable file found, find a writable location for new file creation
                if (cfgPath.empty())
                {
                    for (auto& it : cfgPaths)
                    {
                        std::error_code ec;
                        auto status = std::filesystem::status(it, ec);

                        if (ec && ec.value() == 2) // File doesn't exist, check if directory is writable
                        {
                            std::ofstream ofile;
                            ofile.open(it, std::ios::binary);
                            if (ofile.is_open())
                            {
                                cfgPath = it;
                                ofile.close();
                                break;
                            }
                        }
                        else if (!ec && ((std::filesystem::perms::owner_read & status.permissions()) == std::filesystem::perms::owner_read &&
                                 (std::filesystem::perms::owner_write & status.permissions()) == std::filesystem::perms::owner_write))
                        {
                            cfgPath = it;
                            break;
                        }
                    }
                }

                // Fallback to first path if nothing else worked
                if (cfgPath.empty())
                {
                    cfgPath = cfgPaths.front();
                }

                apiValue = GetPrivateProfileIntW(L"MAIN", L"API", 0, cfgPath.c_str());
            }

            if (apiValue)
            {
                DxvkVersion dxvkVer = GetDxvkVersionFromApiValue(apiValue);

                // Auto-detect if apiValue == 1
                if (dxvkVer == DxvkVersion::None && apiValue == 1)
                {
                    dxvkVer = DetectBestDxvkVersion();
                }

                const wchar_t* dllName = GetDxvkDllName(dxvkVer);
                if (dllName)
                {
                    lstrcpyW(path, dllName);
                    d3d9.dll = LoadLibraryW(path);
                }
            }

            if (d3d9.dll == NULL)
            {
                DWORD pathLen = GetSystemDirectoryW(path, MAX_PATH - wcslen(L"\\d3d9.dll"));
                lstrcpyW(path + pathLen, L"\\d3d9.dll");
                d3d9.dll = LoadLibraryW(path);
            }

            if (d3d9.dll == NULL)
            {
                DWORD error = GetLastError();
                wchar_t errorMsg[512];
                wsprintfW(errorMsg, L"Failed to load D3D9 implementation!\nError code: %lu\nPath: %ls", error, path);
                MessageBoxW(0, errorMsg, L"DLL Load Error", MB_ICONERROR);
                ExitProcess(0);
            }

            d3d9.D3DPERF_BeginEvent = GetProcAddress(d3d9.dll, "D3DPERF_BeginEvent");
            d3d9.D3DPERF_EndEvent = GetProcAddress(d3d9.dll, "D3DPERF_EndEvent");
            d3d9.D3DPERF_GetStatus = GetProcAddress(d3d9.dll, "D3DPERF_GetStatus");
            d3d9.D3DPERF_QueryRepeatFrame = GetProcAddress(d3d9.dll, "D3DPERF_QueryRepeatFrame");
            d3d9.D3DPERF_SetMarker = GetProcAddress(d3d9.dll, "D3DPERF_SetMarker");
            d3d9.D3DPERF_SetOptions = GetProcAddress(d3d9.dll, "D3DPERF_SetOptions");
            d3d9.D3DPERF_SetRegion = GetProcAddress(d3d9.dll, "D3DPERF_SetRegion");
            d3d9.DebugSetLevel = GetProcAddress(d3d9.dll, "DebugSetLevel");
            d3d9.DebugSetMute = GetProcAddress(d3d9.dll, "DebugSetMute");
            d3d9.Direct3D9EnableMaximizedWindowedModeShim = GetProcAddress(d3d9.dll, "Direct3D9EnableMaximizedWindowedModeShim");
            d3d9.Direct3DCreate9 = GetProcAddress(d3d9.dll, "Direct3DCreate9");
            d3d9.Direct3DCreate9Ex = GetProcAddress(d3d9.dll, "Direct3DCreate9Ex");
            d3d9.Direct3DCreate9On12 = GetProcAddress(d3d9.dll, "Direct3DCreate9On12");
            d3d9.Direct3DCreate9On12Ex = GetProcAddress(d3d9.dll, "Direct3DCreate9On12Ex");
            d3d9.Direct3DShaderValidatorCreate9 = GetProcAddress(d3d9.dll, "Direct3DShaderValidatorCreate9");
            d3d9.PSGPError = GetProcAddress(d3d9.dll, "PSGPError");
            d3d9.PSGPSampleTexture = GetProcAddress(d3d9.dll, "PSGPSampleTexture");
            break;
        }
        case DLL_PROCESS_DETACH:
        {
            FreeLibrary(d3d9.dll);
            break;
        }
    }
    return TRUE;
}