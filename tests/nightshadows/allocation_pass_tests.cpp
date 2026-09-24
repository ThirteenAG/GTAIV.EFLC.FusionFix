#include "../../source/ShadowAllocationPass.hpp"
#if defined(_M_IX86)
#include "../../source/ShadowFloatingPointState.hpp"
#endif

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <limits>
#include <numeric>
#include <random>
#include <set>
#include <vector>

using namespace fusionfix::shadows::budget;
static unsigned checks{};
#define CHECK(expression) do { ++checks; if (!(expression)) { std::cerr << __LINE__ << ": " #expression "\n"; std::exit(1); } } while (false)

using LightRecord = std::array<std::uint8_t, ShadowAllocationPass::LightBytes>;
struct Output
{
    std::array<std::uint32_t, 4> before{{0x13579bdf, 0x89abcdef, 0x12345678, 0xfedcba98}};
    std::array<std::int32_t, 7> indices{{-1,-1,-1,-1,-1,-1,-1}};
    std::array<std::uint32_t, 4> after{{0xcafebabe, 0x76543210, 0xdeadbeef, 0x2468ace0}};
};
static_assert(offsetof(Output, after) - offsetof(Output, indices) == 28, "Seven slots only");

struct Fixture
{
    std::vector<LightRecord> records;
    std::vector<Candidate> candidates;
    explicit Fixture(std::size_t count) : records(count), candidates(count)
    {
        for (std::size_t index = 0; index < count; ++index)
        {
            records[index].fill(static_cast<std::uint8_t>(index % 251));
            candidates[index] = {1000u + index, static_cast<std::uint32_t>(index), Kind::Lamp,
                                 100.0f + static_cast<float>(index), true, true, 0};
        }
    }
    const void* Base() const { return records.data(); }
    std::uint32_t Count() const { return static_cast<std::uint32_t>(records.size()); }
    void ObserveAll(ShadowAllocationPass& pass) const
    {
        for (const auto& candidate : candidates) pass.Observe(candidate, records[candidate.index].data());
    }
    Output NativeOutput() const
    {
        Output output;
        for (std::size_t slot = 0; slot < (std::min)(records.size(), output.indices.size()); ++slot)
            output.indices[slot] = static_cast<std::int32_t>(slot);
        return output;
    }
};

static Frame At(std::uint64_t serial, std::uint32_t time = 100)
{
    return {serial, time, 0x10000, false};
}
static bool SameBytes(const Output& a, const Output& b)
{
    return std::memcmp(&a, &b, sizeof(a)) == 0;
}
static void CheckCanaries(const Output& output)
{
    const Output canaries;
    CHECK(output.before == canaries.before);
    CHECK(output.after == canaries.after);
}
static void CheckCurrentUnique(const Output& output, const Fixture& fixture)
{
    CheckCanaries(output);
    std::set<std::int32_t> seen;
    for (const auto index : output.indices)
    {
        CHECK(index >= -1);
        if (index == -1) continue;
        CHECK(static_cast<std::size_t>(index) < fixture.records.size());
        CHECK(seen.insert(index).second);
        CHECK(std::any_of(fixture.candidates.begin(), fixture.candidates.end(),
            [=](const Candidate& candidate) { return candidate.index == static_cast<std::uint32_t>(index); }));
    }
}
static bool Contains(const Output& output, std::int32_t index)
{
    return std::find(output.indices.begin(), output.indices.end(), index) != output.indices.end();
}
static void BeginAndObserve(ShadowAllocationPass& pass, const Fixture& fixture, Frame frame = At(1))
{
    pass.Begin(frame, fixture.Base(), fixture.Count());
    fixture.ObserveAll(pass);
}
static Output CommitFixture(ShadowAllocationPass& pass, const Fixture& fixture, Frame frame)
{
    BeginAndObserve(pass, fixture, frame);
    auto output = fixture.NativeOutput();
    CHECK(pass.Commit(fixture.Base(), fixture.Count(), output.indices.data()));
    CheckCurrentUnique(output, fixture);
    return output;
}

