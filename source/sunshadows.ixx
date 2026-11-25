module;

#include <common.hxx>
#include <atomic>

export module sunshadows;

import common;
import comvars;

namespace CascadeBounds
{
    // Prepare the native cascade splits, matrices and clipping data before scanning; drawing reuses the same result.
    void (__cdecl* PrepareNativeBounds)() = nullptr;
    uintptr_t (__cdecl* GetNativeBounds)() = nullptr;
    uint32_t* pUpdateBufferIndex = nullptr;
    uintptr_t pCascadeState = 0;
    std::atomic<uintptr_t> PreparedPhase = 0;
    std::atomic<uintptr_t> PreparedBounds = 0;
    std::atomic<uint32_t> PreparedFrame = 0;

    uintptr_t GetPreparedBounds(uintptr_t phase)
    {
        if (PreparedFrame.load(std::memory_order_acquire) != *CTimer::m_frameCount
            || PreparedPhase.load(std::memory_order_relaxed) != phase)
            return 0;

        auto bank = *pUpdateBufferIndex;
        auto bounds = PreparedBounds.load(std::memory_order_relaxed);
        return bank < 2 && bounds == pCascadeState + 0x270 * bank + 0x1A0 ? bounds : 0;
    }

    uintptr_t __fastcall GetScanBounds(uintptr_t phase)
    {
        if (auto bounds = GetPreparedBounds(phase))
            return bounds;
        return GetNativeBounds();
    }

    // At the getter call in CRenderer::IsClipped, ESI holds the phase and EAX receives the bounds.
    __declspec(naked) uintptr_t GetScanBoundsHook()
    {
        __asm
        {
            mov ecx, esi
            jmp GetScanBounds
        }
    }

    void Init()
    {
        auto scan = hook::pattern("6A 01 6A 01 56 8D 86 FC 08 00 00 50 8D 86 F8 08 00 00 50 8D 86 00 09 00 00 50 E8");
        auto draw = hook::pattern("E8 ? ? ? ? A1 ? ? ? ? 69 C0 70 02 00 00 05 ? ? ? ? 50 8D 4C 24 74 E8");
        auto clip = hook::pattern("55 8B EC 83 E4 F0 83 EC 48 56 8B 75 0C 57 83 7E 3C 01 0F 85 ? ? ? ? E8 ? ? ? ? 80 3D ? ? ? ? 00 8B 75 08 8B F8");
        auto callOffset = 0x18;
        if (scan.empty()) // 1.0.7.0, 1.0.8.0
        {
            scan = hook::pattern("6A 01 6A 01 56 8D 96 FC 08 00 00 52 8D 86 F8 08 00 00 50 8D 8E 00 09 00 00 51 E8");
            clip = hook::pattern("55 8B EC 83 E4 F0 83 EC 44 53 56 8B 75 0C 83 7E 3C 01 57 0F 85 ? ? ? ? E8 ? ? ? ? 80 3D ? ? ? ? 00 8B 75 08 8B F8");
            callOffset = 0x19;
        }
        if (scan.size() != 1 || draw.size() != 1 || clip.size() != 1)
            return;

        auto getterCall = clip.get_first(callOffset);
        injector::scoped_unprotect writable(getterCall, 5);
        if (!writable.bUnprotected)
            return;
        GetNativeBounds = reinterpret_cast<decltype(GetNativeBounds)>(injector::GetBranchDestination(getterCall).as_int());
        PrepareNativeBounds = reinterpret_cast<decltype(PrepareNativeBounds)>(injector::GetBranchDestination(draw.get_first()).as_int());
        pUpdateBufferIndex = *draw.get_first<uint32_t*>(6);
        pCascadeState = *draw.get_first<uintptr_t>(17);

        // CRenderPhaseCascadeShadows::BuildDrawList: reuse the prepared state instead of advancing the splits twice.
        static uintptr_t afterPrepare = uintptr_t(draw.get_first(5));
        static auto DrawHook = safetyhook::create_mid(draw.get_first(), [](SafetyHookContext& regs)
        {
            if (GetPreparedBounds(regs.edi))
                regs.eip = afterPrepare;
        });
        if (!DrawHook)
            return;

        // Install both consumers before the scan can publish. Until then they use the native paths.
        static injector::scoped_call GetterHook(getterCall, GetScanBoundsHook);

        // CRenderPhaseCascadeShadows::BuildRenderList, after the shadow/sun enablement checks, before caster scanning.
        static auto ScanHook = safetyhook::create_mid(scan.get_first(), [](SafetyHookContext& regs)
        {
            PrepareNativeBounds();
            // Publish before caster work starts. FinishBuildRenderList waits before the next update reuses the bank.
            // Each 0x270-byte bank holds its cascade clipping data at +0x1A0.
            PreparedPhase.store(regs.esi, std::memory_order_relaxed);
            PreparedBounds.store(pCascadeState + 0x270 * *pUpdateBufferIndex + 0x1A0, std::memory_order_relaxed);
            PreparedFrame.store(*CTimer::m_frameCount, std::memory_order_release);
        });

        if (!ScanHook)
        {
            GetterHook.restore();
            DrawHook.reset();
        }
    }
}

