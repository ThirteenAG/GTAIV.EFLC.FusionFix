module;

#include <common.hxx>
#include <dxgi1_4.h>
#include <span>
#include <unordered_map>
#include <intrin.h>
#include <upscaler/graphics_api.h>

export module memory;

import common;
import comvars;
import settings;

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
    uint64_t GetTotalVideoMemory()
    {
        static auto VRAM = GetProcessPreferredGPUMemory();
        static auto d3d9VRAM = static_cast<uint64_t>(rage::grcDevice::GetD3DDevice()->GetAvailableTextureMem());
        static auto TotalVRAM = std::max(std::max(VRAM, d3d9VRAM), _2048mb);
        return TotalVRAM;
    }

    // The video memory the game sizes its texture budget against, in MB
    std::atomic<uint32_t> nVideoMemoryMB{ 2048 };

    SafetyHookInline shGetAvailableVideoMemory = {};
    uint64_t GetAvailableVideoMemory()
    {
        auto ret = shGetAvailableVideoMemory.call<uint64_t>();

        if (ret < _2048mb)
            ret = GetTotalVideoMemory();

        nVideoMemoryMB = static_cast<uint32_t>(ret >> 20);
        return ret;
    }

    // 1.0.7.0, 1.1.2.0: a 32-bit value that some callers convert as signed
    uint32_t GetAvailableVideoMemory32()
    {
        auto ret = std::max<uint64_t>(shGetAvailableVideoMemory.call<uint32_t>(), GetTotalVideoMemory());
        ret = std::min<uint64_t>(ret, INT32_MAX);
        nVideoMemoryMB = static_cast<uint32_t>(ret >> 20);
        return static_cast<uint32_t>(ret);
    }
}

// GTA IV is a 32-bit process, and what runs out first is its address space, not video memory. The game keeps a copy of
// every texture it streams in system memory (to make the texture again when its video memory is taken back or the
// device is reset), so the texture budget of the streaming is paid for in address space with every graphics API. The
// game caps the budget by free virtual memory only on PCs with less than 2.5 GB of RAM, so on any PC today a heavy area
// or a big texture mod uses the address space up until an allocation fails. The budget follows the room in the address
// space instead, measured on a thread of its own: what is free plus what is reserved but unused (memory the game has
// given back stays reserved by its heap and is used again), and the largest free block (a big texture needs one
// piece). It is cut quickly when room gets short and raised slowly after a while of plenty, and only a little: address
// space taken once is rarely given back to the system. Cars and people get more variety only when the world budget is
// at its maximum and there is still room, and they give it back first.
namespace StreamingBudget
{
    struct Limits
    {
        uint32_t nCriticalRoom, nCriticalLargest;   // MB: cut hard below either
        uint32_t nTightRoom, nTightLargest;         // cut below either
        uint32_t nRaiseRoom, nRaiseLargest;         // raise above both
        uint32_t nCrowdRoom, nCrowdLargest;         // more car and ped variety above both
        float fMaxScale;
    };

    // Direct3D 9 keeps a wider margin: a device reset allocates again. DirectX 12 and Vulkan start with less room (their
    // drivers map more into the process), so their limits are lower and the same steps apply.
    constexpr Limits Direct3D9Limits = { 500, 96, 800, 160, 1300, 384, 1600, 448, 1.15f };
    constexpr Limits OtherLimits = { 400, 80, 700, 128, 1200, 320, 1500, 384, 1.15f };

    constexpr float fMinScale = 0.6f;
    constexpr float fRaiseStep = 0.025f;
    constexpr float fCutStep = 0.05f;
    constexpr float fCriticalCutStep = 0.15f;
    constexpr float fCrowdStep = 0.05f;
    constexpr float fCrowdCutStep = 0.25f;

    // Of the game's free memory multiplier
    std::atomic<float> fScale{ 1.0f };

    // 1.0.7.0, 1.1.2.0: the multiplier is a constant in memory; the code reads this one instead
    float fBaseMultiplier = 0.0f;
    float fMultiplier = 0.0f;

    // More car and ped variety, 0 to 1 (PopulationBudget)
    std::atomic<float> fCrowd{ 0.0f };

    struct AddressSpace
    {
        uint32_t nRoom;
        uint32_t nLargest;
    };

    // In MB: walks the regions of the address space (not their memory), some ten thousand in a running game
    AddressSpace Measure()
    {
        static auto SystemInfo = []()
        {
            SYSTEM_INFO si;
            GetSystemInfo(&si);
            return si;
        }();

        auto addr = reinterpret_cast<uintptr_t>(SystemInfo.lpMinimumApplicationAddress);
        auto end = reinterpret_cast<uintptr_t>(SystemInfo.lpMaximumApplicationAddress);
        uint64_t room = 0;
        uint64_t largest = 0;
        MEMORY_BASIC_INFORMATION mbi;
        while (addr < end && VirtualQuery(reinterpret_cast<void*>(addr), &mbi, sizeof(mbi)))
        {
            if (mbi.State == MEM_FREE)
                largest = std::max<uint64_t>(largest, mbi.RegionSize);
            if (mbi.State != MEM_COMMIT)
                room += mbi.RegionSize;

            auto next = reinterpret_cast<uintptr_t>(mbi.BaseAddress) + mbi.RegionSize;
            if (next <= addr)
                break;
            addr = next;
        }
        return { static_cast<uint32_t>(room >> 20), static_cast<uint32_t>(largest >> 20) };
    }

