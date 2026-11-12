module;

#include <common.hxx>

export module sunshadows;

import common;

// Render phases carry a type at +0x3C: 0 for the light shadow maps, 1 for the sun cascades and 2 for everything else.
// When the world scan reaches an interior, only type 0 sends it through the interior shadow path, so the people and
// props inside interiors never reached the sun cascades and cast no sun shadows through doorways and windows. Every
// shadow phase (any type but 2) takes that path, the cascades with the directional slot.

static injector::memory_pointer_raw CRenderPhaseWarpShadow__GetLightType = nullptr;

static int __fastcall CRenderPhaseWarpShadow__GetLightTypeHook(uint8_t* pRenderPhase, void*)
{
    if (*(int32_t*)(pRenderPhase + 0x3C) == 1)
    {
        // The cascades never set which entity kinds they take from interiors: static buildings and tracked objects
        pRenderPhase[0x15] = 1;
        pRenderPhase[0x16] = 1;

        // Any type but 0 makes the interior use the directional path (slot -1)
        return 1;
    }

    return injector::thiscall<int(void*)>::call(CRenderPhaseWarpShadow__GetLightType, pRenderPhase);
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
                CRenderPhaseWarpShadow__GetLightType = injector::GetBranchDestination(pattern.get_first(8));
                injector::MakeCALL(pattern.get_first(8), CRenderPhaseWarpShadow__GetLightTypeHook, true);
            }
            else // 1.0.7.0, 1.0.8.0, 1.1.2.0, 1.1.3.0
            {
                pattern = hook::pattern("83 7E 3C 00 75 ? 8B CE 83 CB FF E8");
                CRenderPhaseWarpShadow__GetLightType = injector::GetBranchDestination(pattern.get_first(11));
                injector::MakeCALL(pattern.get_first(11), CRenderPhaseWarpShadow__GetLightTypeHook, true);
            }

            injector::WriteMemory<uint8_t>(pattern.get_first(3), 2, true);
            injector::WriteMemory<uint8_t>(pattern.get_first(4), 0x74, true); // jnz -> jz
        };
    }
} SunShadows;
