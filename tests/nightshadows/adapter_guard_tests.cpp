#include "../../source/ShadowAdapterCE.hpp"

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <vector>

namespace ce = fusionfix::shadows::ce;
static unsigned checks{};
#define CHECK(expression) do { ++checks; if (!(expression)) { std::cerr << __LINE__ << ": " #expression "\n"; std::exit(1); } } while (false)

static std::uint16_t Read16(const std::uint8_t* p)
{
    return static_cast<std::uint16_t>(p[0] | static_cast<std::uint16_t>(p[1]) << 8);
}

static void Write32(std::vector<std::uint8_t>& data, std::size_t offset, std::uint32_t value)
{
    CHECK(offset <= data.size() && 4 <= data.size() - offset);
    std::memcpy(data.data() + offset, &value, sizeof(value));
}

static std::vector<std::uint8_t> MapFile(const char* path)
{
    // Reads the executable as data. Never loads it as code or opens a process.
    std::ifstream input(path, std::ios::binary);
    CHECK(input.good());
    const std::vector<std::uint8_t> disk{std::istreambuf_iterator<char>(input), {}};
    CHECK(disk.size() >= 0x100);
    const auto pe = ce::Read32(disk.data() + 0x3C);
    CHECK(pe <= disk.size() - 0x100);
    CHECK(ce::Read32(disk.data() + pe) == 0x4550);
    const auto sections = Read16(disk.data() + pe + 6);
    const auto optionalSize = Read16(disk.data() + pe + 20);
    const auto imageSize = ce::Read32(disk.data() + pe + 24 + 56);
    const auto headersSize = ce::Read32(disk.data() + pe + 24 + 60);
    CHECK(imageSize == ce::ImageSize && headersSize <= disk.size() && headersSize <= imageSize);
    std::vector<std::uint8_t> mapped(imageSize);
    std::copy_n(disk.begin(), headersSize, mapped.begin());
    const std::size_t sectionTable = pe + 24 + optionalSize;
    CHECK(sectionTable <= disk.size() && static_cast<std::size_t>(sections) * 40 <= disk.size() - sectionTable);
    for (std::size_t i = 0; i < sections; ++i)
    {
        const auto* header = disk.data() + sectionTable + i * 40;
        const auto virtualAddress = ce::Read32(header + 12);
        const auto rawSize = ce::Read32(header + 16);
        const auto rawOffset = ce::Read32(header + 20);
        CHECK(rawOffset <= disk.size() && rawSize <= disk.size() - rawOffset);
        CHECK(virtualAddress <= mapped.size());
        // This CE executable declares a non-page-aligned SizeOfImage. The last
        // section includes padding through the end of the loader's rounded page.
        // Only the declared image span is relevant to the bounded guard.
        const auto copySize = (std::min)(static_cast<std::size_t>(rawSize), mapped.size() - virtualAddress);
        std::copy_n(disk.begin() + rawOffset, copySize, mapped.begin() + virtualAddress);
    }
    return mapped;
}

static bool Valid(const std::vector<std::uint8_t>& mapped, std::uintptr_t base = ce::PreferredBase)
{
    return ce::ValidateMappedImage(mapped.data(), mapped.size(), base);
}

static void MutateReject(std::vector<std::uint8_t>& mapped, std::size_t offset, std::size_t count,
                         std::uintptr_t base = ce::PreferredBase)
{
    for (std::size_t i = 0; i < count; ++i)
    {
        const auto original = mapped[offset + i];
        mapped[offset + i] ^= 0x01;
        CHECK(!Valid(mapped, base));
        mapped[offset + i] = original;
        CHECK(Valid(mapped, base));
    }
}