    void Run()
    {
        SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);
        const auto& limits = FusionFixGraphicsApi::Running() == FusionFixGraphicsApi::Api::Direct3D9 ? Direct3D9Limits : OtherLimits;

        float scale = 1.0f;
        float crowd = 0.0f;
        uint64_t lastChange = 0;
        uint64_t lastCut = GetTickCount64();
        uint64_t roomSince = 0;
        bool watch = true;
        uint64_t lastWalk = 0;
        uint32_t walkFree = 0;
        AddressSpace walked = {};

        while (true)
        {
            Sleep(250);

            // What is free comes from the system's counters; the walk for the room and the largest block every 2 s, every 0.5 s
            // while room is short, and at once when 128 MB or more were taken since the last one. In between, the room
            // shrinks by what was taken.
            auto now = GetTickCount64();
            MEMORYSTATUSEX status = { sizeof(status) };
            GlobalMemoryStatusEx(&status);
            auto free = static_cast<uint32_t>(status.ullAvailVirtual >> 20);
            if (!lastWalk || now - lastWalk >= (watch ? 500u : 2000u) || walkFree > free + 128)
            {
                walked = Measure();
                walkFree = free;
                lastWalk = now;
            }
            AddressSpace space = { walked.nRoom - std::min(walked.nRoom, walkFree > free ? walkFree - free : 0), walked.nLargest };

            // Within about three quarters of the video memory
            auto maxScale = std::clamp(grcDevice::nVideoMemoryMB * 0.00075f, 1.0f, limits.fMaxScale);

            auto critical = space.nRoom < limits.nCriticalRoom || space.nLargest < limits.nCriticalLargest;
            auto tight = critical || space.nRoom < limits.nTightRoom || space.nLargest < limits.nTightLargest;
            auto raise = space.nRoom > limits.nRaiseRoom && space.nLargest > limits.nRaiseLargest;
            watch = !raise;

            auto newScale = std::min(scale, maxScale);
            auto newCrowd = crowd;

            if (tight)
            {
                roomSince = 0;

                // A cut takes a moment to show in the address space
                if (now - lastChange >= 1000)
                {
                    if (critical)
                    {
                        newCrowd = 0.0f;
                        newScale = std::max(fMinScale, newScale - fCriticalCutStep);
                    }
                    else if (crowd > 0.0f)
                        newCrowd = std::max(0.0f, crowd - fCrowdCutStep);
                    else
                        newScale = std::max(fMinScale, newScale - fCutStep);
                    lastCut = now;
                }
            }
            else if (raise)
            {
                if (!roomSince)
                    roomSince = now;

                // Back up to the game's own budget soon after a cut, above it only after a long while of room
                auto wait = newScale < 1.0f ? 5000 : 30000;
                if (now - roomSince >= 5000 && now - lastCut >= wait && now - lastChange >= 2000)
                {
                    if (newScale < maxScale)
                        newScale = std::min(maxScale, newScale + fRaiseStep);
                    else if (space.nRoom > limits.nCrowdRoom && space.nLargest > limits.nCrowdLargest)
                        newCrowd = std::min(1.0f, crowd + fCrowdStep);
                }
            }
            else
                roomSince = 0;

            if (newScale != scale)
            {
                scale = newScale;
                fScale = scale;
                if (fBaseMultiplier)
                    fMultiplier = fBaseMultiplier * scale;
                lastChange = now;
            }

            if (newCrowd != crowd)
            {
                crowd = newCrowd;
                fCrowd = crowd;
                lastChange = now;
            }
        }
    }
}

namespace grcResourceCache
{
    SafetyHookInline shGetAvailableFreeMemory = {};
    int64_t __fastcall GetAvailableFreeMemory(void* _this, void* edx, float Multiplier)
    {
        return shGetAvailableFreeMemory.unsafe_fastcall<int64_t>(_this, edx, Multiplier * StreamingBudget::fScale.load(std::memory_order_relaxed));
    }

    // The cache keeps the resources it has to forget in an array of 65536 entries: a texture, a vertex or an index buffer
    // adds itself when it is destroyed, and the cache empties the array the next time it makes room for a new resource.
    // Nothing checks that the array is full, so a big unload with nothing created in between (a large area left behind,
    // quitting with a big texture mod) writes past it, into its entry count first. What doesn't fit waits in a list of
    // its own and goes into the array as far as it fits when the cache starts emptying it. Both are only used under the
    // cache's lock.
    constexpr uint32_t nReleaseQueueSize = 0x10000;
    constexpr ptrdiff_t nReleaseQueueOffset = 0x148;
    constexpr ptrdiff_t nReleaseCountOffset = nReleaseQueueOffset + nReleaseQueueSize * sizeof(uint32_t);

    std::vector<uint32_t> ReleaseOverflow;
    uint32_t nReleaseOverflowSlot = 0;

