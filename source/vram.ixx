module;

#include <common.hxx>
#include <dxgi1_6.h>

export module vram;

import common;
import comvars;

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

class VRam
{
public:
    VRam()
    {
        FusionFix::onInitEvent() += []()
        {
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
        };
    }
} VRam;