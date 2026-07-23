module;

#include <common.hxx>
#include <dxgi1_4.h>
#include <psapi.h>

export module memory;

import common;
import comvars;

constexpr uint64_t _2048mb = 2147483648ull;

uint64_t GetProcessPreferredGPUMemory()
{
    uint64_t Memory = _2048mb;

    IDirect3DDevice9* Device = rage::grcDevice::GetD3DDevice();
    if (!Device)
        return Memory;

    // Get d3d9 adapter details
    D3DDEVICE_CREATION_PARAMETERS DeviceCreationParams;
    if (FAILED(Device->GetCreationParameters(&DeviceCreationParams)))
        return Memory;

    IDirect3D9* d3d9 = nullptr;
    if (FAILED(Device->GetDirect3D(&d3d9)) || !d3d9)
        return Memory;

    D3DADAPTER_IDENTIFIER9 AdapterIdentifier;
    if (FAILED(d3d9->GetAdapterIdentifier(DeviceCreationParams.AdapterOrdinal, 0, &AdapterIdentifier)))
    {
        d3d9->Release();

        return Memory;
    }
    d3d9->Release();

    HMODULE hDxgi = LoadLibraryA("dxgi.dll");
    if (!hDxgi)
        return Memory;

    // Helper to get system memory
    MEMORYSTATUSEX MemoryInfo;
    MemoryInfo.dwLength = sizeof(MEMORYSTATUSEX);
    GlobalMemoryStatusEx(&MemoryInfo);
    uint64_t TotalSystemMemory = MemoryInfo.ullTotalPhys;

    IDXGIAdapter* SelectedAdapter = nullptr;
    uint64_t MaxDedicated = 0;

    auto EnumerateAdapters = [&](IDXGIFactory* Factory) -> bool
    {
        if (!Factory)
            return false;

        UINT AdapterID = 0;
        IDXGIAdapter* Adapter = nullptr;

        while (SUCCEEDED(Factory->EnumAdapters(AdapterID, &Adapter)))
        {
            DXGI_ADAPTER_DESC Description;

            if (SUCCEEDED(Adapter->GetDesc(&Description)))
            {
                // Match with d3d9 identifier
                if (Description.VendorId == AdapterIdentifier.VendorId &&
                    Description.DeviceId == AdapterIdentifier.DeviceId &&
                    Description.SubSysId == AdapterIdentifier.SubSysId &&
                    Description.Revision == AdapterIdentifier.Revision)
                {
                    if (SelectedAdapter)
                        SelectedAdapter->Release();

                    SelectedAdapter = Adapter;
                    SelectedAdapter->AddRef();
                    Adapter->Release(); // Release the enum ref

                    return true; // Found match, done
                }

                // Track max dedicated as fallback
                if (Description.DedicatedVideoMemory > MaxDedicated)
                {
                    MaxDedicated = Description.DedicatedVideoMemory;

                    if (SelectedAdapter)
                        SelectedAdapter->Release();

                    SelectedAdapter = Adapter;
                    SelectedAdapter->AddRef();
                }
            }

            Adapter->Release();
            AdapterID++;
        }

        return false; // No match found
    };

    // Prefer CreateDXGIFactory2 with IDXGIFactory3
    IDXGIFactory* FactoryToUse = nullptr;
    typedef HRESULT(WINAPI* CreateDXGIFactory2_T)(UINT Flags, REFIID riid, void** ppFactory);
    CreateDXGIFactory2_T CreateDXGIFactory2_fun = (CreateDXGIFactory2_T)GetProcAddress(hDxgi, "CreateDXGIFactory2");

    if (CreateDXGIFactory2_fun)
    {
        IDXGIFactory3* Factory3 = nullptr;

        if (SUCCEEDED(CreateDXGIFactory2_fun(0, __uuidof(IDXGIFactory3), (void**)&Factory3)))
        {
            FactoryToUse = static_cast<IDXGIFactory*>(Factory3);
        }
    }

    // Fallback to CreateDXGIFactory if Factory2 not available
    if (!FactoryToUse)
    {
        typedef HRESULT(WINAPI* CreateDXGIFactory_T)(REFIID riid, void** ppFactory);
        CreateDXGIFactory_T CreateDXGIFactory_fun = (CreateDXGIFactory_T)GetProcAddress(hDxgi, "CreateDXGIFactory");

        if (CreateDXGIFactory_fun)
        {
            IDXGIFactory* Factory = nullptr;

            if (SUCCEEDED(CreateDXGIFactory_fun(__uuidof(IDXGIFactory), (void**)&Factory)))
            {
                FactoryToUse = Factory;
            }
        }
    }

    // Enumerate using the preferred factory
    if (FactoryToUse)
    {
        EnumerateAdapters(FactoryToUse);

        FactoryToUse->Release();
    }

    if (!SelectedAdapter)
    {
        FreeLibrary(hDxgi);

        return Memory;
    }

    // Query the selected adapter
    uint64_t Dedicated = 0;
    uint64_t Shared = 0;
    uint64_t Budget = 0;

    DXGI_ADAPTER_DESC Description;
    if (SUCCEEDED(SelectedAdapter->GetDesc(&Description)))
    {
        Dedicated = Description.DedicatedVideoMemory;
        Shared = Description.SharedSystemMemory;
    }

    IDXGIAdapter3* Adapter3 = nullptr;
    if (SUCCEEDED(SelectedAdapter->QueryInterface(__uuidof(IDXGIAdapter3), (void**)&Adapter3)))
    {
        DXGI_QUERY_VIDEO_MEMORY_INFO VRAMInfo;

        if (SUCCEEDED(Adapter3->QueryVideoMemoryInfo(0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, &VRAMInfo)))
        {
            Budget = VRAMInfo.Budget;
        }

        Adapter3->Release();
    }
    else
    {
        IDXGIAdapter2* Adapter2 = nullptr;

        if (SUCCEEDED(SelectedAdapter->QueryInterface(__uuidof(IDXGIAdapter2), (void**)&Adapter2)))
        {
            DXGI_ADAPTER_DESC2 Description2;

            if (SUCCEEDED(Adapter2->GetDesc2(&Description2)))
            {
                Dedicated = Description2.DedicatedVideoMemory;
                Shared = Description2.SharedSystemMemory;
            }

            Adapter2->Release();
        }
    }

    SelectedAdapter->Release();

    FreeLibrary(hDxgi); // Free only after all interfaces are released

    // Safety caps
    Memory = std::max(std::max(Budget, Dedicated), _2048mb);
    Memory = std::min(Memory, TotalSystemMemory / 2);

    return Memory;
}