    uint32_t* QueueRelease(uintptr_t cache, uint32_t resource)
    {
        auto& count = *reinterpret_cast<uint32_t*>(cache + nReleaseCountOffset);
        auto queue = reinterpret_cast<uint32_t*>(cache + nReleaseQueueOffset);
        if (count < nReleaseQueueSize)
        {
            queue[count] = resource;
            return &queue[count++];
        }

        ReleaseOverflow.push_back(resource);
        return &nReleaseOverflowSlot;
    }

    void RefillReleaseQueue(uintptr_t cache)
    {
        auto& count = *reinterpret_cast<uint32_t*>(cache + nReleaseCountOffset);
        if (ReleaseOverflow.empty() || count >= nReleaseQueueSize)
            return;

        auto n = std::min<size_t>(ReleaseOverflow.size(), nReleaseQueueSize - count);
        std::copy(ReleaseOverflow.end() - n, ReleaseOverflow.end(), reinterpret_cast<uint32_t*>(cache + nReleaseQueueOffset) + count);
        ReleaseOverflow.resize(ReleaseOverflow.size() - n);
        count += static_cast<uint32_t>(n);
    }

    // The copies of the add inlined into the destructors use the cache by its address
    uintptr_t nCache = 0;

    template<uintptr_t SafetyHookContext::* Resource>
    struct QueueReleaseInline
    {
        void operator()(injector::reg_pack& regs)
        {
            auto slot = QueueRelease(nCache, static_cast<uint32_t>(regs.*Resource));
            regs.eax = *reinterpret_cast<uint32_t*>(nCache + nReleaseCountOffset);
            regs.ecx = reinterpret_cast<uintptr_t>(slot);
        }
    };
}

// Entities with a drawable take a link from a pool and give it back when the drawable goes; one lock guards the pool and
// the streaming cleanup list the links are kept in. When the pool is empty, or the cleanup list is full, the function
// that takes a link deletes the drawable of an entity far away to make room while it holds the lock, and that entity
// gives its link back through the function that takes the same lock: the thread waits for itself and the game freezes.
// The lock becomes one its owner can take again. At both places the take nests the release, the pool and the list are
// consistent (nothing taken yet, or the new link already off the free list), so the release does what the game meant.
namespace EntityLinkPool
{
    CRITICAL_SECTION Lock;

    SafetyHookInline shTakeLink = {};
    void* __fastcall TakeLink(void* pool, void* edx, void* entity, void* drawable)
    {
        EnterCriticalSection(&Lock);
        auto link = shTakeLink.unsafe_fastcall<void*>(pool, edx, entity, drawable);
        LeaveCriticalSection(&Lock);
        return link;
    }

    SafetyHookInline shReleaseLink = {};
    void* __fastcall ReleaseLink(void* pool, void* edx, void* link)
    {
        EnterCriticalSection(&Lock);
        auto ret = shReleaseLink.unsafe_fastcall<void*>(pool, edx, link);
        LeaveCriticalSection(&Lock);
        return ret;
    }
}

// Unless started with -managed, the game keeps the pixels of every texture it loads in system memory (texture + 0x48):
// the Direct3D texture is made from that copy when the texture is first drawn, and again whenever the resource cache
// has taken its video memory back or the device was reset. The copy is written once and read only at those moments,
// yet it holds address space for as long as the texture lives, with every graphics API. Bigger copies live in sections
// of the page file instead, mapped into the address space only while the game reads or writes them: when the texture is
// loaded, when it is (re)made in video memory, between a lock and its unlock, and in the two functions that write the
// copy without a device. While a copy is not mapped the texture holds a small odd number in its place (an id; the first
// 64 KB of the address space are never mapped, so it can't be a pointer).
namespace TextureCopies
{
    constexpr uint32_t nMinSize = 0x10000;
    constexpr uint32_t nMaxCopies = 0x8000;

    struct Copy
    {
        HANDLE hSection;
        uint32_t nSize;
        uint8_t* pView;
        uint32_t nMapped;
    };

    SRWLOCK Lock = SRWLOCK_INIT;
    Copy Copies[nMaxCopies] = {};
    std::vector<uint16_t> FreeIds;
    std::unordered_map<uintptr_t, uint16_t> MappedViews;
    thread_local uint16_t nLastAlloc = 0;

    constexpr ptrdiff_t CopyOffset = 0x48;
    constexpr ptrdiff_t FlagsOffset = 0x26;     // 0x80: make the Direct3D texture from the copy

    uintptr_t& CopyOf(uintptr_t texture)
    {
        return *reinterpret_cast<uintptr_t*>(texture + CopyOffset);
    }

    bool IsId(uintptr_t value)
    {
        return (value & 1) && value < nMaxCopies * 2;
    }

    // Under the lock
    void Map(uintptr_t texture)
    {
        auto& copy = CopyOf(texture);
        if (!IsId(copy))
        {
            auto it = MappedViews.find(copy);
            if (it != MappedViews.end())
                Copies[it->second].nMapped++;
            return;
        }

        auto id = static_cast<uint16_t>(copy >> 1);
        auto& c = Copies[id];
        if (c.pView)
        {
            c.nMapped++;
            copy = uintptr_t(c.pView);
            return;
        }
        c.pView = static_cast<uint8_t*>(MapViewOfFile(c.hSection, FILE_MAP_WRITE, 0, 0, c.nSize));
        if (!c.pView)
            throw std::runtime_error("TextureCopies: a view of a texture's copy can't be mapped");
        c.nMapped = 1;
        MappedViews[uintptr_t(c.pView)] = id;
        copy = uintptr_t(c.pView);
    }

