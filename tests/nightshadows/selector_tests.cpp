#include "../../source/StableHeadlightSelector.hpp"
#include "../../source/ShadowAdapterCE.hpp"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <random>
#include <set>
#include <vector>

using namespace fusionfix::shadows;

static unsigned checks = 0;
#define CHECK(expression) do { ++checks; if (!(expression)) { std::cerr << __LINE__ << ": " #expression "\n"; std::exit(1); } } while (false)

static FrameContext Frame(std::uint64_t index, std::uint32_t time = 0, bool driving = false, std::uintptr_t car = 0)
{
    return { index, time, 101, driving, car };
}

static HeadlightCandidate Light(std::uintptr_t id, float squaredDistance, bool player = false, int direction = 0)
{
    return { id, { squaredDistance, direction != 0, direction > 0 }, player };
}

static std::array<std::uintptr_t, 2> Set(const StableHeadlightSelector& selector)
{
    auto ids = selector.ActiveIdentities();
    std::sort(ids.begin(), ids.end());
    return ids;
}

static void Expect(const StableHeadlightSelector& selector, std::uintptr_t a, std::uintptr_t b)
{
    std::array<std::uintptr_t, 2> wanted{a,b};
    std::sort(wanted.begin(), wanted.end());
    CHECK(Set(selector) == wanted);
}

static void GeometryTests()
{
    const Vec3 forward{0, 1, 0};
    auto result = EvaluateGeometry({903, -370, 16}, {903, -375, 16}, &forward);
    CHECK(result.distanceSquared == 25 && result.directionKnown && result.aimedAtPlayer);
    result = EvaluateGeometry({903, -380, 16}, {903, -375, 16}, &forward);
    CHECK(result.distanceSquared == 25 && result.directionKnown && !result.aimedAtPlayer);
    result = EvaluateGeometry({903, -380, 16}, {903, -375, 16});
    CHECK(!result.directionKnown && result.distanceSquared == 25);
    const Vec3 zero{};
    CHECK(!EvaluateGeometry({2,2,2}, {1,1,1}, &zero).directionKnown);
    const Vec3 invalid{std::numeric_limits<float>::quiet_NaN(), 1, 1};
    CHECK(!EvaluateGeometry({2,2,2}, {1,1,1}, &invalid).directionKnown);
    const Vec3 huge{0, std::numeric_limits<float>::max(), 0};
    CHECK(EvaluateGeometry({1,100,1}, {1,1,1}, &huge).aimedAtPlayer);
    const Vec3 infinite{0, std::numeric_limits<float>::infinity(), 0};
    CHECK(!EvaluateGeometry({1,100,1}, {1,1,1}, &infinite).directionKnown);
    CHECK(!EvaluateGeometry({std::numeric_limits<float>::quiet_NaN(),1,1}, {1,1,1}, &forward).directionKnown);
}

static void CapacityAndIdentityTests()
{
    StableHeadlightSelector selector;
    selector.BeginFrame(Frame(0));
    CHECK(!selector.Consider(Light(0, 1)));
    CHECK(!selector.Consider(Light(3, std::numeric_limits<float>::infinity())));
    CHECK(!selector.Consider(Light(4, std::numeric_limits<float>::quiet_NaN())));
    CHECK(!selector.Consider(Light(5, -1)));
    // More than the reservoir capacity; exact ties choose stable IDs.
    for (std::uintptr_t i = 200; i != 0; --i) CHECK(!selector.Consider(Light(i, 100)));
    selector.BeginFrame(Frame(1)); // Same millisecond still is a new render frame.
    Expect(selector, 1, 2);
    const auto active = Set(selector);
    std::set<std::uintptr_t> accepted;
    for (std::uintptr_t i = 200; i != 0; --i)
    {
        if (selector.Consider(Light(i, static_cast<float>(i)))) accepted.insert(i);
        CHECK(Set(selector) == active);
    }
    CHECK(accepted.size() == 2 && accepted.count(1) && accepted.count(2));
    // Duplicate observations consume one identity, regardless of score order.
    selector.Reset();
    selector.BeginFrame(Frame(0));
    selector.Consider(Light(5, 100));
    selector.Consider(Light(5, 1));
    selector.Consider(Light(5, 20));
    selector.Consider(Light(6, 2));
    selector.BeginFrame(Frame(1, 16));
    Expect(selector, 5, 6);
    // Reject each invalid call; keep a same-frame valid observation regardless
    // of duplicate order, rather than letting the last duplicate control state.
    CHECK(!selector.Consider(Light(5, 1400)));
    CHECK(selector.Consider(Light(5, 1)));
    CHECK(!selector.Consider(Light(5, std::numeric_limits<float>::quiet_NaN())));
    CHECK(selector.Consider(Light(6, 2)));
    CHECK(!selector.Consider(Light(6, 1400)));
    selector.BeginFrame(Frame(2, 32));
    Expect(selector, 5, 6);
}

