module;

#include <common.hxx>
#include <dxgi1_4.h>
#include <span>
#include <unordered_map>
#include <deque>
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

    // What the game was told, for the streaming budget
    std::atomic<uint64_t> nVideoMemory{ 0 };

    SafetyHookInline shGetAvailableVideoMemory = {};
    uint64_t GetAvailableVideoMemory()
    {
        auto ret = shGetAvailableVideoMemory.call<uint64_t>();

        if (ret < _2048mb)
            ret = GetTotalVideoMemory();

        nVideoMemory.store(ret, std::memory_order_relaxed);
        return ret;
    }

    // 1.0.7.0, 1.1.2.0: a 32-bit value that some callers convert as signed
    uint32_t GetAvailableVideoMemory32()
    {
        auto ret = std::max<uint64_t>(shGetAvailableVideoMemory.call<uint32_t>(), GetTotalVideoMemory());
        ret = std::min<uint64_t>(ret, INT32_MAX);
        nVideoMemory.store(ret, std::memory_order_relaxed);
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
// piece). It is cut quickly when room gets short and raised back after a while of plenty.
//
// The game's own budget is a table of 2008: 800 MB with high texture quality, whatever the card has. When it is used
// up, the streaming makes room for the next model by deleting loaded ones in stages: those out of range first, then
// those in range that are not in view, at last those being drawn. From the second stage on what it deletes is needed
// again the moment the camera turns: models and textures pop in, and with bigger textures (texture packs) or longer
// view distances the same things are loaded and deleted hundreds of times a second while buildings and roads in view
// disappear. The budget grows while the streaming deletes models that are in range, up to a share of the video
// memory, and comes back down when much of it has been unused for a while. Bigger car and ped models (car mods) raise
// it by what they hold beyond the game's own budgets for them.
namespace StreamingBudget
{
    struct Limits
    {
        uint32_t nCriticalRoom, nCriticalLargest;   // MB: cut hard below either
        uint32_t nTightRoom, nTightLargest;         // cut below either
        uint32_t nRaiseRoom, nRaiseLargest;         // raise above both
    };

    // Direct3D 9 keeps a wider margin: a device reset allocates again. DirectX 12 and Vulkan start with less room (their
    // drivers map more into the process), so their limits are lower and the same steps apply.
    constexpr Limits Direct3D9Limits = { 500, 96, 800, 160, 1300, 384 };
    constexpr Limits OtherLimits = { 400, 80, 700, 128, 1200, 320 };

    // MB above the limits a cut starts at: the budget only grows with this much to spare, so it stops short of them
    constexpr uint32_t nGrowRoom = 250;
    constexpr uint32_t nGrowLargest = 128;

    constexpr float fMinScale = 0.6f;
    constexpr float fRaiseStep = 0.025f;
    constexpr float fCutStep = 0.05f;
    constexpr float fCriticalCutStep = 0.15f;

    // Of the game's free memory multiplier
    std::atomic<float> fScale{ 1.0f };

    // How much of the population budgets' growth is left: 1 until the budget is cut below the game's own, 0 at the
    // lowest cut
    std::atomic<float> fRoom{ 1.0f };

    // How much of the growth from Traffic and Pedestrian Variety is left, and the bytes the models hold beyond what
    // Original allows (PopulationBudget). That growth is the first to go when the address space gets short, before the
    // world's budget is touched, and comes back slowly once there is room to spare.
    std::atomic<float> fVariety{ 1.0f };
    std::atomic<int32_t> nVarietyBytes{ 0 };
    constexpr int32_t nVarietyInUse = 8 * 1024 * 1024;
    constexpr float fVarietyCutStep = 0.25f;
    constexpr float fVarietyRaiseStep = 0.05f;      // Per second
    constexpr uint32_t nVarietyRaiseDelay = 10000;  // After the last cut

    // Bytes: what the car and ped models hold beyond the game's own budgets for them (PopulationBudget). They are paid
    // from the same budget as the world, so it grows by as much: the world keeps the room it has with the game's own
    // models instead of streaming out around bigger ones.
    std::atomic<int32_t> nPopulationExtra{ 0 };

    // The stage the streaming's make-room pass is at when it deletes the next model: 0 out of range, 1 to 4 in range
    // and not in view, 5 and up also what is drawn. The highest since the budget was last looked at.
    constexpr ptrdiff_t nMakeRoomStageOffset = 0x48;
    constexpr int32_t nStageInRange = 1;
    constexpr int32_t nStageDrawn = 5;
    std::atomic<int32_t> nMakeRoomStage{ 0 };

    SafetyHookInline shDeleteNext = {};
    bool __fastcall DeleteNext(uintptr_t pass, void* edx, int32_t flags)
    {
        auto ret = shDeleteNext.unsafe_fastcall<bool>(pass, edx, flags);
        auto stage = *reinterpret_cast<int32_t*>(pass + nMakeRoomStageOffset);
        if (stage > nMakeRoomStage.load(std::memory_order_relaxed))
            nMakeRoomStage.store(stage, std::memory_order_relaxed);
        return ret;
    }

    constexpr float fMaxDemandScale = 4.0f;
    constexpr float fVideoShare = 0.75f;            // Of the video memory, for the budget at most
    constexpr float fGrowStep = 0.05f;              // Per second while models in range are deleted
    constexpr float fDrawnGrowStep = 0.2f;          // Per second while drawn ones are
    constexpr float fAmpleCredit = 0.35f;           // More left than this for a while: a step back,
    constexpr float fKeptCredit = 0.25f;            // as long as this much stays
    constexpr uint32_t nAmpleSeconds = 60;

    // The game's own budget at its multiplier and what is left of the budget, bytes
    std::atomic<int64_t> nGameBudget{ 0 };
    std::atomic<int64_t> nCredit{ 0 };

    // 1.0.7.0, 1.1.2.0: the multiplier is a constant in memory; the code reads this one instead
    float fBaseMultiplier = 0.0f;
    float fMultiplier = 0.0f;

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

        float scale = 1.0f;         // Of the budget the streaming needs: cut and raised by the room in the address space
        float content = 1.0f;       // That budget, of the game's own
        float variety = 1.0f;
        uint64_t lastVariety = 0;
        uint64_t lastContent = 0;
        uint32_t ample = 0;         // Seconds
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

            auto critical = space.nRoom < limits.nCriticalRoom || space.nLargest < limits.nCriticalLargest;
            auto tight = critical || space.nRoom < limits.nTightRoom || space.nLargest < limits.nTightLargest;
            auto raise = space.nRoom > limits.nRaiseRoom && space.nLargest > limits.nRaiseLargest;
            watch = !raise;

            auto budget = nGameBudget.load(std::memory_order_relaxed);
            auto credit = nCredit.load(std::memory_order_relaxed);

            // Once a second: up while the streaming deletes what is in range, within what the video memory holds; down
            // when much has been unused for a while
            auto newContent = content;
            if (now - lastContent >= 1000 && budget > 0)
            {
                lastContent = now;
                auto stage = nMakeRoomStage.exchange(0, std::memory_order_relaxed);
                auto most = fMaxDemandScale;
                if (auto video = grcDevice::nVideoMemory.load(std::memory_order_relaxed))
                    most = std::clamp(video * fVideoShare / budget, 1.0f, fMaxDemandScale);

                auto roomy = space.nRoom > limits.nTightRoom + nGrowRoom && space.nLargest > limits.nTightLargest + nGrowLargest;
                ample = credit > static_cast<int64_t>(budget * fAmpleCredit) ? ample + 1 : 0;
                if (stage >= nStageInRange && roomy && scale == 1.0f)
                    newContent = std::min(most, content + (stage >= nStageDrawn ? fDrawnGrowStep : fGrowStep));
                else if (ample >= nAmpleSeconds && credit - static_cast<int64_t>(budget * fGrowStep) > static_cast<int64_t>(budget * fKeptCredit))
                    newContent = std::max(1.0f, content - fGrowStep);
                newContent = std::min(newContent, most);
            }

            auto newScale = scale;

            if (tight)
            {
                roomSince = 0;

                // A cut takes a moment to show in the address space. The variety's growth goes first, then what the
                // budget grew by, and they stay off until there is room to spare again.
                if (now - lastChange >= 1000)
                {
                    auto cut = critical ? fCriticalCutStep : fCutStep;
                    if (variety > 0.0f && nVarietyBytes.load(std::memory_order_relaxed) > nVarietyInUse)
                        variety = critical ? 0.0f : std::max(0.0f, variety - fVarietyCutStep);
                    else if (newContent > 1.0f)
                        newContent = std::max(1.0f, newContent * (1.0f - cut));
                    else
                        newScale = std::max(fMinScale, newScale - cut);
                    lastCut = now;
                    lastChange = now;
                }
            }
            else if (raise)
            {
                if (!roomSince)
                    roomSince = now;

                // Back up to the game's own budget soon after a cut
                if (newScale < 1.0f && now - roomSince >= 5000 && now - lastCut >= 5000 && now - lastChange >= 2000)
                    newScale = std::min(1.0f, newScale + fRaiseStep);
            }
            else
                roomSince = 0;

            // The variety's growth back once a second, with room to spare and the world's budget whole
            if (!tight && variety < 1.0f && scale == 1.0f && now - lastCut >= nVarietyRaiseDelay && now - lastVariety >= 1000 &&
                space.nRoom > limits.nTightRoom + nGrowRoom && space.nLargest > limits.nTightLargest + nGrowLargest)
            {
                variety = std::min(1.0f, variety + fVarietyRaiseStep);
                lastVariety = now;
            }
            fVariety.store(variety, std::memory_order_relaxed);

            if (newScale != scale || newContent != content)
            {
                if (newScale != scale)
                    lastChange = now;
                scale = newScale;
                content = newContent;
                fScale = scale * content;
                fRoom = (scale - fMinScale) / (1.0f - fMinScale);
                if (fBaseMultiplier)
                    fMultiplier = fBaseMultiplier * scale * content;
            }

        }
    }
}

