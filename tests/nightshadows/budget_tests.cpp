#include "../../source/PlayerShadowBudget.hpp"

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <random>
#include <set>
#include <vector>

using namespace fusionfix::shadows::budget;
static unsigned checks{};
#define CHECK(expression) do { ++checks; if (!(expression)) { std::cerr << __LINE__ << ": " #expression "\n"; std::exit(1); } } while (false)

static Candidate Light(std::uint64_t key, std::uint32_t index, Kind kind, float distance, bool influences = true)
{
    return {key, index, kind, distance, influences, true, 0};
}

static Frame At(std::uint64_t frame, bool driving = false)
{
    return {frame, static_cast<std::uint32_t>(frame * 16), 100, driving};
}

static std::set<std::uint64_t> Keys(const PlayerShadowBudget::Selection& selection)
{
    std::set<std::uint64_t> result;
    for (const auto& slot : selection.slots) if (slot.key) result.insert(slot.key);
    return result;
}

static void CheckCurrent(const PlayerShadowBudget::Selection& selection, const std::vector<Candidate>& input)
{
    std::set<std::uint32_t> indices;
    std::set<std::uint64_t> keys;
    unsigned count = 0, beams = 0;
    for (std::size_t i = 0; i < selection.slots.size(); ++i)
    {
        const auto& slot = selection.slots[i];
        if (!(selection.validMask & (1u << i)))
        {
            CHECK(!slot.key && slot.index == PlayerShadowBudget::InvalidIndex);
            continue;
        }
        CHECK(slot.key && slot.index != PlayerShadowBudget::InvalidIndex);
        CHECK(indices.insert(slot.index).second);
        CHECK(keys.insert(slot.key).second);
        CHECK(std::any_of(input.begin(), input.end(), [&](const Candidate& c) {
            return c.key == slot.key && c.index == slot.index && c.kind == slot.kind &&
                c.generation == slot.generation && c.engineEligible;
        }));
        if (slot.kind != Kind::Lamp) ++beams;
        ++count;
    }
    CHECK(count == selection.count && count <= 7 && beams <= 2);
}

static PlayerShadowBudget::Selection Run(PlayerShadowBudget& budget, Frame frame, const std::vector<Candidate>& input)
{
    budget.BeginPass(frame);
    for (const auto& candidate : input) budget.Add(candidate);
    const auto result = budget.Finalize();
    CheckCurrent(result, input);
    return result;
}

static void PrioritiesAndTransitions()
{
    PlayerShadowBudget budget;
    std::vector<Candidate> lights{
        Light(1,0,Kind::Lamp,900), Light(2,1,Kind::Lamp,1000),
        Light(3,2,Kind::PlayerBeam,25,false), Light(4,3,Kind::OtherBeam,1),
        Light(5,4,Kind::OtherBeam,2)
    };
    for (std::uint64_t id = 10; id < 230; ++id)
        lights.push_back(Light(id, static_cast<std::uint32_t>(id), Kind::OtherBeam, 3));
    for (std::uint64_t id = 300; id < 310; ++id)
        lights.push_back(Light(id, static_cast<std::uint32_t>(id), Kind::Lamp, 1, false));
    auto selected = Run(budget, At(0, true), lights);
    CHECK(selected.count == 7);
    CHECK(selected.safeToApply);
    auto keys = Keys(selected);
    CHECK(keys.count(1) && keys.count(2) && keys.count(3));
    CHECK(!keys.count(4) && !keys.count(5));
    selected = Run(budget, At(1, false), lights);
    keys = Keys(selected);
    CHECK(keys.count(1) && keys.count(2) && keys.count(4) && keys.count(5));
    CHECK(!keys.count(3)); // Player's old beam does not reach Niko.
    lights[2].influencesPlayer = true;
    selected = Run(budget, At(2, false), lights);
    keys = Keys(selected);
    CHECK(keys.count(1) && keys.count(2) && keys.count(3) && keys.count(4));
    CHECK(!keys.count(5)); // Max two beams despite many close traffic requests.
    lights[2].kind = Kind::OtherBeam;
    lights[3].kind = Kind::PlayerBeam;
    selected = Run(budget, At(3, true), lights);
    keys = Keys(selected);
    CHECK(keys.count(1) && keys.count(2) && keys.count(4) && !keys.count(3));
}