// Render phases carry a type at +0x3C: 0 for the light shadow maps, 1 for the sun cascades and 2 for everything else.
// When the world scan reaches an interior, only type 0 sends it through the interior shadow path, so the people and
// props inside interiors never reached the sun cascades and cast no sun shadows through doorways and windows. Every
// shadow phase (any type but 2) takes that path, the cascades with the directional slot.

static injector::memory_pointer_raw CRenderPhaseWarpShadow__GetWarpShadowType = nullptr;

static int __fastcall CRenderPhaseWarpShadow__GetWarpShadowTypeHook(uint8_t* pRenderPhase, void*)
{
    if (*(int32_t*)(pRenderPhase + 0x3C) == 1)
    {
        // The cascades never set which entity kinds they take from interiors: static buildings and tracked objects
        pRenderPhase[0x15] = 1;
        pRenderPhase[0x16] = 1;

        // Any type but 0 makes the interior use the directional path (slot -1)
        return 1;
    }

    return injector::thiscall<int(void*)>::call(CRenderPhaseWarpShadow__GetWarpShadowType, pRenderPhase);
}

class SunShadows
{
public:
    SunShadows()
    {
        FusionFix::onInitEvent() += []()
        {
            // CRenderer::HandleMLOObjectsInScan: if (pRenderPhase->type == 0) -> if (pRenderPhase->type != 2)
            auto pattern = hook::pattern("83 7E 3C 00 75 ? 8B CE E8 ? ? ? ? 85 C0 75 ? 8B CE E8");
            if (!pattern.empty()) // 1.2.0.32, 1.2.0.43, 1.2.0.59
            {
                CRenderPhaseWarpShadow__GetWarpShadowType = injector::GetBranchDestination(pattern.get_first(8));
                injector::MakeCALL(pattern.get_first(8), CRenderPhaseWarpShadow__GetWarpShadowTypeHook, true);
            }
            else // 1.0.7.0, 1.0.8.0, 1.1.2.0, 1.1.3.0
            {
                pattern = hook::pattern("83 7E 3C 00 75 ? 8B CE 83 CB FF E8");
                CRenderPhaseWarpShadow__GetWarpShadowType = injector::GetBranchDestination(pattern.get_first(11));
                injector::MakeCALL(pattern.get_first(11), CRenderPhaseWarpShadow__GetWarpShadowTypeHook, true);
            }

            injector::WriteMemory<uint8_t>(pattern.get_first(3), 2, true);
            injector::WriteMemory<uint8_t>(pattern.get_first(4), 0x74, true); // jnz -> jz

            CascadeBounds::Init();
        };
    }
} SunShadows;
