#pragma once
#include <intrin.h>
#include <xmmintrin.h>

namespace fusionfix::shadows
{
    // The bundled x86 mid-hook preserves XMM registers but not x87/MXCSR.
    // Save legacy FP/SSE state, use masked round-to-nearest policy math, then
    // restore the game's complete incoming state before the native instruction.
    class FloatingPointState
    {
        alignas(16) unsigned char state_[512];
    public:
        FloatingPointState() noexcept
        {
            _fxsave(state_);
            _mm_setcsr(0x1F80);
#if defined(_M_IX86)
            __asm fninit
#endif
        }
        ~FloatingPointState() noexcept { _fxrstor(state_); }
        FloatingPointState(const FloatingPointState&) = delete;
        FloatingPointState& operator=(const FloatingPointState&) = delete;
    };
}