static void IndicesAbsenceAndReuse()
{
    PlayerShadowBudget budget;
    std::vector<Candidate> lights;
    for (std::uint64_t id = 1; id <= 7; ++id)
        lights.push_back(Light(id, static_cast<std::uint32_t>(id), Kind::Lamp, static_cast<float>(id)));
    const auto before = Run(budget, At(0), lights);
    for (auto& light : lights) light.index += 1000;
    auto after = Run(budget, At(1), lights);
    for (std::size_t i = 0; i < 7; ++i)
    {
        CHECK(before.slots[i].key == after.slots[i].key);
        CHECK(before.slots[i].index + 1000 == after.slots[i].index);
    }
    lights.erase(lights.begin() + 2);
    lights.push_back(Light(99, 9900, Kind::Lamp, 2));
    after = Run(budget, At(2), lights);
    CHECK(!Keys(after).count(3) && Keys(after).count(99)); // no stale grace indices
    // Reused key describes a different kind/generation; only current data wins.
    lights[0].kind = Kind::PlayerBeam;
    lights[0].generation = 25;
    lights[0].index = 9001;
    after = Run(budget, At(3, true), lights);
    CHECK(Keys(after).count(1));
    for (const auto& slot : after.slots) if (slot.key == 1)
        CHECK(slot.kind == Kind::PlayerBeam && slot.generation == 25 && slot.index == 9001);
    lights.clear();
    after = Run(budget, At(4), lights);
    CHECK(after.count == 0 && after.validMask == 0);
    budget.Reset();
    CHECK(budget.Finalize().count == 0);
}

static void RejectionTests()
{
    PlayerShadowBudget budget;
    auto isolated = Run(budget, At(0), {Light(0, 0, Kind::Lamp, 1)});
    CHECK(isolated.unsupportedIdentity && !isolated.safeToApply && !isolated.invalidInput && !isolated.ambiguousRecords);
    isolated = Run(budget, At(1), {Light(1, 0, Kind::Lamp, 1), Light(1, 1, Kind::Lamp, 1)});
    CHECK(isolated.ambiguousRecords == 2 && !isolated.safeToApply && !isolated.unsupportedIdentity && !isolated.invalidInput);
    isolated = Run(budget, At(2), {Light(1, 0, Kind::Lamp, std::numeric_limits<float>::quiet_NaN())});
    CHECK(isolated.invalidInput && !isolated.safeToApply && !isolated.unsupportedIdentity && !isolated.ambiguousRecords);
    std::vector<Candidate> lights{
        Light(0, 0, Kind::Lamp, 1),
        Light(1, 1, Kind::Lamp, 1), Light(1, 2, Kind::Lamp, 1), // ambiguous key
        Light(2, 2, Kind::Lamp, 1), // shares second conflicting index
        Light(3, 3, Kind::Lamp, 1), Light(4, 3, Kind::Lamp, 1), // ambiguous index
        Light(5, 5, Kind::Lamp, 1), Light(5, 5, Kind::Lamp, 1), // exact duplicate
        Light(6, 6, Kind::Lamp, std::numeric_limits<float>::quiet_NaN()),
        Light(7, 7, Kind::Lamp, -1), Light(8, 8, Kind::Lamp, 9999),
        Light(9, 9, Kind::PlayerBeam, 1500),
        Light(10, 10, Kind::Lamp, 1),
        Light(11, PlayerShadowBudget::InvalidIndex, Kind::Lamp, 1)
    };
    lights[12].engineEligible = false;
    auto result = Run(budget, At(0), lights);
    CHECK(result.count == 1 && Keys(result).count(5));
    CHECK(result.ambiguousRecords == 5);
    CHECK(!result.safeToApply && result.unsupportedIdentity && result.invalidInput);
    std::mt19937 random(42);
    for (unsigned i = 0; i < 100; ++i)
    {
        std::shuffle(lights.begin(), lights.end(), random);
        budget.Reset();
        result = Run(budget, At(i), lights);
        CHECK(result.count == 1 && Keys(result).count(5) && result.ambiguousRecords == 5);
    }
    lights.clear();
    for (std::uint64_t i = 1; i <= PlayerShadowBudget::InputCapacity + 1; ++i)
        lights.push_back(Light(i, static_cast<std::uint32_t>(i), Kind::Lamp, 1));
    result = Run(budget, At(101), lights);
    CHECK(result.inputOverflow && result.count == 0);
    CHECK(!result.safeToApply);
    lights.resize(8);
    result = Run(budget, At(102), lights);
    CHECK(!result.inputOverflow && result.count == 7); // clean recovery next pass
    CHECK(result.safeToApply);
}