static void WritesOnlySevenIndices()
{
    Fixture fixture(12);
    fixture.candidates[11].distanceSquared = 0.1f;
    const auto recordsBefore = fixture.records;
    ShadowAllocationPass pass;
    auto output = fixture.NativeOutput();
    const auto before = output;
    BeginAndObserve(pass, fixture);
    CHECK(pass.Active());
    CHECK(pass.Commit(fixture.Base(), fixture.Count(), output.indices.data()));
    CHECK(!pass.Active());
    CHECK(output.indices != before.indices);
    CHECK(Contains(output, 11));
    CheckCurrentUnique(output, fixture);
    CHECK(fixture.records == recordsBefore);
    const auto committed = output;
    CHECK(!pass.Commit(fixture.Base(), fixture.Count(), output.indices.data()));
    CHECK(SameBytes(output, committed));
}

static void RejectedTransactionsAreByteExact()
{
    for (unsigned scenario = 0; scenario < 15; ++scenario)
    {
        Fixture fixture(8);
        ShadowAllocationPass pass;
        auto output = fixture.NativeOutput();
        const auto before = output;
        if (scenario != 0) BeginAndObserve(pass, fixture);
        bool committed = false;
        switch (scenario)
        {
            case 0: committed = pass.Commit(fixture.Base(), fixture.Count(), output.indices.data()); break;
            case 1: pass.Cancel(); committed = pass.Commit(fixture.Base(), fixture.Count(), output.indices.data()); break;
            case 2: committed = pass.Commit(fixture.Base(), fixture.Count() + 1, output.indices.data()); break;
            case 3: { const auto other = fixture.records; committed = pass.Commit(other.data(), fixture.Count(), output.indices.data()); break; }
            case 4: fixture.records[7][0x48] ^= 0x04; committed = pass.Commit(fixture.Base(), fixture.Count(), output.indices.data()); break;
            case 5: fixture.records[0][0x60] ^= 0x01; committed = pass.Commit(fixture.Base(), fixture.Count(), output.indices.data()); break;
            case 6: pass.EndInvocation(); committed = pass.Commit(fixture.Base(), fixture.Count(), output.indices.data()); break;
            case 7: committed = pass.Commit(fixture.Base(), fixture.Count(), nullptr); break;
            case 8: committed = pass.Commit(nullptr, fixture.Count(), output.indices.data()); break;
            case 9: pass.Begin(At(2), nullptr, 8); committed = pass.Commit(nullptr, 8, output.indices.data()); break;
            case 10: pass.Begin(At(2), fixture.Base(), 4097); committed = pass.Commit(fixture.Base(), 4097, output.indices.data()); break;
            case 11: fixture.records[7][0x7f] ^= 0x80; committed = pass.Commit(fixture.Base(), fixture.Count(), output.indices.data()); break;
            case 12: pass.Begin(At(2), fixture.Base(), fixture.Count()); committed = pass.Commit(fixture.Base(), fixture.Count(), output.indices.data()); break;
            case 13: pass.Begin(At(2), fixture.Base(), UINT32_MAX); committed = pass.Commit(fixture.Base(), UINT32_MAX, output.indices.data()); break;
            case 14:
            {
                // No allocation exists at this address. Range rejection must
                // happen before the bridge can read even one candidate byte.
                const auto* overflowing = reinterpret_cast<const void*>(UINTPTR_MAX - 63u);
                pass.Begin(At(2), overflowing, 1);
                CHECK(!pass.Active());
                committed = pass.Commit(overflowing, 1, output.indices.data());
                break;
            }
        }
        CHECK(!committed);
        CHECK(SameBytes(output, before));
        CHECK(!pass.Active());
    }
}

static void NativeOutputValidation()
{
    for (unsigned scenario = 0; scenario < 4; ++scenario)
    {
        Fixture fixture(8);
        ShadowAllocationPass pass;
        BeginAndObserve(pass, fixture);
        auto output = fixture.NativeOutput();
        switch (scenario)
        {
            case 0: output.indices[3] = output.indices[2]; break;
            case 1: output.indices[3] = -2; break;
            case 2: output.indices[3] = 8; break;
            case 3: output.indices[3] = INT32_MAX; break;
        }
        const auto before = output;
        CHECK(!pass.Commit(fixture.Base(), fixture.Count(), output.indices.data()));
        CHECK(SameBytes(output, before));
    }
    Fixture fixture(8);
    ShadowAllocationPass pass;
    pass.Begin(At(1), fixture.Base(), fixture.Count());
    pass.Observe(fixture.candidates[0], fixture.records[0].data());
    auto output = fixture.NativeOutput();
    const auto before = output;
    CHECK(!pass.Commit(fixture.Base(), fixture.Count(), output.indices.data()));
    CHECK(SameBytes(output, before));
}

