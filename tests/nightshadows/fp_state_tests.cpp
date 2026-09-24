#include "../../source/ShadowFloatingPointState.hpp"
#include "../../source/StableHeadlightSelector.hpp"
#include "../../source/PlayerShadowBudget.hpp"

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>

#if !defined(_M_IX86)
#error This harness specifically validates the 32-bit GTA IV callback environment.
#endif

struct alignas(16) FxImage { unsigned char bytes[512]; };
struct Evidence
{
    FxImage entry;
    FxImage afterExceptions;
    FxImage nestedEntry;
    FxImage nestedExit;
    volatile float sseValue{};
    volatile double x87Value{};
    std::uint32_t policyCount{};
    bool nestedRestored{};
};

static unsigned checks{};
#define CHECK(expression) do { ++checks; if (!(expression)) { std::cerr << __LINE__ << ": " #expression "\n"; std::exit(1); } } while (false)

template<typename T> static T Read(const FxImage& image, unsigned offset)
{
    T value;
    std::memcpy(&value, image.bytes + offset, sizeof(value));
    return value;
}
template<typename T> static void Write(FxImage& image, unsigned offset, T value)
{
    std::memcpy(image.bytes + offset, &value, sizeof(value));
}

static bool EqualFp(const FxImage& a, const FxImage& b)
{
    if (std::memcmp(a.bytes, b.bytes, 5) || // x87 control, status (includes TOP), abridged tag
        std::memcmp(a.bytes + 24, b.bytes + 24, 4)) // MXCSR
        return false;
    for (unsigned i = 0; i < 8; ++i)
        if (std::memcmp(a.bytes + 32 + i * 16, b.bytes + 32 + i * 16, 10)) return false;
    return true;
}

// The optimized assembly listing for this exact callback is retained by the
// runner. No FP parameters/results are passed across its external call boundary.
extern "C" __declspec(noinline) void GuardedPolicy(Evidence* evidence, bool earlyReturn)
{
    const fusionfix::shadows::FloatingPointState guard;
    _fxsave(evidence->entry.bytes);
    if (earlyReturn) return;

    volatile float inputZero = 0.0f;
    const auto zero = _mm_set_ss(inputZero);
    _mm_store_ss(const_cast<float*>(&evidence->sseValue), _mm_div_ss(zero, zero));
    auto* x87Result = const_cast<double*>(&evidence->x87Value);
    __asm {
        fld1
        fldz
        fdivp st(1), st(0)
        mov eax, x87Result
        fstp qword ptr [eax]
    }
    _fxsave(evidence->afterExceptions.bytes);

    {
        const fusionfix::shadows::FloatingPointState nested;
        _fxsave(evidence->nestedEntry.bytes);
        volatile double one = 1.0, three = 3.0;
        evidence->x87Value = one / three;
    }
    _fxsave(evidence->nestedExit.bytes);
    evidence->nestedRestored = EqualFp(evidence->afterExceptions, evidence->nestedExit);

    const fusionfix::shadows::Vec3 position{903, -376, 16}, source{903, -381, 16}, forward{0,1,0};
    const auto geometry = fusionfix::shadows::EvaluateGeometry(position, source, &forward);
    fusionfix::shadows::StableHeadlightSelector selector;
    selector.BeginFrame({1, 16, 100, true, 200});
    selector.Consider({200, geometry, true});
    selector.BeginFrame({2, 32, 100, true, 200});
    evidence->policyCount = selector.Consider({200, geometry, true}) ? 1 : 0;

    fusionfix::shadows::budget::PlayerShadowBudget budget;
    budget.BeginPass({2, 32, 100, true});
    budget.Add({1, 0, fusionfix::shadows::budget::Kind::Lamp, geometry.distanceSquared, true, true, 0});
    budget.Add({2, 1, fusionfix::shadows::budget::Kind::Lamp, geometry.distanceSquared + 1, true, true, 0});
    budget.Add({200, 2, fusionfix::shadows::budget::Kind::PlayerBeam, geometry.distanceSquared, false, true, 0});
    const auto& selection = budget.Finalize();
    if (selection.safeToApply && selection.count == 3) evidence->policyCount += 3;
}

