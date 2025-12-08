module;

#include <common.hxx>
#include <dxgi1_4.h>
#include <psapi.h>
#include <D3D9Types.h>

export module vram;

import common;
import comvars;
import settings;

export unsigned int nStreamingMemory = 0;

constexpr uint64_t _2048mb = 2147483648ull;

uint64_t GetProcessPreferredGPUMemory()
{
    uint64_t memory = _2048mb;

    IDirect3DDevice9* device = rage::grcDevice::GetD3DDevice();
    if (!device)
        return memory;

    // Get D3D9 adapter details
    D3DDEVICE_CREATION_PARAMETERS params;
    if (FAILED(device->GetCreationParameters(&params)))
        return memory;

    IDirect3D9* d3d9 = nullptr;
    if (FAILED(device->GetDirect3D(&d3d9)) || !d3d9)
        return memory;

    D3DADAPTER_IDENTIFIER9 identifier;
    if (FAILED(d3d9->GetAdapterIdentifier(params.AdapterOrdinal, 0, &identifier)))
    {
        d3d9->Release();
        return memory;
    }
    d3d9->Release();

    HMODULE hDxgi = LoadLibraryA("dxgi.dll");
    if (!hDxgi)
        return memory;

    // Helper to get system RAM
    MEMORYSTATUSEX memInfo;
    memInfo.dwLength = sizeof(MEMORYSTATUSEX);
    GlobalMemoryStatusEx(&memInfo);
    uint64_t total_system_ram = memInfo.ullTotalPhys;

    IDXGIAdapter* selected_adapter = nullptr;
    uint64_t max_dedicated = 0;

    auto EnumerateAdapters = [&](IDXGIFactory* factory) -> bool {
        if (!factory) return false;

        UINT adapter_index = 0;
        IDXGIAdapter* adapter = nullptr;
        while (SUCCEEDED(factory->EnumAdapters(adapter_index, &adapter)))
        {
            DXGI_ADAPTER_DESC desc;
            if (SUCCEEDED(adapter->GetDesc(&desc)))
            {
                // Match with D3D9 identifier
                if (desc.VendorId == identifier.VendorId &&
                    desc.DeviceId == identifier.DeviceId &&
                    desc.SubSysId == identifier.SubSysId &&
                    desc.Revision == identifier.Revision)
                {
                    if (selected_adapter)
                        selected_adapter->Release();
                    selected_adapter = adapter;
                    selected_adapter->AddRef();
                    adapter->Release();  // Release the Enum ref
                    return true;  // Found match, done
                }
                // Track max dedicated as fallback
                if (desc.DedicatedVideoMemory > max_dedicated)
                {
                    max_dedicated = desc.DedicatedVideoMemory;
                    if (selected_adapter)
                        selected_adapter->Release();
                    selected_adapter = adapter;
                    selected_adapter->AddRef();
                }
            }
            adapter->Release();
            adapter_index++;
        }
        return false;  // No match found
    };

    // Prefer CreateDXGIFactory2 with IDXGIFactory3
    IDXGIFactory* factory_to_use = nullptr;
    typedef HRESULT(WINAPI* CreateDXGIFactory2_t)(UINT Flags, REFIID riid, void** ppFactory);
    CreateDXGIFactory2_t CreateDXGIFactory2_fn = (CreateDXGIFactory2_t)GetProcAddress(hDxgi, "CreateDXGIFactory2");
    if (CreateDXGIFactory2_fn)
    {
        IDXGIFactory3* factory3 = nullptr;
        if (SUCCEEDED(CreateDXGIFactory2_fn(0, __uuidof(IDXGIFactory3), (void**)&factory3)))
        {
            factory_to_use = static_cast<IDXGIFactory*>(factory3);
        }
    }

    // Fallback to CreateDXGIFactory if Factory2 not available
    if (!factory_to_use)
    {
        typedef HRESULT(WINAPI* CreateDXGIFactory_t)(REFIID riid, void** ppFactory);
        CreateDXGIFactory_t CreateDXGIFactory_fn = (CreateDXGIFactory_t)GetProcAddress(hDxgi, "CreateDXGIFactory");
        if (CreateDXGIFactory_fn)
        {
            IDXGIFactory* factory = nullptr;
            if (SUCCEEDED(CreateDXGIFactory_fn(__uuidof(IDXGIFactory), (void**)&factory)))
            {
                factory_to_use = factory;
            }
        }
    }

    // Enumerate using the preferred factory
    if (factory_to_use)
    {
        EnumerateAdapters(factory_to_use);
        factory_to_use->Release();
    }

    if (!selected_adapter)
    {
        FreeLibrary(hDxgi);
        return memory;
    }

    // Now query the selected adapter
    uint64_t dedicated = 0;
    uint64_t shared = 0;
    uint64_t budget = 0;

    DXGI_ADAPTER_DESC desc;
    if (SUCCEEDED(selected_adapter->GetDesc(&desc)))
    {
        dedicated = desc.DedicatedVideoMemory;
        shared = desc.SharedSystemMemory;
    }

    IDXGIAdapter3* adapter3 = nullptr;
    if (SUCCEEDED(selected_adapter->QueryInterface(__uuidof(IDXGIAdapter3), (void**)&adapter3)))
    {
        DXGI_QUERY_VIDEO_MEMORY_INFO videoMemoryInfo;
        if (SUCCEEDED(adapter3->QueryVideoMemoryInfo(0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, &videoMemoryInfo)))
        {
            budget = videoMemoryInfo.Budget;
        }
        adapter3->Release();
    }
    else
    {
        IDXGIAdapter2* adapter2 = nullptr;
        if (SUCCEEDED(selected_adapter->QueryInterface(__uuidof(IDXGIAdapter2), (void**)&adapter2)))
        {
            DXGI_ADAPTER_DESC2 desc2;
            if (SUCCEEDED(adapter2->GetDesc2(&desc2)))
            {
                dedicated = desc2.DedicatedVideoMemory;
                shared = desc2.SharedSystemMemory;
            }
            adapter2->Release();
        }
    }

    selected_adapter->Release();

    FreeLibrary(hDxgi);  // Free only after all interfaces are released

    // Safety caps
    memory = std::max(std::max(budget, dedicated), _2048mb);
    memory = std::min(memory, total_system_ram / 2);

    return memory;
}

