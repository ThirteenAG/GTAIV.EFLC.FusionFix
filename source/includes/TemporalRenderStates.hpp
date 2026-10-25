#pragma once
#include <d3d9.h>

namespace TemporalRenderStates
{
    // Temporal resolve writes both the visible image and its history. Preserve
    // every MRT mask because the surrounding game passes configure them separately.
    class ColorWriteState
    {
        static constexpr D3DRENDERSTATETYPE States[] = {
            D3DRS_COLORWRITEENABLE, D3DRS_COLORWRITEENABLE1,
            D3DRS_COLORWRITEENABLE2, D3DRS_COLORWRITEENABLE3
        };
        IDirect3DDevice9* device;
        DWORD saved[4]{};
    public:
        explicit ColorWriteState(IDirect3DDevice9* value) : device(value)
        {
            for (unsigned i = 0; i < 4; ++i) device->GetRenderState(States[i], &saved[i]);
        }
        ~ColorWriteState()
        {
            for (unsigned i = 0; i < 4; ++i) device->SetRenderState(States[i], saved[i]);
        }
        ColorWriteState(const ColorWriteState&) = delete;
        ColorWriteState& operator=(const ColorWriteState&) = delete;
        static void Set(IDirect3DDevice9* device, DWORD mask)
        {
            for (auto state : States) device->SetRenderState(state, mask);
        }
    };
}