static void VehicleBeamTests()
{
    CHECK(!ce::IsVehicleBeam(0, 0));
    CHECK(!ce::IsVehicleBeam(1, 0));
    CHECK(!ce::IsVehicleBeam(0, 0x12340000));
    CHECK(ce::IsVehicleBeam(0x12340000, 0x12340000)); // merged two-lamp request
    CHECK(ce::IsVehicleBeam(0x12340001, 0x12340000)); // single working lamp
    CHECK(!ce::IsVehicleBeam(0x12340002, 0x12340000));
    CHECK(!ce::IsVehicleBeam(0x1233FFFF, 0x12340000));
    CHECK(!ce::IsVehicleBeam(0x12350000, 0x12340000)); // neighboring traffic vehicle
    constexpr auto maximum = std::numeric_limits<std::uintptr_t>::max();
    CHECK(ce::IsVehicleBeam(maximum, maximum));
    CHECK(!ce::IsVehicleBeam(0, maximum)); // addition must not wrap to zero
    CHECK(ce::IsVehicleBeam(maximum, maximum - 1));
    CHECK(!ce::IsVehicleBeam(1, maximum));
    for (std::uintptr_t owner = 0x10000000; owner < 0x10100000; owner += 0x1000)
    {
        CHECK(ce::IsVehicleBeam(owner, owner));
        CHECK(ce::IsVehicleBeam(owner + 1, owner));
        CHECK(!ce::IsVehicleBeam(owner - 1, owner));
        CHECK(!ce::IsVehicleBeam(owner + 2, owner));
        CHECK(!ce::IsVehicleBeam(owner + 0x1000, owner));
    }
}

int main(int argc, char** argv)
{
    CHECK(argc == 2);
    VehicleBeamTests();
    auto mapped = MapFile(argv[1]);
    CHECK(Valid(mapped));
    const auto pe = ce::Read32(mapped.data() + 0x3C);
    MutateReject(mapped, 0, 2); // DOS signature
    MutateReject(mapped, pe, 4); // PE signature
    MutateReject(mapped, pe + 4, 2); // architecture
    MutateReject(mapped, pe + 8, 4); // timestamp
    MutateReject(mapped, pe + 24, 2); // PE32 optional header
    MutateReject(mapped, pe + 24 + 56, 4); // declared image size
    MutateReject(mapped, ce::SubmitRva, 25); // audited 16-argument adapter
    for (const auto call : ce::CallRvas)
    {
        MutateReject(mapped, call - 9, 9); // source flags / final argument pushes
        MutateReject(mapped, call, 5); // opcode and each rel32 target byte
        MutateReject(mapped, call + 5, 3); // caller's 64-byte stack cleanup
    }
    MutateReject(mapped, 0x769F26, 6); // frame comparison and absolute operand
    MutateReject(mapped, 0x4D0F82, 6); // frame increment and absolute operand
    CHECK(!ce::ValidateMappedImage(nullptr, mapped.size(), ce::PreferredBase));
    CHECK(!ce::ValidateMappedImage(mapped.data(), mapped.size() - 1, ce::PreferredBase));
    CHECK(!ce::ValidateMappedImage(mapped.data(), 0, ce::PreferredBase));
    CHECK(!ce::ValidateMappedImage(mapped.data(), mapped.size() + 1, ce::PreferredBase));

    Write32(mapped, 0x3C, ce::ImageSize - 4);
    CHECK(!Valid(mapped));
    Write32(mapped, 0x3C, std::numeric_limits<std::uint32_t>::max());
    CHECK(!Valid(mapped));
    Write32(mapped, 0x3C, pe);

    constexpr std::uint32_t relocatedBase = 0x50000000;
    CHECK(!Valid(mapped, relocatedBase));
    Write32(mapped, 0x769F28, relocatedBase + ce::FrameCounterRva);
    CHECK(!Valid(mapped, relocatedBase)); // both operands must relocate
    Write32(mapped, 0x4D0F84, relocatedBase + ce::FrameCounterRva);
    CHECK(Valid(mapped, relocatedBase));
    CHECK(!Valid(mapped, ce::PreferredBase));
    MutateReject(mapped, 0x769F28, 4, relocatedBase);
    MutateReject(mapped, 0x4D0F84, 4, relocatedBase);

    // An image cannot occupy addresses past the 32-bit address-space boundary.
    constexpr std::uint32_t impossibleBase = 0xFFF00000;
    Write32(mapped, 0x769F28, impossibleBase + ce::FrameCounterRva);
    Write32(mapped, 0x4D0F84, impossibleBase + ce::FrameCounterRva);
    CHECK(!Valid(mapped, impossibleBase));
    std::cout << "PASS: " << checks << " mapped PE guard checks; executable read from disk only.\n";
    return 0;
}