static void DrivingTransitions()
{
    StableHeadlightSelector selector;
    selector.BeginFrame(Frame(0));
    selector.Consider(Light(1, 1));
    selector.Consider(Light(2, 2));
    selector.BeginFrame(Frame(1, 16));
    Expect(selector, 1, 2);
    selector.Consider(Light(1, 1));
    selector.Consider(Light(2, 2));
    selector.BeginFrame(Frame(2, 32, true, 1000));
    Expect(selector, 0, 0);
    CHECK(!selector.Consider(Light(1, 1)));
    CHECK(!selector.Consider(Light(2, 2)));
    selector.Consider(Light(10, 25, true));
    selector.Consider(Light(11, 26, true));
    selector.BeginFrame(Frame(3, 48, true, 1000));
    Expect(selector, 10, 11);
    CHECK(selector.Consider(Light(10, 25, true)));
    CHECK(!selector.Consider(Light(10, 25, false))); // Current tag always gates.
    CHECK(!selector.Consider(Light(11, 2000, true))); // No distant shadow on grace.
    CHECK(!selector.Consider(Light(3, 0.1f)));
    selector.BeginFrame(Frame(4, 64, true, 2000));
    Expect(selector, 0, 0);
    selector.Consider(Light(20, 4, true));
    selector.Consider(Light(21, 5, true));
    selector.BeginFrame(Frame(5, 80, true, 2000));
    Expect(selector, 20, 21);
    selector.Consider(Light(20, 4, true));
    selector.Consider(Light(21, 5, true));
    selector.BeginFrame(Frame(6, 96));
    Expect(selector, 20, 21); // Exit keeps the still-eligible pair.
    CHECK(selector.Consider(Light(20, 4, false)));
    CHECK(selector.Consider(Light(21, 5, false)));

    // Conflicting BeginFrame on same render serial cannot emit a new pair.
    selector.BeginFrame(Frame(6, 97, true, 3000));
    CHECK(!selector.Consider(Light(20, 4, true)));
    CHECK(!selector.Consider(Light(30, 2, true)));
    selector.BeginFrame(Frame(7, 112, true, 3000));
    Expect(selector, 0, 0);
}

static void HoldAimAndGraceTests()
{
    StableHeadlightSelector selector;
    selector.BeginFrame(Frame(0));
    selector.Consider(Light(1, 100));
    selector.Consider(Light(2, 101));
    selector.BeginFrame(Frame(1, 16));
    for (std::uint64_t f = 1; f < 80; ++f)
    {
        selector.Consider(Light(1, f % 2 ? 100.0f : 101.0f));
        selector.Consider(Light(2, f % 2 ? 101.0f : 100.0f));
        selector.Consider(Light(3, 98));
        selector.BeginFrame(Frame(f + 1, static_cast<std::uint32_t>((f + 1) * 16)));
        Expect(selector, 1, 2); // No churn after hold expires under small jitter.
    }
    selector.Consider(Light(1, 100));
    selector.Consider(Light(2, 101));
    selector.Consider(Light(3, 400, false, 1)); // Distant aimed light beats near unknown.
    selector.Consider(Light(4, 0.1f, false, -1)); // Looking away is lower priority.
    selector.BeginFrame(Frame(81, 1296));
    Expect(selector, 1, 3);
    CHECK(selector.Consider(Light(3, 400, false, 1)));
    selector.Consider(Light(1, 100));
    selector.BeginFrame(Frame(82, 1312));
    // One missed observation preserves ownership, but never bypasses eligibility.
    selector.Consider(Light(1, 100));
    selector.BeginFrame(Frame(83, 1328));
    Expect(selector, 1, 3);
    selector.Consider(Light(1, 100));
    selector.BeginFrame(Frame(84, 1344));
    Expect(selector, 1, 3);
    selector.Consider(Light(1, 100));
    selector.BeginFrame(Frame(85, 1360));
    Expect(selector, 0, 1);
    CHECK(!selector.Consider(Light(1, 1300))); // Explicit distance invalidation.
    selector.BeginFrame(Frame(86, 1376));
    Expect(selector, 0, 0);
}

static void ClockAndSessionTests()
{
    StableHeadlightSelector selector;
    constexpr auto end = std::numeric_limits<std::uint32_t>::max();
    selector.BeginFrame(Frame(100, end - 15));
    selector.Consider(Light(1, 1));
    selector.Consider(Light(2, 2));
    selector.BeginFrame(Frame(101, 0));
    Expect(selector, 1, 2); // uint32 time rollover is a normal 16ms interval.
    selector.Consider(Light(1, 1));
    selector.Consider(Light(2, 2));
    selector.BeginFrame(Frame(102, 16));
    Expect(selector, 1, 2);
    selector.BeginFrame(Frame(103, 8));
    Expect(selector, 0, 0); // Backward clock is a discontinuity.
    selector.Consider(Light(1, 1));
    selector.Consider(Light(2, 2));
    selector.BeginFrame(Frame(104, 5000));
    Expect(selector, 0, 0);
    selector.Consider(Light(1, 1));
    selector.Consider(Light(2, 2));
    auto otherSession = Frame(105, 5016);
    otherSession.session = 202;
    selector.BeginFrame(otherSession);
    Expect(selector, 0, 0);
    selector.Consider(Light(1, 1));
    selector.BeginFrame({0, 5032, 202, false, 0});
    Expect(selector, 0, 0);
}