namespace StreamingReadWait
{
    bool __cdecl WaitForRead(HANDLE semaphore)
    {
        auto result = semaphore ? WaitForSingleObject(semaphore, 10) : WAIT_FAILED;
        if (result != WAIT_OBJECT_0 && result != WAIT_TIMEOUT)
            Sleep(10);
        return result == WAIT_OBJECT_0;
    }
}

namespace grcResourceCache
{
    constexpr ptrdiff_t nVideoUsedOffset = 0x40268;
    constexpr ptrdiff_t nSystemUsedOffset = 0x402C8;

    SafetyHookInline shGetAvailableFreeMemory = {};
    int64_t __fastcall GetAvailableFreeMemory(void* _this, void* edx, float Multiplier)
    {
        auto scale = StreamingBudget::fScale.load(std::memory_order_relaxed);
        auto extra = static_cast<int64_t>(StreamingBudget::nPopulationExtra.load(std::memory_order_relaxed) * std::min(scale, 1.0f));
        auto credit = shGetAvailableFreeMemory.unsafe_fastcall<int64_t>(_this, edx, Multiplier * scale);

        // What is used is the larger of the video and the system memory of the resources
        auto cache = reinterpret_cast<uintptr_t>(_this);
        auto used = std::max(*reinterpret_cast<int64_t*>(cache + nVideoUsedOffset), *reinterpret_cast<int64_t*>(cache + nSystemUsedOffset));
        StreamingBudget::nGameBudget.store(static_cast<int64_t>((credit + used) / scale), std::memory_order_relaxed);

        // The streaming keeps it in 32 bits
        credit = std::min<int64_t>(credit + extra, INT32_MAX);
        StreamingBudget::nCredit.store(credit, std::memory_order_relaxed);
        return credit;
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

// How much the streaming has loaded, for work that belongs behind the loading screen of a game being loaded and not
// behind the one the game starts with
export namespace LoadingScreenWork
{
    constexpr uint32_t nGameResources = 300;    // This much loaded behind a loading screen: a game is being loaded
    std::atomic<uint32_t> nResourcesLoaded{ 0 };
}

// The game makes a model's Direct3D vertex and index buffers when the model is first drawn and a texture's Direct3D
// texture when it is first bound, on the render thread in the middle of the frame, as many as the frame happens to
// need. Whatever comes into view for the first time, after a load, when the camera turns or on leaving an interior,
// brings several hundred at once, and that frame takes 50 to 150 ms and more. They are made ahead instead, in the
// order the streaming loads them: a limited number per frame while playing, all of them behind the loading screen of
// the first game loaded, and as many as fit into a limited time behind the loading screens after it (what is left
// goes on with the frames after it). What is drawn before its turn is still made on the spot, as the game does, so
// nothing is ever missing.
namespace EarlyUploads
{
    constexpr uint32_t nFrameUploads = 16;      // Per frame while little is waiting
    constexpr uint32_t nCatchUpFrames = 10;     // What waits is made within about this many frames
    constexpr double fUploadMs = 0.125;         // The copies of a frame's uploads take no longer than this each
    constexpr double fLoadingScreenMs = 1500.0; // The time one loading screen spends on them at most
    constexpr double fLoadingFrameMs = 30.0;    // In one of its frames

    enum class Kind { VertexBuffer, IndexBuffer, Texture };

    struct Upload
    {
        uintptr_t nResource;
        Kind kind;
    };

    // What the streaming has loaded and nothing has drawn or changed yet. A buffer or texture leaves when it is
    // destroyed or locked for writing, under the lock an upload holds: it stays alive and unchanged while it is made.
    SRWLOCK Lock = SRWLOCK_INIT;
    std::deque<Upload> Queue;
    std::unordered_map<uintptr_t, Kind> Waiting;    // By address, with its kind: a freed one's address can come back as another kind

    void Add(uintptr_t resource, Kind kind)
    {
        AcquireSRWLockExclusive(&Lock);
        if (auto waiting = Waiting.find(resource); waiting == Waiting.end() || waiting->second != kind)
        {
            Waiting[resource] = kind;
            Queue.push_back({ resource, kind });
        }
        ReleaseSRWLockExclusive(&Lock);
        LoadingScreenWork::nResourcesLoaded++;
    }

    void Forget(uintptr_t resource)
    {
        AcquireSRWLockExclusive(&Lock);
        Waiting.erase(resource);
        ReleaseSRWLockExclusive(&Lock);
    }

    // grcVertexBufferD3D and grcIndexBufferD3D made from a resource: the call that keeps the buffer's contents for
    // the Direct3D buffer made later
    void(__thiscall* KeepVertices)(uintptr_t) = nullptr;
    void __fastcall LoadedVertexBuffer(uintptr_t buffer, void* edx)
    {
        KeepVertices(buffer);
        Add(buffer, Kind::VertexBuffer);
    }

    void(__thiscall* KeepIndices)(uintptr_t) = nullptr;
    void __fastcall LoadedIndexBuffer(uintptr_t buffer, void* edx)
    {
        KeepIndices(buffer);
        Add(buffer, Kind::IndexBuffer);
    }

    SafetyHookInline shDestroyVertexBuffer = {};
    void __fastcall DestroyVertexBuffer(uintptr_t buffer, void* edx)
    {
        Forget(buffer);
        shDestroyVertexBuffer.unsafe_fastcall(buffer, edx);
    }

    SafetyHookInline shDestroyIndexBuffer = {};
    void __fastcall DestroyIndexBuffer(uintptr_t buffer, void* edx)
    {
        Forget(buffer);
        shDestroyIndexBuffer.unsafe_fastcall(buffer, edx);
    }

    int(__thiscall* MakeVertexBuffer)(uintptr_t) = nullptr;
    int(__thiscall* MakeIndexBuffer)(uintptr_t) = nullptr;
    void MakeTexture(uintptr_t texture);

    // Render thread
    int64_t nUploadTicks = 0;
    size_t nReady = 0;      // In the queue since the frame before: loaded completely by now
    std::atomic<uint32_t> nUnlimitedFrames{ 0 };

    int64_t Now()
    {
        LARGE_INTEGER now{};
        QueryPerformanceCounter(&now);
        return now.QuadPart;
    }

    // The system brings a new buffer or texture into video memory only when something is drawn with it, which is
    // most of what the first draw of many new ones waits for. Each is drawn with once right away, with a triangle
    // that writes nothing.
    struct MadeResource
    {
        IUnknown* pResource;
        Kind kind;
    };
    std::vector<MadeResource> Made;

    void Use()
    {
        if (Made.empty())
            return;

        constexpr UINT nVertices = 0x10000;
        constexpr UINT nStride = sizeof(float) * 3;
        auto device = rage::grcDevice::GetD3DDevice();
        static IDirect3DVertexBuffer9* vertices = nullptr;      // Managed: outlives a device reset
        if (vertices || SUCCEEDED(device->CreateVertexBuffer(nVertices * nStride, D3DUSAGE_WRITEONLY, D3DFVF_XYZ, D3DPOOL_MANAGED, &vertices, nullptr)))
        {
            // Everything changed here is read first and set back through the same device, so what the game remembers of
            // the device's state stays true
            static constexpr std::pair<D3DRENDERSTATETYPE, DWORD> states[] =
            {
                { D3DRS_COLORWRITEENABLE, 0 }, { D3DRS_COLORWRITEENABLE1, 0 }, { D3DRS_COLORWRITEENABLE2, 0 }, { D3DRS_COLORWRITEENABLE3, 0 },
                { D3DRS_ZENABLE, D3DZB_FALSE }, { D3DRS_ZWRITEENABLE, FALSE }, { D3DRS_STENCILENABLE, FALSE }, { D3DRS_ALPHABLENDENABLE, FALSE },
                { D3DRS_ALPHATESTENABLE, FALSE }, { D3DRS_FOGENABLE, FALSE }, { D3DRS_LIGHTING, FALSE }, { D3DRS_CULLMODE, D3DCULL_NONE },
            };
            DWORD oldStates[std::size(states)] = {};
            for (size_t i = 0; i < std::size(states); i++)
            {
                device->GetRenderState(states[i].first, &oldStates[i]);
                device->SetRenderState(states[i].first, states[i].second);
            }

            IDirect3DVertexShader9* vertexShader = nullptr;
            IDirect3DPixelShader9* pixelShader = nullptr;
            IDirect3DVertexDeclaration9* declaration = nullptr;
            IDirect3DVertexBuffer9* stream = nullptr;
            IDirect3DIndexBuffer9* indices = nullptr;
            IDirect3DBaseTexture9* texture = nullptr;
            UINT offset = 0, stride = 0;
            DWORD colorOp = 0, colorArg = 0;
            device->GetVertexShader(&vertexShader);
            device->GetPixelShader(&pixelShader);
            device->GetVertexDeclaration(&declaration);
            device->GetStreamSource(0, &stream, &offset, &stride);
            device->GetIndices(&indices);
            device->GetTexture(0, &texture);
            device->GetTextureStageState(0, D3DTSS_COLOROP, &colorOp);
            device->GetTextureStageState(0, D3DTSS_COLORARG1, &colorArg);

            device->SetVertexShader(nullptr);
            device->SetPixelShader(nullptr);
            device->SetFVF(D3DFVF_XYZ);
            device->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
            device->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
            for (auto& made : Made)
            {
                device->SetTexture(0, nullptr);
                device->SetStreamSource(0, vertices, 0, nStride);
                switch (made.kind)
                {
                    case Kind::VertexBuffer:
                    {
                        auto buffer = static_cast<IDirect3DVertexBuffer9*>(made.pResource);
                        D3DVERTEXBUFFER_DESC desc{};
                        if (FAILED(buffer->GetDesc(&desc)) || desc.Size < nStride * 3)
                            continue;
                        device->SetStreamSource(0, buffer, 0, nStride);
                        device->DrawPrimitive(D3DPT_TRIANGLELIST, 0, 1);
                        break;
                    }
                    case Kind::IndexBuffer:
                    {
                        auto buffer = static_cast<IDirect3DIndexBuffer9*>(made.pResource);
                        D3DINDEXBUFFER_DESC desc{};
                        if (FAILED(buffer->GetDesc(&desc)) || desc.Format != D3DFMT_INDEX16 || desc.Size < sizeof(uint16_t) * 3)
                            continue;
                        device->SetIndices(buffer);
                        device->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, 0, 0, nVertices, 0, 1);
                        break;
                    }
                    case Kind::Texture:
                        device->SetTexture(0, static_cast<IDirect3DBaseTexture9*>(made.pResource));
                        device->DrawPrimitive(D3DPT_TRIANGLELIST, 0, 1);
                        break;
                }
            }

            device->SetTextureStageState(0, D3DTSS_COLOROP, colorOp);
            device->SetTextureStageState(0, D3DTSS_COLORARG1, colorArg);
            device->SetTexture(0, texture);
            device->SetIndices(indices);
            device->SetVertexDeclaration(declaration);
            device->SetStreamSource(0, stream, offset, stride);
            device->SetVertexShader(vertexShader);
            device->SetPixelShader(pixelShader);
            for (size_t i = 0; i < std::size(states); i++)
                device->SetRenderState(states[i].first, oldStates[i]);
            for (IUnknown* held : { static_cast<IUnknown*>(vertexShader), static_cast<IUnknown*>(pixelShader), static_cast<IUnknown*>(declaration),
                static_cast<IUnknown*>(stream), static_cast<IUnknown*>(indices), static_cast<IUnknown*>(texture) })
            {
                if (held)
                    held->Release();
            }
        }
        for (auto& made : Made)
            made.pResource->Release();
        Made.clear();
    }

    int64_t nTicksPerMs = 0;
    int64_t nLoadingTicksLeft = 0;
    uint32_t nLoadedBefore = 0;
    bool bLoading = false;
    bool bFirstGame = true;

    void EndFrame()
    {
        if (!nUploadTicks)
        {
            LARGE_INTEGER frequency{};
            QueryPerformanceFrequency(&frequency);
            nTicksPerMs = frequency.QuadPart / 1000;
            nUploadTicks = static_cast<int64_t>(frequency.QuadPart * fUploadMs / 1000.0);
        }

        auto start = Now();
        auto loading = (CMenuManager::bLoadscreenShown && *CMenuManager::bLoadscreenShown) || bLoadingShown;
        if (loading && !bLoading)
        {
            nLoadingTicksLeft = static_cast<int64_t>(fLoadingScreenMs * nTicksPerMs);
            nLoadedBefore = LoadingScreenWork::nResourcesLoaded;
        }
        else if (!loading && bLoading && LoadingScreenWork::nResourcesLoaded - nLoadedBefore >= LoadingScreenWork::nGameResources)
            bFirstGame = false;
        bLoading = loading;

        // As much as the time allows behind a loading screen, and after a device reset, when all of it has to be made
        // again
        auto all = loading && nLoadingTicksLeft > 0;
        auto everything = loading && bFirstGame;
        if (nUnlimitedFrames)
        {
            nUnlimitedFrames--;
            all = true;
        }

        auto uploads = all || everything ? nReady : std::min(nReady, std::max<size_t>(nFrameUploads, nReady / nCatchUpFrames));
        auto limit = everything ? INT64_MAX : all ? static_cast<int64_t>(fLoadingFrameMs * nTicksPerMs) : static_cast<int64_t>(uploads * nUploadTicks);
        for (size_t made = 0; made < uploads; made++)
        {
            if (made && Now() - start >= limit)
                break;

            AcquireSRWLockExclusive(&Lock);
            if (!Queue.empty())
            {
                auto upload = Queue.front();
                Queue.pop_front();
                if (auto waiting = Waiting.find(upload.nResource); waiting != Waiting.end() && waiting->second == upload.kind)
                {
                    Waiting.erase(waiting);
                    IUnknown* made = nullptr;
                    switch (upload.kind)
                    {
                        case Kind::VertexBuffer:
                            MakeVertexBuffer(upload.nResource);
                            made = *reinterpret_cast<IUnknown**>(upload.nResource + 0x1C);
                            break;
                        case Kind::IndexBuffer:
                            MakeIndexBuffer(upload.nResource);
                            made = *reinterpret_cast<IUnknown**>(upload.nResource + 0x0C);
                            break;
                        case Kind::Texture:
                            MakeTexture(upload.nResource);
                            made = *reinterpret_cast<IUnknown**>(upload.nResource + 0x18);
                            break;
                    }
                    if (made)
                    {
                        made->AddRef();
                        Made.push_back({ made, upload.kind });
                    }
                }
            }
            ReleaseSRWLockExclusive(&Lock);
        }

        Use();
        if (loading)
            nLoadingTicksLeft -= Now() - start;

        AcquireSRWLockShared(&Lock);
        nReady = Queue.size();
        ReleaseSRWLockShared(&Lock);
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
        else
            EarlyUploads::Add(texture, EarlyUploads::Kind::Texture);
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
        EarlyUploads::Forget(texture);
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
        EarlyUploads::Forget(texture);
        return Mapped(texture, NoDevice(), [&]() { return shUpdate.unsafe_fastcall<int>(texture, edx, src, width, height, bpp); });
    }

    SafetyHookInline shRebuild = {};
    int __fastcall Rebuild(uintptr_t texture, void* edx, void* image)
    {
        EarlyUploads::Forget(texture);
        return Mapped(texture, NoDevice(), [&]() { return shRebuild.unsafe_fastcall<int>(texture, edx, image); });
    }

    SafetyHookInline shDestroy = {};
    void __fastcall Destroy(uintptr_t texture, void* edx)
    {
        EarlyUploads::Forget(texture);
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

namespace EarlyUploads
{
    void MakeTexture(uintptr_t texture)
    {
        TextureCopies::Make(texture, nullptr);
    }
}

// The population streaming loads another car or ped model while the memory of the ones it has is under a budget (40 and
// 50 MB). Everything counts: the shared car textures (vehshare) and the taxi, police, emergency, train, helicopter and
// boat models it keeps loaded whatever the budget says. With bigger models and textures (car mods, HD packs) those
// alone fill it: no other car model is loaded, and taxis are soon most of the cars on the streets. The budgets follow
// the size of the models in memory instead, so they hold as many models as the game's own budgets hold of its own
// models, and the car budget also holds the shared textures whatever their size. The models are paid from the
// streaming budget of the world, which grows by what they hold beyond the game's own budgets (StreamingBudget):
// otherwise a car mod's traffic takes its room from the city around it. When the address space gets short, that
// growth goes back with the world's.
// Traffic and Pedestrian Variety (High, Very High) gives the budgets room for two or four times as many models, so more
// different cars and people are on the streets at once (the game picks each new car or ped from the models in memory).
namespace PopulationBudget
{
    // MB per model in memory: the game's own models average about 3.2 (cars) and 4.4 (peds), and its shared car
    // textures take 1.6, so with these its own budgets are kept
    constexpr float fStockVehicleModelMB = 3.3f;
    constexpr float fStockPedModelMB = 4.9f;
    constexpr float fStockVehicleSharedMB = 1.6f;

    GameRef<int32_t> nVehicleBudget;
    GameRef<int32_t> nPedBudget;

    // Bytes of the shared car textures, the car memory the population streaming starts a session with
    int32_t nVehicleShared = 0;

    // Loaded model groups of the population streaming: 64 model indices each, -1 after the last
    constexpr ptrdiff_t VehicleGroups[] = { 0x000, 0x100, 0x200, 0x300 };
    constexpr ptrdiff_t PedGroups[] = { 0x404, 0x504, 0x604 };
    constexpr ptrdiff_t VehicleMemory = 0x70C;
    constexpr ptrdiff_t PedMemory = 0x708;

    // A model can be in two groups (the police models of the zone are in its car group too) but is in memory once
    int32_t CountModels(uintptr_t population, std::span<const ptrdiff_t> groups)
    {
        std::array<int32_t, 4 * 64> seen;
        int32_t count = 0;
        for (auto group : groups)
        {
            auto models = reinterpret_cast<const int32_t*>(population + group);
            for (int32_t i = 0; i < 64 && models[i] >= 0; i++)
            {
                if (std::find(seen.begin(), seen.begin() + count, models[i]) == seen.begin() + count)
                    seen[count++] = models[i];
            }
        }
        return count;
    }

    // How much bigger than the game's own the models in memory are, from at least a few of them
    float ModelScale(uintptr_t population, std::span<const ptrdiff_t> groups, int32_t bytes, float stockMB)
    {
        auto count = CountModels(population, groups);
        if (count < 3 || bytes <= 0)
            return 1.0f;
        return std::max(1.0f, bytes / (count * stockMB * 1048576.0f));
    }

    // Traffic and Pedestrian Variety: Original, High, Very High, as many models at once as Original holds
    constexpr float aVarietyModels[] = { 1.0f, 2.0f, 4.0f };

    // Of the 100 VehicleStructs (limits.ixx); Very High loads up to about 45
    constexpr int32_t nMaxPopulationCarModels = 64;

    void Update(uintptr_t population)
    {
        // The game's own, once it has set them up for the session
        static int32_t nStockVehicleBudget = nVehicleBudget;
        static int32_t nStockPedBudget = nPedBudget;

        auto vehicleBytes = *reinterpret_cast<const int32_t*>(population + VehicleMemory);
        auto pedBytes = *reinterpret_cast<const int32_t*>(population + PedMemory);
        auto vehicles = ModelScale(population, VehicleGroups, vehicleBytes - nVehicleShared, fStockVehicleModelMB);
        auto peds = ModelScale(population, PedGroups, pedBytes, fStockPedModelMB);

        // Traffic and Pedestrian Variety: room for more models at once, as far as the address space allows it
        static auto variety = FusionFixSettings.GetRef("PREF_POPULATIONVARIETY");
        auto models = aVarietyModels[std::clamp(variety->get(), 0, static_cast<int32_t>(std::size(aVarietyModels)) - 1)];
        auto more = 1.0f + (models - 1.0f) * StreamingBudget::fVariety.load(std::memory_order_relaxed);

        // The game's room for car models beyond its shared textures, scaled, and the shared textures
        constexpr auto nStockVehicleShared = static_cast<int64_t>(fStockVehicleSharedMB * 1048576.0f);
        auto VehicleBudget = [vehicles](float more)
        {
            return std::max<int64_t>(nStockVehicleBudget, nVehicleShared + static_cast<int64_t>((nStockVehicleBudget - nStockVehicleShared) * vehicles * more));
        };
        auto originalVehicleBudget = VehicleBudget(1.0f);
        auto originalPedBudget = static_cast<int64_t>(nStockPedBudget * peds);
        auto vehicleBudget = VehicleBudget(models);
        auto pedBudget = static_cast<int64_t>(originalPedBudget * models);

        // The growth goes back with the world's while the address space is short, the variety's first
        auto room = StreamingBudget::fRoom.load(std::memory_order_relaxed);
        auto Budget = [room](int64_t budget, int32_t stock)
        {
            return static_cast<int32_t>(std::min<int64_t>(stock + static_cast<int64_t>((budget - stock) * room), INT32_MAX));
        };
        nVehicleBudget = Budget(VehicleBudget(more), nStockVehicleBudget);
        nPedBudget = Budget(static_cast<int64_t>(originalPedBudget * more), nStockPedBudget);

        // Each car model in memory takes a VehicleStruct, and a full pool crashes the game: past this many in the
        // population's groups no more are loaded, so parked, mission and the player's cars keep theirs
        if (CountModels(population, VehicleGroups) >= nMaxPopulationCarModels)
            nVehicleBudget = std::min<int32_t>(nVehicleBudget, std::max(vehicleBytes, nStockVehicleBudget));

        // What the models hold beyond the game's own budgets. A cut budget doesn't free the models at once: the game
        // deletes them one by one, so the world keeps paying for them until then.
        auto Extra = [](int32_t bytes, int64_t budget, int32_t stock)
        {
            return static_cast<int32_t>(std::max<int64_t>(0, std::min<int64_t>(bytes, budget) - stock));
        };
        StreamingBudget::nPopulationExtra = Extra(vehicleBytes, vehicleBudget, nStockVehicleBudget) + Extra(pedBytes, pedBudget, nStockPedBudget);
        StreamingBudget::nVarietyBytes = Extra(vehicleBytes, vehicleBudget, static_cast<int32_t>(originalVehicleBudget)) + Extra(pedBytes, pedBudget, static_cast<int32_t>(originalPedBudget));
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

            // Let Windows schedule the main thread across the process's allowed CPUs.
            auto mainAffinity = hook::pattern("6A 01 8B F1 FF 15 ? ? ? ? 50 FF 15 ? ? ? ? E8");
            injector::MakeNOP(mainAffinity.get_first(0), 2, true);
            injector::MakeNOP(mainAffinity.get_first(4), 13, true);

            // Wake as soon as the next streaming read completes instead of sleeping between semaphore polls.
            auto readWait = hook::pattern("56 57 8B F9 33 F6 39 B7 ? ? ? ? 74 ? 8B FF 8B 87 ? ? ? ? 40 33 C9 83 F8 08 0F 44 C1 8B 84 87 ? ? ? ? FF 70 0C E8 ? ? ? ?");
            if (!readWait.empty()) // 1.2.0.32, 1.2.0.43, 1.2.0.59
            {
                injector::MakeCALL(readWait.get_first(0x29), StreamingReadWait::WaitForRead, true);
                injector::WriteMemory<uint8_t>(hook::pattern("6A 0A E8 ? ? ? ? 83 C6 0A 33 C0 83 C4 04 81 FE E8 03 00 00 0F 44 F0").get_first(1), 0, true);
            }
            else // 1.0.7.0, 1.0.8.0, 1.1.2.0, 1.1.3.0
            {
                readWait = hook::pattern("56 57 8B F1 33 FF 39 BE ? ? ? ? 0F 84 ? ? ? ? 8B 86 ? ? ? ? 83 C0 01 83 F8 08 75 ? 33 C0 8B 84 86 ? ? ? ? 8B 48 0C 51 E8 ? ? ? ?");
                injector::MakeCALL(readWait.get_first(0x2D), StreamingReadWait::WaitForRead, true);
                injector::WriteMemory<uint8_t>(hook::pattern("6A 0A E8 ? ? ? ? 83 C7 0A 83 C4 04 81 FF E8 03 00 00 75 ? 33 FF").get_first(1), 0, true);
            }

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

            // The population streaming's session start, its car memory set to the shared car textures
            pattern = hook::pattern("03 44 24 ? 83 C4 18 89 86 0C 07 00 00");
            if (!pattern.empty()) // 1.2.0.32, 1.2.0.43, 1.2.0.59: in eax
            {
                static auto VehicleSharedHook = safetyhook::create_mid(pattern.get_first(7), [](SafetyHookContext& regs)
                {
                    PopulationBudget::nVehicleShared = regs.eax;
                });
            }
            else // 1.0.7.0, 1.0.8.0, 1.1.2.0, 1.1.3.0: in edx
            {
                pattern = hook::pattern("03 D1 89 96 0C 07 00 00");
                static auto VehicleSharedHook = safetyhook::create_mid(pattern.get_first(2), [](SafetyHookContext& regs)
                {
                    PopulationBudget::nVehicleShared = regs.edx;
                });
            }

            // The streaming arena: the buddy allocator of 160 MB that holds the system memory part of every streamed
            // resource, built once at startup. Its blocks are powers of two, so with the big resources of texture packs
            // and car mods, or a bigger streaming budget, it ends up in pieces too small for the next one long before it
            // is full. The game then deletes what it has loaded, least used first, until a block of that size is free:
            // buildings and roads around the player disappear and are loaded again. 400 MB instead: room for the 200 MB
            // the streaming may use of it (stream.ini) at any rounding (the game's own smaller arena below 1.4 GB of
            // RAM stays).
            injector::WriteMemory<uint32_t>(hook::pattern("BF 00 80 02 00").get_first(1), 400 * 1024, true);

            // The streaming's make-room pass deleting its next model
            pattern = find_pattern("8D 41 48 50 FF 74 24 08 E8 ? ? ? ? C2 04 00", "8B 54 24 04 8D 41 48 50 52 E8 ? ? ? ? C2 04 00");
            StreamingBudget::shDeleteNext = safetyhook::create_inline(pattern.get_first(0), StreamingBudget::DeleteNext);

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

                // Vertex buffers, index buffers and textures made ahead of their first draw
                pattern = hook::pattern("8B CE C6 46 07 00 E8 ? ? ? ? C7 46 18 00 00 00 00");
                if (!pattern.empty()) // 1.2.0.32, 1.2.0.43, 1.2.0.59
                {
                    // Where the buffers of a loaded resource keep their contents
                    EarlyUploads::KeepVertices = reinterpret_cast<decltype(EarlyUploads::KeepVertices)>(injector::MakeCALL(pattern.get_first(6), EarlyUploads::LoadedVertexBuffer, true).get<void>());
                    pattern = hook::pattern("83 3F 00 74 0D 8B CE E8");
                    EarlyUploads::KeepIndices = reinterpret_cast<decltype(EarlyUploads::KeepIndices)>(injector::MakeCALL(pattern.get_first(7), EarlyUploads::LoadedIndexBuffer, true).get<void>());

                    // The functions that make the Direct3D buffers
                    pattern = hook::pattern("56 8B F1 8B 06 8B 40 24 FF D0 84 C0 0F 84 ? ? ? ? 33 C0 38 46 07 57 B9 00 02 00 00");
                    EarlyUploads::MakeVertexBuffer = reinterpret_cast<decltype(EarlyUploads::MakeVertexBuffer)>(pattern.get_first(0));
                    pattern = hook::pattern("51 56 8B F1 8B 06 8B 40 20 FF D0 84 C0 0F 84 ? ? ? ? 53 8B 5E 14");
                    EarlyUploads::MakeIndexBuffer = reinterpret_cast<decltype(EarlyUploads::MakeIndexBuffer)>(pattern.get_first(0));

                    // Their destructors, and their locks from the vtables the destructors set: the vertex buffer's
                    // three, and the one function behind both of the index buffer's
                    pattern = hook::pattern("56 8B F1 57 80 7E 06 00 C7 06 ? ? ? ? 74 05 E8");
                    auto vertexBuffer = *pattern.get_first<void**>(10);
                    EarlyUploads::shDestroyVertexBuffer = safetyhook::create_inline(pattern.get_first(0), EarlyUploads::DestroyVertexBuffer);
                    pattern = hook::pattern("56 8B F1 57 8B 7E 10 C7 06 ? ? ? ? 85 FF 74");
                    auto indexBuffer = *pattern.get_first<void**>(9);
                    EarlyUploads::shDestroyIndexBuffer = safetyhook::create_inline(pattern.get_first(0), EarlyUploads::DestroyIndexBuffer);

                    static std::vector<SafetyHookMid> lockHooks;
                    auto indexLock = injector::GetBranchDestination(reinterpret_cast<uintptr_t>(indexBuffer[1]) + 5).as_int();
                    for (auto lock : { reinterpret_cast<uintptr_t>(vertexBuffer[1]), reinterpret_cast<uintptr_t>(vertexBuffer[2]), reinterpret_cast<uintptr_t>(vertexBuffer[3]), indexLock })
                    {
                        lockHooks.emplace_back(safetyhook::create_mid(lock, [](SafetyHookContext& regs)
                        {
                            EarlyUploads::Forget(regs.ecx);
                        }));
                    }

                    FusionFix::onEndScene() += []()
                    {
                        EarlyUploads::EndFrame();
                    };

                    FusionFix::onBeforeReset() += []()
                    {
                        EarlyUploads::nUnlimitedFrames = 30;
                    };
                }

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