SafetyHookInline shgetAvailableVidMem = {};
int64_t getAvailableVidMem()
{
    auto ret = shgetAvailableVidMem.call<int64_t>();
    if (ret < static_cast<int64_t>(_2048mb))
    {
        static auto vram = GetProcessPreferredGPUMemory();
        static auto d3d9vram = static_cast<uint64_t>(rage::grcDevice::GetD3DDevice()->GetAvailableTextureMem());
        static auto totalVRAM = std::max(std::max(vram, d3d9vram), _2048mb);
        ret = static_cast<int64_t>(totalVRAM);
    }
    return ret;
}

class ExtraStreamingMemory
{
private:
    static constexpr size_t MIN_MEMORY_THRESHOLD = 2LL * 1024 * 1024 * 1024; // 2GB
    static constexpr size_t MAX_MEMORY_THRESHOLD = 2900LL * 1024 * 1024;     // 2.9GB
    static constexpr float MAX_MULTIPLIER = 1.5f;
    static constexpr float MIN_MULTIPLIER = 0.9f;

public:
    static float GetDynamicMultiplier()
    {
        PROCESS_MEMORY_COUNTERS pmc;
        if (!GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc)))
        {
            return MIN_MULTIPLIER; // Fallback to safe value
        }

        size_t currentMemory = pmc.WorkingSetSize;

        // Below 2GB - use maximum multiplier (1.5)
        if (currentMemory < MIN_MEMORY_THRESHOLD)
        {
            return MAX_MULTIPLIER;
        }

        // Above 2.9GB - use minimum multiplier (0.9)
        if (currentMemory >= MAX_MEMORY_THRESHOLD)
        {
            return MIN_MULTIPLIER;
        }

        // Between 2GB and 2.9GB - linear interpolation
        float memoryRange = (float)(MAX_MEMORY_THRESHOLD - MIN_MEMORY_THRESHOLD);
        float currentRange = (float)(currentMemory - MIN_MEMORY_THRESHOLD);
        float ratio = currentRange / memoryRange; // 0.0 to 1.0

        // Linear interpolation: 1.5 at 2GB -> 0.9 at 2.9GB
        return MAX_MULTIPLIER - (ratio * (MAX_MULTIPLIER - MIN_MULTIPLIER));
    }
};