static void VehicleBeamGeometryIntegration()
{
    StableHeadlightSelector selector;
    constexpr std::uintptr_t playerVehicle = 0x12340000, trafficVehicle = 0x12350000;
    const Vec3 niko{903, -376, 16}, forwards{0, 1, 0};
    const auto candidate = [&](std::uintptr_t key, Vec3 position, std::uintptr_t occupied) {
        return HeadlightCandidate{key, EvaluateGeometry(niko, position, &forwards), ce::IsVehicleBeam(key, occupied)};
    };
    selector.BeginFrame(Frame(0, 0, true, playerVehicle));
    CHECK(!selector.Consider(candidate(trafficVehicle, {903,-377,16}, playerVehicle)));
    CHECK(!selector.Consider(candidate(playerVehicle, {903,-380,16}, playerVehicle)));
    selector.BeginFrame(Frame(1, 16, true, playerVehicle));
    Expect(selector, 0, playerVehicle); // two physical lamps are one beam request
    CHECK(!selector.Consider(candidate(trafficVehicle, {903,-377,16}, playerVehicle)));
    CHECK(selector.Consider(candidate(playerVehicle, {903,-380,16}, playerVehicle)));
    // A destroyed lamp changes its request key. No pointer dereference occurs.
    selector.BeginFrame(Frame(2, 32, true, playerVehicle));
    CHECK(!selector.Consider(candidate(playerVehicle + 1, {903,-380,16}, playerVehicle)));
    selector.BeginFrame(Frame(3, 48, true, playerVehicle));
    CHECK(selector.Consider(candidate(playerVehicle + 1, {903,-380,16}, playerVehicle)));
    CHECK(!selector.Consider(candidate(playerVehicle + 2, {903,-380,16}, playerVehicle)));
    selector.BeginFrame(Frame(4, 64));
    CHECK(selector.Consider(candidate(playerVehicle + 1, {903,-380,16}, 0)));
    CHECK(!selector.Consider(candidate(playerVehicle + 1, {903,-440,16}, 0)));
}

static void ShuffledTrafficStress()
{
    // Each run compares independent selectors across the same evolving traffic,
    // transitions, duplicate IDs, saturation, and aim changes. This catches
    // order-dependent output without restating the selector implementation.
    for (unsigned seed = 0; seed < 160; ++seed)
    {
        std::mt19937 random(seed);
        StableHeadlightSelector ordered, shuffled;
        for (std::uint64_t f = 0; f < 120; ++f)
        {
            const bool driving = f >= 30 && f < 75;
            const auto car = driving ? (f >= 60 ? 2000U : 1000U) : 0U;
            auto frame = Frame(f, static_cast<std::uint32_t>(f * 16), driving, car);
            ordered.BeginFrame(frame);
            shuffled.BeginFrame(frame);
            CHECK(Set(ordered) == Set(shuffled));
            const auto frozen = Set(shuffled);
            std::vector<HeadlightCandidate> observations;
            for (std::uintptr_t id = 1; id <= 96; ++id)
            {
                if ((id + f) % 11 == 0) continue; // intermittent disappearance
                const float score = static_cast<float>((id * 37 + f * 3) % 1200);
                const bool player = car == 1000 ? id == 10 || id == 11 : car == 2000 ? id == 20 || id == 21 : false;
                const int direction = (id + f / 17) % 7 == 0 ? 1 : ((id % 3) == 0 ? -1 : 0);
                observations.push_back(Light(id, score, player, direction));
                observations.push_back(Light(id, score + 0.5f, player, direction));
            }
            std::set<std::uintptr_t> orderedAllowed, shuffledAllowed;
            for (const auto& light : observations) if (ordered.Consider(light)) orderedAllowed.insert(light.identity);
            std::shuffle(observations.begin(), observations.end(), random);
            for (const auto& light : observations)
            {
                if (shuffled.Consider(light)) shuffledAllowed.insert(light.identity);
                CHECK(Set(shuffled) == frozen);
            }
            CHECK(orderedAllowed == shuffledAllowed);
            CHECK(shuffledAllowed.size() <= 2);
            if (driving)
                for (const auto id : shuffledAllowed)
                    CHECK(car == 1000 ? id == 10 || id == 11 : id == 20 || id == 21);
        }
    }
}

int main()
{
    GeometryTests();
    CapacityAndIdentityTests();
    DrivingTransitions();
    HoldAimAndGraceTests();
    ClockAndSessionTests();
    VehicleBeamGeometryIntegration();
    ShuffledTrafficStress();
    std::cout << "PASS: " << checks << " checks; 160 shuffled 120-frame traffic/transition scenarios.\n";
}