    void Unmap(uintptr_t texture)
    {
        auto& copy = CopyOf(texture);
        auto it = MappedViews.find(copy);
        if (it == MappedViews.end())
            return;

        auto id = it->second;
        auto& c = Copies[id];
        if (--c.nMapped)
            return;
        MappedViews.erase(it);
        UnmapViewOfFile(c.pView);
        c.pView = nullptr;
        copy = (uintptr_t(id) << 1) | 1;
    }

    // Under the lock; returns the copy's size, to count once the lock is left
    uint32_t Release(uint16_t id)
    {
        auto& c = Copies[id];
        if (c.pView)
        {
            MappedViews.erase(uintptr_t(c.pView));
            UnmapViewOfFile(c.pView);
        }
        CloseHandle(c.hSection);
        auto size = c.nSize;
        c = {};
        FreeIds.push_back(id);
        return size;
    }

    // The cache counts the system memory of the copies (its allocator's blocks, rounded the same way) in what the streaming
    // has used: the sections are counted there too, or the streaming would load textures past its budget
    uintptr_t nCache = 0;
    constexpr ptrdiff_t SystemMemoryOffset = 0x402C8;
    constexpr ptrdiff_t SystemMemoryLockOffset = 0x40530;

    void CountSystemMemory(int64_t bytes)
    {
        auto lock = reinterpret_cast<CRITICAL_SECTION*>(nCache + SystemMemoryLockOffset);
        auto locked = *reinterpret_cast<uintptr_t*>(lock) != 0;
        if (locked)
            EnterCriticalSection(lock);
        *reinterpret_cast<int64_t*>(nCache + SystemMemoryOffset) += bytes;
        if (locked)
            LeaveCriticalSection(lock);
    }

    int64_t CacheBlock(uint32_t size)
    {
        return (size + 415) & ~0xFFu;
    }

    // The resource cache's allocator, called for a texture's copy: a section and a view of it for bigger ones
    void* AllocCopy(void* cache, uint32_t size, auto&& original)
    {
        if (size >= nMinSize)
        {
            AcquireSRWLockExclusive(&Lock);
            if (!FreeIds.empty())
            {
                auto id = FreeIds.back();
                auto section = CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE | SEC_COMMIT, 0, size, nullptr);
                auto view = section ? static_cast<uint8_t*>(MapViewOfFile(section, FILE_MAP_WRITE, 0, 0, size)) : nullptr;
                if (view)
                {
                    FreeIds.pop_back();
                    Copies[id] = { section, size, view, 1 };
                    MappedViews[uintptr_t(view)] = id;
                    nLastAlloc = id;
                    ReleaseSRWLockExclusive(&Lock);
                    nCache = reinterpret_cast<uintptr_t>(cache);
                    CountSystemMemory(CacheBlock(size));
                    return view;
                }
                if (section)
                    CloseHandle(section);
            }
            ReleaseSRWLockExclusive(&Lock);
        }
        return original();
    }

    // Called by the texture functions only (the vertex and index buffers' calls stay the game's)
    void* (__thiscall* CacheAllocThis)(void*, int32_t, uint32_t) = nullptr;
    void* __fastcall AllocCopyThis(void* cache, void* edx, int32_t type, uint32_t size) // 1.2.x
    {
        return AllocCopy(cache, size, [&]() { return CacheAllocThis(cache, type, size); });
    }

    void* (__stdcall* CacheAllocStd)(void*, int32_t, uint32_t) = nullptr;
    void* __stdcall AllocCopyStd(void* cache, int32_t type, uint32_t size) // 1.0.8.0, 1.1.3.0
    {
        return AllocCopy(cache, size, [&]() { return CacheAllocStd(cache, type, size); });
    }

    // Right after the texture got its copy: unmapped until the texture is drawn
    void Loaded(uintptr_t texture)
    {
        uint32_t released = 0;
        AcquireSRWLockExclusive(&Lock);
        auto last = std::exchange(nLastAlloc, 0);
        if (last)
        {
            if (Copies[last].pView && CopyOf(texture) == uintptr_t(Copies[last].pView))
                Unmap(texture);
            else if (Copies[last].hSection && Copies[last].pView)
                released = Release(last); // the game dropped it (no pixels to copy)
        }
        ReleaseSRWLockExclusive(&Lock);
        if (released)
            CountSystemMemory(-CacheBlock(released));
    }

    // Whether the texture functions write the copy themselves: without a device, or while a resource is being built
    bool NoDevice()
    {
        if (!rage::grcDevice::GetD3DDevice())
            return true;
        // The thread's allocator: the game's TLS block (first slot of the TLS array), + 8
        auto block = *reinterpret_cast<uintptr_t*>(__readfsdword(0x2C));
        auto allocator = *reinterpret_cast<void**>(block + 8);
        return reinterpret_cast<bool(__thiscall*)(void*)>((*reinterpret_cast<void***>(allocator))[15])(allocator);
    }

    // Every user maps and unmaps through the count, so a copy stays mapped while any thread reads it
    template<typename F>
    auto Mapped(uintptr_t texture, bool map, F&& call)
    {
        if (!map)
            return call();
        AcquireSRWLockExclusive(&Lock);
        Map(texture);
        ReleaseSRWLockExclusive(&Lock);
        auto ret = call();
        AcquireSRWLockExclusive(&Lock);
        Unmap(texture);
        ReleaseSRWLockExclusive(&Lock);
        return ret;
    }

    SafetyHookInline shMake = {};
    int __fastcall Make(uintptr_t texture, void* edx)
    {
        // Nothing to make: the game's function would return at once (its callers ignore what it returns); checked once here,
        // so the flag can't be set by another thread between this test and the game's own
        auto flags = *reinterpret_cast<volatile uint8_t*>(texture + FlagsOffset);
        if (!(flags & 0x80))
            return flags;
        return Mapped(texture, true, [&]() { return shMake.unsafe_fastcall<int>(texture, edx); });
    }

    SafetyHookInline shLock = {};
    int __fastcall LockTexture(uintptr_t texture, void* edx, int32_t a, int32_t b, void* c, int32_t d)
    {
        // Mapped until the unlock (the lock hands out a pointer into the copy)
        AcquireSRWLockExclusive(&Lock);
        Map(texture);
        ReleaseSRWLockExclusive(&Lock);
        return shLock.unsafe_fastcall<int>(texture, edx, a, b, c, d);
    }

    SafetyHookInline shUnlock = {};
    int __fastcall UnlockTexture(uintptr_t texture, void* edx, int32_t a)
    {
        auto ret = shUnlock.unsafe_fastcall<int>(texture, edx, a);
        AcquireSRWLockExclusive(&Lock);
        Unmap(texture);
        ReleaseSRWLockExclusive(&Lock);
        return ret;
    }

    SafetyHookInline shUpdate = {};
    int __fastcall Update(uintptr_t texture, void* edx, void* src, int32_t width, int32_t height, int32_t bpp)
    {
        return Mapped(texture, NoDevice(), [&]() { return shUpdate.unsafe_fastcall<int>(texture, edx, src, width, height, bpp); });
    }

    SafetyHookInline shRebuild = {};
    int __fastcall Rebuild(uintptr_t texture, void* edx, void* image)
    {
        return Mapped(texture, NoDevice(), [&]() { return shRebuild.unsafe_fastcall<int>(texture, edx, image); });
    }

    SafetyHookInline shDestroy = {};
    void __fastcall Destroy(uintptr_t texture, void* edx)
    {
        auto& copy = CopyOf(texture);
        uint32_t released = 0;
        AcquireSRWLockExclusive(&Lock);
        if (IsId(copy))
        {
            released = Release(static_cast<uint16_t>(copy >> 1));
            copy = 0;
        }
        else if (auto it = MappedViews.find(copy); it != MappedViews.end())
        {
            released = Release(it->second);
            copy = 0;
        }
        ReleaseSRWLockExclusive(&Lock);
        if (released)
            CountSystemMemory(-CacheBlock(released));
        shDestroy.unsafe_fastcall(texture, edx);
    }

    void Init()
    {
        FreeIds.reserve(nMaxCopies);
        for (uint32_t id = nMaxCopies - 1; id >= 1; id--)
            FreeIds.push_back(static_cast<uint16_t>(id));
    }
}

