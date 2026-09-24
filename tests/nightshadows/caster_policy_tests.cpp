// Reuse the existing disk PE fixture and run its base adapter checks too.
#define main BaseAdapterTests
#include "adapter_guard_tests.cpp"
#undef main
#include "../../source/HeadlightCasterPolicy.hpp"
#include "../../source/ShadowCasterCE.hpp"
#include "../../source/ShadowGuardDiagnostics.hpp"

namespace policy = fusionfix::shadows::caster;
namespace guard = ce::casterguard;

int main(int argc, char** argv)
{
    CHECK(BaseAdapterTests(argc, argv) == 0);
    policy::Context occupied{};
    occupied.car = 0x20000;
    occupied.ownBeam = true;
    occupied.occupants = {0x30000,0x31000,0x32000,0x33000,0x34000,0x35000,0x36000,0x37000,0x38000};
    for (std::uint32_t slot = 0; slot <= 9; ++slot)
        for (std::uint32_t kind = 0; kind <= 5; ++kind)
            for (unsigned active = 0; active <= 1; ++active)
            {
                const bool expected = slot >= 1 && slot <= 7 && kind == 4 && active;
                CHECK(policy::OwnBeam(slot,kind,active != 0,occupied.car,occupied.car) == expected);
                CHECK(policy::OwnBeam(slot,kind,active != 0,occupied.car+1,occupied.car) == expected);
                CHECK(!policy::OwnBeam(slot,kind,active != 0,occupied.car+2,occupied.car));
                CHECK(!policy::OwnBeam(slot,kind,active != 0,occupied.car,0));
            }
    for (unsigned type = 0; type < 16; ++type)
    {
        CHECK(policy::Exclude(occupied, occupied.car, type, true) == (type == 2));
        CHECK(!policy::Exclude(occupied, occupied.car, type, false));
        CHECK(!policy::Exclude(occupied, 0, type, true));
        CHECK(!policy::Exclude(occupied, 0x90000, type, true));
        for (const auto occupant : occupied.occupants)
        {
            CHECK(policy::Exclude(occupied, occupant, type, true) == (type == 3));
            CHECK(!policy::Exclude(occupied, occupant, type, false));
        }
    }
    auto lamp = occupied;
    lamp.ownBeam = false;
    CHECK(!policy::Exclude(lamp, occupied.car, 2, true));
    CHECK(!policy::Exclude(lamp, occupied.occupants[0], 3, true));
    policy::Context current{};
    for (int frame = 0; frame < 100; ++frame)
    {
        {
            const policy::Scope own(current,occupied);
            CHECK(policy::Exclude(current, occupied.car, 2, true));
            {
                const policy::Scope other(current, lamp);
                CHECK(!policy::Exclude(current, occupied.car, 2, true));
                { const policy::Scope invalid(current, {}); CHECK(!current.ownBeam); }
                CHECK(current.car == lamp.car && !current.ownBeam);
            }
            CHECK(policy::Exclude(current, occupied.occupants[0], 3, true));
        }
        CHECK(!current.ownBeam && current.car == 0);
        CHECK(!policy::Exclude(current, occupied.occupants[0], 3, true));
    }
    auto mapped = MapFile(argv[1]);
    CHECK(guard::Validate(mapped.data(), mapped.size(), ce::PreferredBase));
    auto details = ce::diagnostics::Describe(mapped.data(), mapped.size(), ce::PreferredBase);
    CHECK(details.find("allocation_guard=1 caster_guard=1") != std::string::npos);
    CHECK(details.find("mismatch=") == std::string::npos);
    CHECK(ce::diagnostics::Describe(nullptr, mapped.size(), ce::PreferredBase) == "invalid_image_span");
    CHECK(ce::diagnostics::Describe(mapped.data(), 4, ce::PreferredBase) == "invalid_image_span");
    for (const auto& range : ce::allocation::detail::GuardedRanges)
    {
        mapped[range.rva] ^= 1;
        details = ce::diagnostics::Describe(mapped.data(), mapped.size(), ce::PreferredBase);
        CHECK(details.find("allocation_guard=0") != std::string::npos);
        CHECK(details.find("mismatch=") != std::string::npos);
        mapped[range.rva] ^= 1;
    }
    // The PE image-base/header check is separate from the compared ranges.
    Write32(mapped, 0x148+24+28, 0x10000000);
    details = ce::diagnostics::Describe(mapped.data(), mapped.size(), ce::PreferredBase);
    CHECK(details.find("header_base=10000000") != std::string::npos);
    CHECK(details.find("allocation_guard=0") != std::string::npos);
    Write32(mapped, 0x148+24+28, ce::PreferredBase);
    // Reproduce the real source conflict without executing/injecting game code:
    // the official light-admission workaround NOPs this six-byte branch before
    // installing its mid hook. That alone invalidates the full-body guard.
    constexpr std::size_t admissionBranch = 0x527BD6;
    std::array<std::uint8_t,6> originalBranch{};
    std::memcpy(originalBranch.data(),mapped.data()+admissionBranch,originalBranch.size());
    CHECK(originalBranch[0] == 0x0F && originalBranch[1] == 0x84);
    CHECK(guard::Validate(mapped.data(),mapped.size(),ce::PreferredBase));
    std::fill_n(mapped.data()+admissionBranch,originalBranch.size(),std::uint8_t{0x90});
    CHECK(!ce::allocation::ValidateMappedImage(mapped.data(),mapped.size(),ce::PreferredBase));
    CHECK(!guard::Validate(mapped.data(),mapped.size(),ce::PreferredBase));
    details=ce::diagnostics::Describe(mapped.data(),mapped.size(),ce::PreferredBase);
    CHECK(details.find("mismatch=527bd6:f>90") != std::string::npos);
    std::memcpy(mapped.data()+admissionBranch,originalBranch.data(),originalBranch.size());
    CHECK(guard::Validate(mapped.data(),mapped.size(),ce::PreferredBase));
    CHECK(!guard::Validate(nullptr, mapped.size(), ce::PreferredBase));
    for (const auto& range : guard::Ranges)
    {
        CHECK(ce::allocation::detail::ValidMetadata(range));
        for (std::size_t byte = 0; byte < range.size; ++byte)
            for (unsigned bit = 0; bit < 8; ++bit)
            {
                mapped[range.rva+byte] ^= static_cast<std::uint8_t>(1u << bit);
                CHECK(!guard::Validate(mapped.data(),mapped.size(),ce::PreferredBase));
                mapped[range.rva+byte] ^= static_cast<std::uint8_t>(1u << bit);
            }
        // Relocate each guarded range, including every absolute operand.
        auto shifted = mapped;
        constexpr std::uint32_t otherBase = 0x10000000;
        for (std::size_t r = 0; r < range.relocationCount; ++r)
        {
            const auto offset = range.rva + range.relocationOffsets[r];
            Write32(shifted, offset, ce::Read32(mapped.data()+offset)-ce::PreferredBase+otherBase);
        }
        CHECK(ce::allocation::detail::MatchRelocatedRange(shifted.data(),shifted.size(),otherBase,range));
        CHECK(!ce::allocation::detail::MatchRelocatedRange(mapped.data(),range.rva+range.size-1,ce::PreferredBase,range));
    }
    CHECK(guard::Validate(mapped.data(), mapped.size(), ce::PreferredBase));
    std::cout << "Caster policy, guard and startup diagnostics PASS; total checks=" << checks << '\n';
}