static bool bExtraStreamingMemory = false;
static SafetyHookInline streamingBudgetHook = {};
static unsigned int __fastcall CalculateStreamingMemoryBudget(void* _this, void* edx, float a2)
{
    if (bExtraStreamingMemory)
        a2 = ExtraStreamingMemory::GetDynamicMultiplier();

    nStreamingMemory = streamingBudgetHook.unsafe_fastcall<unsigned int>(_this, edx, a2);
    return nStreamingMemory;
}

bool bEnableHighDetailReflections = false;
injector::hook_back<void(__cdecl*)(int, int16_t, int)> hbsub_B1DEE0;
void __cdecl sub_B1DEE0(int a1, int16_t a2, int a3)
{
    if (bEnableHighDetailReflections)
    {
        for (int i = 0; i <= 11; ++i)
            hbsub_B1DEE0.fun(i, 285, 0);
        return;
    }
    return hbsub_B1DEE0.fun(a1, a2, a3);
}

class VRam
{
public:
    VRam()
    {
        FusionFix::onInitEvent() += []() {
            CIniReader iniReader("");
            bExtraStreamingMemory = iniReader.ReadInteger("EXPERIMENTAL", "ExtraStreamingMemory", 0) != 0;
            bEnableHighDetailReflections = iniReader.ReadInteger("EXPERIMENTAL", "EnableHighDetailReflections", 0) != 0;
            bool bRemoveBBoxCulling = iniReader.ReadInteger("EXPERIMENTAL", "RemoveBBoxCulling", 0) != 0;

            auto pattern = find_pattern("8B 0D ? ? ? ? 83 EC ? 33 C0", "A1 ? ? ? ? 8B 08 83 EC ? 56 57");
            if (!pattern.empty())
            {
                shgetAvailableVidMem = safetyhook::create_inline(pattern.get_first(), getAvailableVidMem);
            }

            pattern = hook::pattern("83 3D ? ? ? ? ? 0F 85 ? ? ? ? A1 ? ? ? ? 85 C0 74");
            if (!pattern.empty())
            {
                static int* g_cmdarg_nomemrestrict = *pattern.get_first<int*>(2);
                static auto loc_427FC9 = resolve_next_displacement(pattern.get_first(0)).value();
                struct NoMemRestrictHook
                {
                    void operator()(injector::reg_pack& regs)
                    {
                        if (bExtraStreamingMemory)
                        {
                            return;
                        }
                        else
                        {
                            return_to(loc_427FC9);
                        }

                        // Original code for reference
                        if (*g_cmdarg_nomemrestrict)
                            return_to(loc_427FC9);
                    }
                }; injector::MakeInline<NoMemRestrictHook>(pattern.get_first(0), pattern.get_first(13));
            }
            else
            {
                pattern = find_pattern("75 7E A1 ? ? ? ? 85 C0");
                if (!pattern.empty())
                    injector::WriteMemory<uint8_t>(pattern.get_first(0), 0xEB, true); // jnz -> jmp
            }

            if (bExtraStreamingMemory)
            {
                pattern = find_pattern("55 8B EC 83 E4 ? F3 0F 10 55 ? 83 EC");
                if (!pattern.empty())
                    streamingBudgetHook = safetyhook::create_inline(pattern.get_first(0), CalculateStreamingMemoryBudget);
            }

            if (bEnableHighDetailReflections)
            {
                auto pattern = find_pattern("FF B6 ? ? ? ? E8 ? ? ? ? 6A ? 6A ? E8 ? ? ? ? 83 C4 ? 85 C0 74 ? 6A ? 6A", "68 ? ? ? ? 50 E8 ? ? ? ? 6A ? 6A ? E8 ? ? ? ? 83 C4 ? 85 C0 74 ? 6A");
                if (!pattern.empty())
                {
                    hbsub_B1DEE0.fun = injector::MakeCALL(pattern.get_first(6), sub_B1DEE0, true).get();

                    if (bRemoveBBoxCulling)
                    {
                        auto pattern = hook::pattern("0F 85 ? ? ? ? E8 ? ? ? ? 80 3D ? ? ? ? ? 8B 75");
                        if (!pattern.empty())
                            injector::WriteMemory<uint16_t>(pattern.get_first(0), 0xE990, true);
                    }
                }
            }
        };
    }
} VRam;