// The population streaming loads another car or ped model while the memory of the ones it has is under a budget (40 and
// 50 MB). Every model counts, also the taxis, police and emergency models it keeps loaded whatever the budget says, so
// with bigger models (car mods) those alone fill it: no other car model is loaded, models are dropped, and taxis are
// soon the only cars on the streets. The budgets follow the size of the models in memory instead, so they hold about
// as many models as the game's own budgets hold of its own models; with room in the address space a few more
// (StreamingBudget's crowd).
namespace PopulationBudget
{
    // MB per model in memory: the game's own models average about 3.2 (cars) and 4.4 (peds), so with these its own
    // budgets are kept
    constexpr float fStockVehicleModelMB = 3.6f;
    constexpr float fStockPedModelMB = 4.9f;
    constexpr float fMaxModelScale = 4.0f;
    constexpr float fMaxVehicleCrowd = 1.5f;
    constexpr float fMaxPedCrowd = 1.3f;

    GameRef<int32_t> nVehicleBudget;
    GameRef<int32_t> nPedBudget;

    // Loaded model groups of the population streaming: 64 model indices each, -1 after the last
    constexpr ptrdiff_t VehicleGroups[] = { 0x000, 0x100, 0x200, 0x300 };
    constexpr ptrdiff_t PedGroups[] = { 0x404, 0x504, 0x604 };
    constexpr ptrdiff_t VehicleMemory = 0x70C;
    constexpr ptrdiff_t PedMemory = 0x708;

    int32_t CountModels(uintptr_t population, std::span<const ptrdiff_t> groups)
    {
        int32_t count = 0;
        for (auto group : groups)
        {
            auto models = reinterpret_cast<const int32_t*>(population + group);
            for (int32_t i = 0; i < 64 && models[i] >= 0; i++)
                count++;
        }
        return count;
    }