static void BadObservations()
{
    for (unsigned scenario = 0; scenario < 13; ++scenario)
    {
        Fixture fixture(8);
        ShadowAllocationPass pass;
        pass.Begin(At(1), fixture.Base(), fixture.Count());
        auto candidate = fixture.candidates[0];
        const void* source = fixture.records[0].data();
        switch (scenario)
        {
            case 0: source = nullptr; break;
            case 1: source = fixture.records[1].data(); break;
            case 2: candidate.index = 8; break;
            case 3: candidate.index = UINT32_MAX; break;
            case 4: candidate.key = 0; break;
            case 5: candidate.distanceSquared = -1; break;
            case 6: candidate.distanceSquared = std::numeric_limits<float>::quiet_NaN(); break;
            case 7: candidate.distanceSquared = std::numeric_limits<float>::infinity(); break;
            case 8: pass.Observe(candidate, source); break; // exact duplicate boundary hit
            case 9: pass.Observe(fixture.candidates[1], fixture.records[1].data()); candidate.key = fixture.candidates[1].key; break;
            case 10: pass.Observe(candidate, source); candidate.key += 100; break;
            case 11: candidate.kind = static_cast<Kind>(255); break;
            case 12: candidate.engineEligible = false; break;
        }
        pass.Observe(candidate, source);
        Output output;
        const auto before = output;
        CHECK(!pass.Commit(fixture.Base(), fixture.Count(), output.indices.data()));
        CHECK(SameBytes(output, before));
    }
}

static void CapacityAndEmptyPass()
{
    Fixture full(512);
    ShadowAllocationPass fullPass;
    const auto output = CommitFixture(fullPass, full, At(1));
    CHECK(std::count(output.indices.begin(), output.indices.end(), -1) == 0);
    Fixture overflow(513);
    ShadowAllocationPass overflowPass;
    BeginAndObserve(overflowPass, overflow);
    auto original = overflow.NativeOutput();
    const auto before = original;
    CHECK(!overflowPass.Active());
    CHECK(!overflowPass.Commit(overflow.Base(), overflow.Count(), original.indices.data()));
    CHECK(SameBytes(original, before));
    ShadowAllocationPass emptyPass;
    LightRecord nonnullEmptyBacking{};
    Output empty;
    const auto beforeEmpty = empty;
    emptyPass.Begin(At(1), nonnullEmptyBacking.data(), 0);
    CHECK(emptyPass.Commit(nonnullEmptyBacking.data(), 0, empty.indices.data()));
    CHECK(SameBytes(empty, beforeEmpty));
    CHECK(!emptyPass.Active());
}

static void ShuffledCurrentIndicesOnly()
{
    Fixture fixture(20);
    ShadowAllocationPass pass;
    auto previous = CommitFixture(pass, fixture, At(0, 100));
    std::array<std::uint64_t, 7> selectedKeys{};
    for (std::size_t i = 0; i < selectedKeys.size(); ++i)
        selectedKeys[i] = fixture.candidates[static_cast<std::size_t>(previous.indices[i])].key;
    std::mt19937 random(0x9315);
    for (std::uint64_t frame = 1; frame <= 100; ++frame)
    {
        std::vector<std::uint32_t> indices(fixture.Count());
        std::iota(indices.begin(), indices.end(), 0u);
        std::shuffle(indices.begin(), indices.end(), random);
        for (std::size_t i = 0; i < fixture.candidates.size(); ++i)
            fixture.candidates[i].index = indices[i];
        std::shuffle(fixture.candidates.begin(), fixture.candidates.end(), random);
        previous = CommitFixture(pass, fixture, At(frame, 100 + static_cast<std::uint32_t>(frame)));
        for (std::size_t slot = 0; slot < selectedKeys.size(); ++slot)
        {
            const auto current = std::find_if(fixture.candidates.begin(), fixture.candidates.end(),
                [&](const Candidate& c) { return c.key == selectedKeys[slot]; });
            CHECK(current != fixture.candidates.end());
            CHECK(previous.indices[slot] == static_cast<std::int32_t>(current->index));
        }
    }
}