// Integer-only boundary around the guard. Capture the restored state before any
// assertions or CRT code can issue floating-point instructions. Restore the
// harness's own FP state even when the test seed contains unmasked pending flags.
extern "C" __declspec(noinline) void RunSeed(const FxImage* seed, FxImage* before, FxImage* after,
                                              Evidence* evidence, bool earlyReturn)
{
    FxImage original;
    _fxsave(original.bytes);
    _fxrstor(const_cast<unsigned char*>(seed->bytes));
    _fxsave(before->bytes);
    GuardedPolicy(evidence, earlyReturn);
    _fxsave(after->bytes);
    _fxrstor(original.bytes);
}

static void DefaultEnvironment(const FxImage& image)
{
    CHECK(Read<std::uint16_t>(image, 0) == 0x037F);
    CHECK(Read<std::uint16_t>(image, 2) == 0);
    CHECK(image.bytes[4] == 0);
    CHECK(Read<std::uint32_t>(image, 24) == 0x1F80);
}

int main()
{
    FxImage initial;
    _fxsave(initial.bytes);
    auto supportedMxcsr = Read<std::uint32_t>(initial, 28);
    if (!supportedMxcsr) supportedMxcsr = 0xFFBF;
    unsigned cases = 0;
    for (unsigned rounding = 0; rounding < 4; ++rounding)
    for (unsigned top = 0; top < 8; ++top)
    for (unsigned masks = 0; masks < 2; ++masks)
    for (unsigned tags = 0; tags < 4; ++tags)
    {
        FxImage seed = initial, before{}, after{};
        Evidence evidence{};
        const auto control = static_cast<std::uint16_t>(0x0040u | (masks ? 0x003Eu : 0x003Fu) |
            ((top % 2 ? 0x03u : 0x00u) << 8) | (rounding << 10));
        const auto status = static_cast<std::uint16_t>((top << 11) | (masks ? 0x8085u : 0x0005u));
        const unsigned char tagPatterns[]{0xFF, 0x55, 0x01, 0x00};
        Write(seed, 0, control);
        Write(seed, 2, status);
        seed.bytes[4] = tagPatterns[tags];
        std::uint32_t mxcsr = (0x1F80u | 0x3Fu | (rounding << 13) | ((top & 1) ? 0x8040u : 0));
        if (masks) mxcsr &= ~0x80u; // invalid exception unmasked, sticky flag already set
        Write(seed, 24, mxcsr & supportedMxcsr);
        for (unsigned i = 0; i < 8; ++i)
        {
            const std::uint64_t significand = 0x8000000000000000ull | (static_cast<std::uint64_t>(i + 1) << 52);
            const auto exponent = static_cast<std::uint16_t>(0x3FFFu + i + ((i & 1) ? 0x8000u : 0));
            Write(seed, 32 + i * 16, significand);
            Write(seed, 40 + i * 16, exponent);
        }
        RunSeed(&seed, &before, &after, &evidence, false);
        CHECK(EqualFp(seed, before)); // Verify the requested pending-state seed really loaded.
        CHECK(EqualFp(before, after));
        DefaultEnvironment(evidence.entry);
        DefaultEnvironment(evidence.nestedEntry);
        CHECK(evidence.nestedRestored);
        CHECK((Read<std::uint16_t>(evidence.afterExceptions, 2) & 4) != 0); // x87 divide-by-zero
        CHECK((Read<std::uint32_t>(evidence.afterExceptions, 24) & 1) != 0); // SSE invalid
        CHECK(evidence.policyCount == 4);
        // The early-return callback path must restore exactly the same state.
        RunSeed(&seed, &before, &after, &evidence, true);
        CHECK(EqualFp(before, after));
        DefaultEnvironment(evidence.entry);
        ++cases;
    }
    std::cout << "PASS: " << checks << " FP checks across " << cases <<
        " seeded x87/MXCSR states (normal, nested, and early-return paths).\n";
}