namespace grcDevice
{
    SafetyHookInline shGetAvailableVideoMemory = {};
    uint64_t GetAvailableVideoMemory()
    {
        auto ret = shGetAvailableVideoMemory.call<uint64_t>();

        if (ret < _2048mb)
        {
            static auto VRAM = GetProcessPreferredGPUMemory();
            static auto d3d9VRAM = static_cast<uint64_t>(rage::grcDevice::GetD3DDevice()->GetAvailableTextureMem());
            static auto TotalVRAM = std::max(std::max(VRAM, d3d9VRAM), _2048mb);

            ret = TotalVRAM;
        }

        return ret;
    }
}

bool bExtraStreamingMemory = false;

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
        PROCESS_MEMORY_COUNTERS ProcessMemoryCounters;
        if (!GetProcessMemoryInfo(GetCurrentProcess(), &ProcessMemoryCounters, sizeof(ProcessMemoryCounters)))
        {
            return MIN_MULTIPLIER; // Fallback to safe value
        }

        size_t CurrentMemory = ProcessMemoryCounters.WorkingSetSize;

        // Below 2GB - use maximum multiplier (1.5)
        if (CurrentMemory < MIN_MEMORY_THRESHOLD)
        {
            return MAX_MULTIPLIER;
        }

        // Above 2.9GB - use minimum multiplier (0.9)
        if (CurrentMemory >= MAX_MEMORY_THRESHOLD)
        {
            return MIN_MULTIPLIER;
        }

        // Between 2GB and 2.9GB - linear interpolation
        float MemoryRange = (float)(MAX_MEMORY_THRESHOLD - MIN_MEMORY_THRESHOLD);
        float CurrentRange = (float)(CurrentMemory - MIN_MEMORY_THRESHOLD);
        float Ratio = CurrentRange / MemoryRange; // 0.0 to 1.0

        // Linear interpolation: 1.5 at 2GB -> 0.9 at 2.9GB
        return MAX_MULTIPLIER - (Ratio * (MAX_MULTIPLIER - MIN_MULTIPLIER));
    }
};

namespace grcResourceCache
{
    SafetyHookInline shGetAvailableFreeMemory = {};
    int64_t __fastcall GetAvailableFreeMemory(void* _this, void* edx, float Multiplier)
    {
        if (bExtraStreamingMemory)
        {
            Multiplier = ExtraStreamingMemory::GetDynamicMultiplier();
        }

        return shGetAvailableFreeMemory.unsafe_fastcall<int64_t>(_this, edx, Multiplier);
    }
}

class Memory
{
public:
    Memory()
    {
        FusionFix::onInitEvent() += []()
        {
            CIniReader iniReader("");

            // [EXPERIMENTAL]
            bExtraStreamingMemory = iniReader.ReadInteger("EXPERIMENTAL", "ExtraStreamingMemory", 0) != 0;

            auto pattern = hook::pattern("8B 0D ? ? ? ? 83 EC ? 33 C0");
            grcDevice::shGetAvailableVideoMemory = safetyhook::create_inline(pattern.get_first(0), grcDevice::GetAvailableVideoMemory);

            if (bExtraStreamingMemory)
            {
                pattern = find_pattern("55 8B EC 83 E4 ? F3 0F 10 55 ? 83 EC", "55 8B EC 83 E4 ? D9 45 ? 83 EC ? 53 56 57 51 D9 1C 24 8B D9");
                grcResourceCache::shGetAvailableFreeMemory = safetyhook::create_inline(pattern.get_first(0), grcResourceCache::GetAvailableFreeMemory);
            }

            // Force the -nomemrestrict commandline arg. Fixes delayed streaming in some circumstances although not when Texture Quality is either Medium or Low.
            pattern = hook::pattern("83 3D ? ? ? ? ? 0F 85 ? ? ? ? A1 ? ? ? ? 85 C0 74");
            if (!pattern.empty())
            {
                //static auto g_cmdarg_nomemrestrict = *pattern.get_first<int32_t*>(2);
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
                        //if (*g_cmdarg_nomemrestrict)
                        //{
                        //    return_to(loc_427FC9);
                        //}
                    }
                }; injector::MakeInline<NoMemRestrictHook>(pattern.get_first(0), pattern.get_first(13));
            }
            else
            {
                pattern = hook::pattern("75 ? A1 ? ? ? ? 85 C0 74 ? 80 38");
                injector::WriteMemory<uint8_t>(pattern.get_first(0), 0xEB, true); // jnz --> jmp
            }
        };
    }
} Memory;