static Fixture Competition()
{
    Fixture fixture(8);
    for (auto& candidate : fixture.candidates) candidate.distanceSquared = 100;
    fixture.candidates[7].distanceSquared = 1;
    return fixture;
}
static void SeedHistory(ShadowAllocationPass& pass)
{
    Fixture first(7);
    for (auto& candidate : first.candidates) candidate.distanceSquared = 100;
    const auto output = CommitFixture(pass, first, At(0, 100));
    CHECK(!Contains(output, 7));
    pass.EndInvocation();
}
static void HistoryAndEarlyReturns()
{
    // A quality/update-divisor early return keeps last actually published slots.
    ShadowAllocationPass early;
    SeedHistory(early);
    auto fixture = Competition();
    BeginAndObserve(early, fixture, At(1, 110));
    early.EndInvocation();
    CHECK(!early.Active());
    auto retained = CommitFixture(early, fixture, At(2, 120));
    CHECK(!Contains(retained, 7));

    // A canceled/failed pass discards history so a fresh closer candidate wins.
    for (unsigned failure = 0; failure < 3; ++failure)
    {
        ShadowAllocationPass pass;
        SeedHistory(pass);
        BeginAndObserve(pass, fixture, At(1, 110));
        if (failure == 0) pass.Cancel();
        else if (failure == 1)
        {
            auto output = fixture.NativeOutput();
            const auto before = output;
            CHECK(!pass.Commit(fixture.Base(), fixture.Count() + 1, output.indices.data()));
            CHECK(SameBytes(output, before));
        }
        else
        {
            pass.Observe(fixture.candidates[0], nullptr);
            pass.EndInvocation();
        }
        const auto fresh = CommitFixture(pass, fixture, At(2, 120));
        CHECK(Contains(fresh, 7));
    }
    // A new invocation must not reuse observations from an early return.
    ShadowAllocationPass noStale;
    BeginAndObserve(noStale, fixture, At(1, 110));
    noStale.EndInvocation();
    noStale.Begin(At(2, 120), fixture.Base(), fixture.Count());
    auto original = fixture.NativeOutput();
    const auto before = original;
    CHECK(!noStale.Commit(fixture.Base(), fixture.Count(), original.indices.data()));
    CHECK(SameBytes(original, before));
}

#if defined(_M_IX86)
__declspec(noinline) static void DisturbFloatingPointUnderGuard()
{
    const fusionfix::shadows::FloatingPointState guard;
    _mm_setcsr(0x5f80); // Different rounding control; all exceptions masked.
    __asm fldz
    __asm fld1
    __asm faddp st(1), st(0)
}
static void PreservesIncomingX87AndMxcsr()
{
    alignas(16) unsigned char original[512]{};
    alignas(16) unsigned char before[512]{};
    alignas(16) unsigned char after[512]{};
    unsigned short customControl = 0x077f;
    _fxsave(original);
    __asm fninit
    __asm fldcw customControl
    __asm fld1
    __asm fldpi
    _mm_setcsr(0x3f80);
    _fxsave(before);
    DisturbFloatingPointUnderGuard();
    _fxsave(after);
    _fxrstor(original); // Do not leave the test's deliberately nonempty x87 stack active.
    CHECK(std::memcmp(before, after, 24) == 0); // x87 control/status/tag and instruction/data pointers
    CHECK(std::memcmp(before + 24, after + 24, 4) == 0); // MXCSR
    for (unsigned slot = 0; slot < 8; ++slot)
        CHECK(std::memcmp(before + 32 + slot * 16, after + 32 + slot * 16, 10) == 0);
}
#endif

int main()
{
    WritesOnlySevenIndices();
    RejectedTransactionsAreByteExact();
    NativeOutputValidation();
    BadObservations();
    CapacityAndEmptyPass();
    ShuffledCurrentIndicesOnly();
    HistoryAndEarlyReturns();
#if defined(_M_IX86)
    PreservesIncomingX87AndMxcsr();
#endif
    std::cout << "Shadow allocation pass: " << checks << " offline checks passed; no game access.\n";
    return 0;
}
