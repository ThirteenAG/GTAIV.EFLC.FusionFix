module;

#include <common.hxx>
#include <dxgi1_6.h>
#include <psapi.h>

export module vram;

import common;
import comvars;
import settings;

export unsigned int nStreamingMemory = 0;

constexpr SIZE_T _2048mb = 2147483648u;

SIZE_T GetProcessPreferredGPUMemory()
{
    SIZE_T memory = _2048mb;

    typedef HRESULT(WINAPI* CreateDXGIFactory2_t)(UINT Flags, REFIID riid, void** ppFactory);
    typedef HRESULT(WINAPI* CreateDXGIFactory_t)(REFIID riid, void** ppFactory);

    HMODULE hDxgi = LoadLibraryA("dxgi.dll");
    if (!hDxgi)
        return memory;

    IDXGIAdapter1* adapter = nullptr;

    // Try CreateDXGIFactory2 with IDXGIFactory6 (Windows 10 1803+)
    CreateDXGIFactory2_t CreateDXGIFactory2_fn = (CreateDXGIFactory2_t)GetProcAddress(hDxgi, "CreateDXGIFactory2");

    if (CreateDXGIFactory2_fn)
    {
        IDXGIFactory6* factory6 = nullptr;
        if (SUCCEEDED(CreateDXGIFactory2_fn(0, __uuidof(IDXGIFactory6), (void**)&factory6)))
        {
            if (SUCCEEDED(factory6->EnumAdapterByGpuPreference(0, DXGI_GPU_PREFERENCE_UNSPECIFIED, __uuidof(IDXGIAdapter1), (void**)&adapter)))
            {
                DXGI_ADAPTER_DESC1 desc;
                if (SUCCEEDED(adapter->GetDesc1(&desc)))
                {
                    memory = desc.DedicatedVideoMemory;
                }
                adapter->Release();
            }
            factory6->Release();
            FreeLibrary(hDxgi);
            return memory;
        }
    }

    // Fallback to regular factory if Factory6 not available
    CreateDXGIFactory_t CreateDXGIFactory_fn = (CreateDXGIFactory_t)GetProcAddress(hDxgi, "CreateDXGIFactory");

    if (CreateDXGIFactory_fn)
    {
        IDXGIFactory* factory = nullptr;
        if (SUCCEEDED(CreateDXGIFactory_fn(__uuidof(IDXGIFactory), (void**)&factory)))
        {
            IDXGIAdapter* basicAdapter = nullptr;
            if (SUCCEEDED(factory->EnumAdapters(0, &basicAdapter)))
            {
                DXGI_ADAPTER_DESC desc;
                if (SUCCEEDED(basicAdapter->GetDesc(&desc)))
                {
                    memory = desc.DedicatedVideoMemory;
                }
                basicAdapter->Release();
            }
            factory->Release();
        }
    }

    FreeLibrary(hDxgi);
    return memory;
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
        FusionFix::onInitEvent() += []()
        {
            CIniReader iniReader("");
            bExtraStreamingMemory = iniReader.ReadInteger("EXPERIMENTAL", "ExtraStreamingMemory", 0) != 0;
            bEnableHighDetailReflections = iniReader.ReadInteger("EXPERIMENTAL", "EnableHighDetailReflections", 0) != 0;
            bool bRemoveBBoxCulling = iniReader.ReadInteger("EXPERIMENTAL", "RemoveBBoxCulling", 0) != 0;

            auto pattern = find_pattern("B8 ? ? ? ? 33 D2 83 C4");
            if (!pattern.empty())
            {
                injector::MakeNOP(pattern.get_first(0), 5, true);
                static auto DefaultVRAMHook = safetyhook::create_mid(pattern.get_first(0), [](SafetyHookContext& regs)
                {
                    static auto vram = GetProcessPreferredGPUMemory();
                    static auto d3d9vram = rage::grcDevice::GetD3DDevice()->GetAvailableTextureMem();
                    static auto totalVRAM = max(max(vram, d3d9vram), _2048mb);
                    regs.eax = totalVRAM;
                });
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
                auto pattern = find_pattern("FF B6 ? ? ? ? E8 ? ? ? ? 6A ? 6A ? E8 ? ? ? ? 83 C4 ? 85 C0 74 ? 6A ? 6A", "E8 ? ? ? ? 6A ? 6A ? E8 ? ? ? ? 83 C4 ? 85 C0 74 ? 6A ? 6A ? 8B C8 E8 ? ? ? ? EB ? 33 C0 8B C8 E8 ? ? ? ? 6A ? 6A ? E8 ? ? ? ? 83 C4 ? 85 C0 74 ? 55");
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