    // How much bigger than the game's own the models in memory are, from at least a few of them
    float ModelScale(uintptr_t population, std::span<const ptrdiff_t> groups, ptrdiff_t memory, float stockMB)
    {
        auto count = CountModels(population, groups);
        auto bytes = *reinterpret_cast<const int32_t*>(population + memory);
        if (count < 3 || bytes <= 0)
            return 1.0f;
        return std::clamp(bytes / (count * stockMB * 1048576.0f), 1.0f, fMaxModelScale);
    }

    void Update(uintptr_t population)
    {
        // The game's own, once it has set them up for the session
        static int32_t nStockVehicleBudget = nVehicleBudget;
        static int32_t nStockPedBudget = nPedBudget;

        auto crowd = StreamingBudget::fCrowd.load(std::memory_order_relaxed);
        auto vehicles = ModelScale(population, VehicleGroups, VehicleMemory, fStockVehicleModelMB) * (1.0f + crowd * (fMaxVehicleCrowd - 1.0f));
        auto peds = ModelScale(population, PedGroups, PedMemory, fStockPedModelMB) * (1.0f + crowd * (fMaxPedCrowd - 1.0f));
        nVehicleBudget = static_cast<int32_t>(nStockVehicleBudget * vehicles);
        nPedBudget = static_cast<int32_t>(nStockPedBudget * peds);
    }
}