static void HysteresisAndClock()
{
    PlayerShadowBudget budget;
    std::vector<Candidate> lights;
    for (std::uint64_t i = 1; i <= 7; ++i)
        lights.push_back(Light(i, static_cast<std::uint32_t>(i), Kind::Lamp, 100));
    auto result = Run(budget, At(0), lights);
    const auto stable = result;
    lights.push_back(Light(99, 99, Kind::Lamp, 98));
    for (std::uint64_t frame = 1; frame < 100; ++frame)
    {
        lights[0].distanceSquared = frame % 2 ? 100.0f : 101.0f;
        result = Run(budget, At(frame), lights);
        CHECK(Keys(result) == Keys(stable));
        for (std::size_t i = 0; i < 7; ++i) CHECK(result.slots[i].key == stable.slots[i].key);
    }
    lights.back().distanceSquared = 1;
    result = Run(budget, At(100), lights);
    CHECK(Keys(result).count(99)); // material improvement after hold may replace
    // Session and clock discontinuities remove historical preference.
    lights[0].distanceSquared = 2;
    auto frame = At(101);
    frame.session = 200;
    result = Run(budget, frame, lights);
    CHECK(Keys(result).count(99) && Keys(result).count(1));
    frame.serial++;
    frame.timeMs = 1;
    result = Run(budget, frame, lights);
    CHECK(Keys(result).count(99));
    frame.serial++;
    frame.timeMs = std::numeric_limits<std::uint32_t>::max() - 15;
    result = Run(budget, frame, lights);
    const auto preWrap = result;
    frame.serial++;
    frame.timeMs = 0;
    result = Run(budget, frame, lights);
    for (std::size_t i = 0; i < 7; ++i) CHECK(result.slots[i].key == preWrap.slots[i].key);
}

static void ShuffledCurrentListStress()
{
    for (unsigned seed = 0; seed < 120; ++seed)
    {
        std::mt19937 random(seed);
        PlayerShadowBudget ordered, shuffled;
        for (std::uint64_t frame = 0; frame < 90; ++frame)
        {
            std::vector<Candidate> lights;
            for (std::uint64_t id = 1; id <= 240; ++id)
            {
                if ((id + frame) % 19 == 0) continue;
                const auto kind = id < 12 ? Kind::Lamp : id == 12 ? Kind::PlayerBeam : Kind::OtherBeam;
                lights.push_back(Light(id, static_cast<std::uint32_t>(lights.size()), kind,
                    static_cast<float>((id * 11 + frame % 5) % 900), (id + frame / 15) % 3 != 0));
            }
            // Randomize actual engine indices independently of stable IDs.
            std::shuffle(lights.begin(), lights.end(), random);
            for (std::size_t i = 0; i < lights.size(); ++i) lights[i].index = static_cast<std::uint32_t>(i);
            auto altered = lights;
            std::shuffle(altered.begin(), altered.end(), random);
            const bool driving = frame >= 30 && frame < 60;
            const auto a = Run(ordered, At(frame, driving), lights);
            const auto b = Run(shuffled, At(frame, driving), altered);
            CHECK(a.count == b.count && a.validMask == b.validMask);
            CHECK(a.safeToApply && b.safeToApply);
            for (std::size_t i = 0; i < 7; ++i)
                CHECK(a.slots[i].key == b.slots[i].key && a.slots[i].index == b.slots[i].index);
            if (driving) for (const auto& slot : b.slots) CHECK(!slot.key || slot.kind != Kind::OtherBeam);
        }
    }
}

int main()
{
    PrioritiesAndTransitions();
    IndicesAbsenceAndReuse();
    RejectionTests();
    HysteresisAndClock();
    ShuffledCurrentListStress();
    std::cout << "PASS: " << checks << " seven-slot budget checks; 120 shuffled 90-pass scenes with 200+ traffic entries.\n";
}