class Memory
{
public:
    Memory()
    {
        FusionFix::onInitEvent() += []()
        {
            SettingsReader reader;

            auto pattern = hook::pattern("8B 0D ? ? ? ? 83 EC ? 33 C0");
            if (!pattern.empty()) // 1.0.8.0, 1.1.3.0, 1.2.0.32, 1.2.0.43, 1.2.0.59
            {
                grcDevice::shGetAvailableVideoMemory = safetyhook::create_inline(pattern.get_first(0), grcDevice::GetAvailableVideoMemory);

                pattern = find_pattern("55 8B EC 83 E4 ? F3 0F 10 55 ? 83 EC", "55 8B EC 83 E4 ? D9 45 ? 83 EC ? 53 56 57 51 D9 1C 24 8B D9");
                grcResourceCache::shGetAvailableFreeMemory = safetyhook::create_inline(pattern.get_first(0), grcResourceCache::GetAvailableFreeMemory);
            }
            else // 1.0.7.0, 1.1.2.0
            {
                pattern = hook::pattern("A1 ? ? ? ? 8B 08 83 EC 1C 56 57 8D 54 24 0C 52 50 8B 41 18 FF D0");
                grcDevice::shGetAvailableVideoMemory = safetyhook::create_inline(pattern.get_first(0), grcDevice::GetAvailableVideoMemory32);

                // The streaming's free memory multiplier: fmul dword ptr [0.85]
                pattern = hook::pattern("2B F8 74 ? 85 FF 89 7C 24 14 DB 44 24 14 7D 06 D8 05 ? ? ? ? D8 0D");
                StreamingBudget::fBaseMultiplier = **pattern.get_first<float*>(24);
                StreamingBudget::fMultiplier = StreamingBudget::fBaseMultiplier;
                injector::WriteMemory(pattern.get_first(24), &StreamingBudget::fMultiplier, true);
            }

            std::thread(StreamingBudget::Run).detach();

            // The car and ped budgets of the population streaming
            pattern = find_pattern("F7 2D ? ? ? ? 8B CA C1 E9 1F 03 CA B8 ? ? ? ? F7 2D ? ? ? ? 8B C2 C1 E8 1F 03 C2 89 0D ? ? ? ? A3 ? ? ? ? 83 C4 10 C3", "8B 0D ? ? ? ? B8 ? ? ? ? F7 E9 8B 0D");
            PopulationBudget::nVehicleBudget.SetAddress(*pattern.get_first<int32_t*>(2));
            pattern = find_pattern("F7 2D ? ? ? ? 8B C2 C1 E8 1F 03 C2 89 0D ? ? ? ? A3 ? ? ? ? 83 C4 10 C3", "8B 0D ? ? ? ? 8B C2 C1 E8 1F");
            PopulationBudget::nPedBudget.SetAddress(*pattern.get_first<int32_t*>(2));

            // Where the population streaming compares the memory of its car models with the budget, before it loads another
            pattern = hook::pattern("05 A0 1C E9 FF 39 86 0C 07 00 00");
            if (!pattern.empty()) // 1.2.0.32, 1.2.0.43, 1.2.0.59: the population streaming in esi
            {
                static auto PopulationBudgetHook = safetyhook::create_mid(pattern.get_first(0), [](SafetyHookContext& regs)
                {
                    PopulationBudget::Update(regs.esi);
                });
            }
            else // 1.0.7.0, 1.0.8.0, 1.1.2.0, 1.1.3.0: in ebp
            {
                pattern = hook::pattern("05 A0 1C E9 FF 39 85 0C 07 00 00");
                static auto PopulationBudgetHook = safetyhook::create_mid(pattern.get_first(0), [](SafetyHookContext& regs)
                {
                    PopulationBudget::Update(regs.ebp);
                });
            }

            // Force the -nomemrestrict commandline arg. Fixes delayed streaming in some circumstances although not when Texture Quality is either Medium or Low.
            pattern = hook::pattern("83 3D ? ? ? ? ? 0F 85 ? ? ? ? A1 ? ? ? ? 85 C0 74");
            if (!pattern.empty()) // 1.0.8.0, 1.1.3.0, 1.2.0.32, 1.2.0.43, 1.2.0.59
            {
                auto loc_427FC9 = resolve_next_displacement(pattern.get_first(0)).value();
                injector::MakeJMP(pattern.get_first(0), loc_427FC9, true);
            }
            else // 1.0.7.0, 1.1.2.0
            {
                pattern = hook::pattern("75 ? A1 ? ? ? ? 85 C0 74 ? 80 38");
                injector::WriteMemory<uint8_t>(pattern.get_first(0), 0xEB, true); // jnz --> jmp
            }

            // Release queue of the resource cache
            pattern = hook::pattern("83 BE 48 01 04 00 00 74 ? 8D 7E 08");
            auto bNewResourceCache = !pattern.empty();
            if (bNewResourceCache) // 1.0.8.0, 1.1.3.0, 1.2.0.32, 1.2.0.43, 1.2.0.59
            {
                // The empty test before the loop that empties the array (cache in esi). Only once per pass: what is left
                // over beyond a full array waits for the next pass, as anything added after a pass does.
                static auto RefillHook = safetyhook::create_mid(pattern.get_first(0), [](SafetyHookContext& regs)
                {
                    grcResourceCache::RefillReleaseQueue(regs.esi);
                });

                // The add
                pattern = hook::pattern("8B 87 48 01 04 00 8D 0C 87 40 89 87 48 01 04 00 89 99 48 01 00 00");
                auto bNewAddPattern = !pattern.empty();
                if (bNewAddPattern) // 1.2.0.32, 1.2.0.43, 1.2.0.59
                {
                    struct QueueReleaseHook
                    {
                        void operator()(injector::reg_pack& regs)
                        {
                            grcResourceCache::QueueRelease(regs.edi, static_cast<uint32_t>(regs.ebx));
                        }
                    }; injector::MakeInline<QueueReleaseHook>(pattern.get_first(0), pattern.get_first(22));

                    pattern = hook::pattern("A1 ? ? ? ? 8D 0C 85 ? ? ? ? 40 A3");
                }
                else // 1.0.8.0, 1.1.3.0
                {
                    pattern = hook::pattern("8B 86 48 01 04 00 8B 54 24 08 8D 48 01 89 8E 48 01 04 00 89 94 86 48 01 00 00");
                    struct QueueReleaseHook
                    {
                        void operator()(injector::reg_pack& regs)
                        {
                            grcResourceCache::QueueRelease(regs.esi, *reinterpret_cast<uint32_t*>(regs.esp + 8));
                        }
                    }; injector::MakeInline<QueueReleaseHook>(pattern.get_first(0), pattern.get_first(26));

                    pattern = hook::pattern("A1 ? ? ? ? 8D 0C 85 ? ? ? ? 83 C0 01 A3");
                }

                // Its inlined copies: mov eax, [count]; lea ecx, queue[eax*4]; inc eax; mov [count], eax; mov [ecx], reg.
                // Other arrays are added to the same way: the queue is the one whose count follows its 65536 entries.
                uintptr_t count = 0;
                uintptr_t queue = 0;
                pattern.for_each_result([&](hook::pattern_match match)
                {
                    auto at = match.get<uint8_t>(0);
                    auto matchCount = *reinterpret_cast<uintptr_t*>(at + 1);
                    auto matchQueue = *reinterpret_cast<uintptr_t*>(at + 8);
                    if (matchQueue == matchCount - grcResourceCache::nReleaseQueueSize * sizeof(uint32_t))
                    {
                        count = matchCount;
                        queue = matchQueue;
                    }
                });
                grcResourceCache::nCache = count - grcResourceCache::nReleaseCountOffset;

                auto inc = bNewAddPattern ? 1 : 3;
                auto copies = hook::pattern("A1 " + pattern_str(to_bytes(count)) + "8D 0C 85 " + pattern_str(to_bytes(queue)));
                copies.count(inc == 1 ? 4 : 3).for_each_result([&](hook::pattern_match match)
                {
                    auto at = match.get<uint8_t>(0);
                    auto end = at + 12 + inc + 5 + 2;
                    switch (end[-1])
                    {
                        case 0x31: injector::MakeInline<grcResourceCache::QueueReleaseInline<&SafetyHookContext::esi>>(at, end); break;
                        case 0x39: injector::MakeInline<grcResourceCache::QueueReleaseInline<&SafetyHookContext::edi>>(at, end); break;
                        default: assert(false);
                    }
                });
            }
            // 1.0.7.0, 1.1.2.0: the cache has no release queue

            // Entity link pool
            {
                uint8_t* take = nullptr;
                ptrdiff_t takeAcquire = 0;
                pattern = hook::pattern("56 57 68 ? ? ? ? 8B F9 E8 ? ? ? ? 83 C4 04 8B CF E8 ? ? ? ? 8B F0 85 F6 75 ? B9");
                if (!pattern.empty()) // 1.2.0.32, 1.2.0.43, 1.2.0.59
                {
                    take = pattern.get_first<uint8_t>(0);
                    takeAcquire = 9;
                }
                else // 1.0.7.0, 1.0.8.0, 1.1.2.0, 1.1.3.0
                {
                    pattern = hook::pattern("53 56 57 68 ? ? ? ? 8B F9 33 DB E8 ? ? ? ? 83 C4 04 8B CF E8 ? ? ? ? 8B F0 3B F3 75 ? B9");
                    take = pattern.get_first<uint8_t>(0);
                    takeAcquire = 12;
                }

                pattern = hook::pattern("56 57 68 ? ? ? ? 8B F9 E8 ? ? ? ? 8B 74 24 10 83 C4 04 8B CE E8");
                auto release = pattern.get_first<uint8_t>(0);

                // The game's spinlock goes: its call is removed, the push of its address and the add after the call stay
                InitializeCriticalSectionAndSpinCount(&EntityLinkPool::Lock, 4000);
                injector::MakeNOP(take + takeAcquire, 5, true);
                injector::MakeNOP(release + 9, 5, true);
                EntityLinkPool::shTakeLink = safetyhook::create_inline(take, EntityLinkPool::TakeLink);
                EntityLinkPool::shReleaseLink = safetyhook::create_inline(release, EntityLinkPool::ReleaseLink);
            }

            // Texture copies
            {
                uint8_t* destroy = nullptr;
                ptrdiff_t vtableOffset = 0;
                pattern = hook::pattern("56 57 8B F9 C7 07 ? ? ? ? E8 ? ? ? ? 29 05 ? ? ? ? 8A 47 4D");
                if (!pattern.empty()) // 1.2.0.32, 1.2.0.43, 1.2.0.59: the cache's allocator takes the cache in ecx
                {
                    destroy = pattern.get_first<uint8_t>(0);
                    vtableOffset = 6;

                    // The two allocations of a copy (made from an image, made from a resource)
                    for (auto copy : { "57 6A 00 B9 ? ? ? ? E8 ? ? ? ? 8B F0 85 F6", "57 6A 00 B9 ? ? ? ? E8 ? ? ? ? 89 45 48" })
                        TextureCopies::CacheAllocThis = reinterpret_cast<decltype(TextureCopies::CacheAllocThis)>(injector::MakeCALL(hook::pattern(copy).get_first(8), TextureCopies::AllocCopyThis, true).get<void>());

                    // After each stores the copy (texture in ebx, ebp)
                    static auto LoadedHook1 = safetyhook::create_mid(hook::pattern("89 73 48 80 4B 26 80").get_first(3), [](SafetyHookContext& regs)
                    {
                        TextureCopies::Loaded(regs.ebx);
                    });
                    static auto LoadedHook2 = safetyhook::create_mid(hook::pattern("83 C4 0C C7 45 18 00 00 00 00 80 4D 26 80").get_first(3), [](SafetyHookContext& regs)
                    {
                        TextureCopies::Loaded(regs.ebp);
                    });
                }
                else if (bNewResourceCache) // 1.0.8.0, 1.1.3.0: the cache's allocator is stdcall
                {
                    destroy = hook::pattern("53 55 8B D9 56 8B F3 C7 03 ? ? ? ? E8 ? ? ? ? 29 05 ? ? ? ? 8A 43 4D").get_first<uint8_t>(0);
                    vtableOffset = 9;

                    for (auto copy : { "57 6A 00 68 ? ? ? ? E8 ? ? ? ? 8B F0 85 F6 74 15 8B 5B 10", "56 6A 00 68 ? ? ? ? E8 ? ? ? ? 85 C0 8B 4C 24 1C 89 01" })
                        TextureCopies::CacheAllocStd = reinterpret_cast<decltype(TextureCopies::CacheAllocStd)>(injector::MakeCALL(hook::pattern(copy).get_first(8), TextureCopies::AllocCopyStd, true).get<void>());

                    // Texture in ebp, ebx
                    static auto LoadedHook1 = safetyhook::create_mid(hook::pattern("89 75 48 80 4D 26 80").get_first(3), [](SafetyHookContext& regs)
                    {
                        TextureCopies::Loaded(regs.ebp);
                    });
                    static auto LoadedHook2 = safetyhook::create_mid(hook::pattern("83 C4 0C C7 43 18 00 00 00 00 80 4B 26 80").get_first(3), [](SafetyHookContext& regs)
                    {
                        TextureCopies::Loaded(regs.ebx);
                    });
                }
                // 1.0.7.0, 1.1.2.0: textures are made managed (Direct3D keeps the copy), the game keeps none of its own

                if (destroy)
                {
                    TextureCopies::Init();

                    // grcTexturePC's functions that use the copy, from its vtable (set by the destructor)
                    auto vtable = *reinterpret_cast<void***>(destroy + vtableOffset);
                    TextureCopies::shMake = safetyhook::create_inline(vtable[4], TextureCopies::Make);
                    TextureCopies::shLock = safetyhook::create_inline(vtable[19], TextureCopies::LockTexture);
                    TextureCopies::shUnlock = safetyhook::create_inline(vtable[20], TextureCopies::UnlockTexture);
                    TextureCopies::shUpdate = safetyhook::create_inline(vtable[26], TextureCopies::Update);
                    TextureCopies::shRebuild = safetyhook::create_inline(vtable[28], TextureCopies::Rebuild);
                    TextureCopies::shDestroy = safetyhook::create_inline(destroy, TextureCopies::Destroy);
                }
            }
        };
    }
} Memory;
