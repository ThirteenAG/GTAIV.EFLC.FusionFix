#include "../../source/ShadowAllocationCE.hpp"

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <vector>

namespace ce = fusionfix::shadows::ce;
namespace allocation = ce::allocation;
static std::size_t checks{};
static std::size_t mutations{};
#define CHECK(expression) do { ++checks; if (!(expression)) { std::cerr << __LINE__ << ": " #expression "\n"; std::exit(1); } } while (false)

static std::uint16_t Read16(const std::uint8_t* p)
{
    return static_cast<std::uint16_t>(p[0] | (static_cast<std::uint16_t>(p[1]) << 8));
}

static void Write32(std::vector<std::uint8_t>& data, std::size_t offset, std::uint32_t value)
{
    CHECK(offset <= data.size() && 4 <= data.size() - offset);
    std::memcpy(data.data() + offset, &value, sizeof(value));
}

struct DiskImage
{
    std::vector<std::uint8_t> mapped;
    std::vector<std::uint32_t> highLowRvas;
};

static DiskImage ReadAsData(const char* path)
{
    // No LoadLibrary/CreateProcess/OpenProcess. Only binary file input and an
    // ordinary byte vector; no copied instruction is ever executed.
    std::ifstream input(path, std::ios::binary);
    CHECK(input.good());
    const std::vector<std::uint8_t> disk{std::istreambuf_iterator<char>(input), {}};
    CHECK(disk.size() >= 0x100);
    const auto pe = ce::Read32(disk.data() + 0x3C);
    CHECK(pe <= disk.size() - 0x100);
    CHECK(ce::Read32(disk.data() + pe) == 0x4550);
    const auto optionalSize = Read16(disk.data() + pe + 20);
    const auto sectionCount = Read16(disk.data() + pe + 6);
    const std::size_t optional = static_cast<std::size_t>(pe) + 24;
    CHECK(optional <= disk.size() && optionalSize <= disk.size() - optional);
    CHECK(optionalSize >= 144);
    CHECK(ce::Read32(disk.data() + optional + 56) == ce::ImageSize);
    const auto headersSize = ce::Read32(disk.data() + optional + 60);
    CHECK(headersSize <= disk.size() && headersSize <= ce::ImageSize);
    DiskImage result;
    result.mapped.resize(ce::ImageSize);
    std::copy_n(disk.begin(), headersSize, result.mapped.begin());
    const auto sectionTable = optional + optionalSize;
    CHECK(sectionTable <= disk.size() &&
        static_cast<std::size_t>(sectionCount) * 40 <= disk.size() - sectionTable);
    for (std::size_t i = 0; i < sectionCount; ++i)
    {
        const auto* section = disk.data() + sectionTable + i * 40;
        const auto va = ce::Read32(section + 12);
        const auto rawSize = ce::Read32(section + 16);
        const auto rawOffset = ce::Read32(section + 20);
        CHECK(rawOffset <= disk.size() && rawSize <= disk.size() - rawOffset);
        CHECK(va <= result.mapped.size());
        // CE has raw padding past its non-page-aligned declared SizeOfImage.
        const auto copySize = (std::min)(static_cast<std::size_t>(rawSize), result.mapped.size() - va);
        std::copy_n(disk.begin() + rawOffset, copySize, result.mapped.begin() + va);
    }

    // Relocate using the PE's actual directory, independently of the header's
    // guard metadata. This detects missing, surplus, or mislocated operands.
    const auto relocationRva = ce::Read32(disk.data() + optional + 136);
    const auto relocationSize = ce::Read32(disk.data() + optional + 140);
    CHECK(relocationRva <= result.mapped.size() &&
        relocationSize <= result.mapped.size() - relocationRva);
    std::size_t cursor = 0;
    while (cursor < relocationSize)
    {
        CHECK(8 <= relocationSize - cursor);
        const auto* block = result.mapped.data() + relocationRva + cursor;
        const auto page = ce::Read32(block);
        const auto blockSize = ce::Read32(block + 4);
        CHECK(blockSize >= 8 && blockSize % 2 == 0 && blockSize <= relocationSize - cursor);
        for (std::size_t offset = 8; offset < blockSize; offset += 2)
        {
            const auto entry = Read16(block + offset);
            const auto type = entry >> 12;
            CHECK(type == 0 || type == 3);
            if (!type)
                continue;
            const auto withinPage = static_cast<std::uint32_t>(entry & 0xFFF);
            CHECK(page <= ce::ImageSize && withinPage <= ce::ImageSize - page);
            const auto rva = page + withinPage;
            CHECK(rva <= ce::ImageSize && 4 <= ce::ImageSize - rva);
            result.highLowRvas.push_back(rva);
        }
        cursor += blockSize;
    }
    std::sort(result.highLowRvas.begin(), result.highLowRvas.end());
    CHECK(std::adjacent_find(result.highLowRvas.begin(), result.highLowRvas.end()) == result.highLowRvas.end());
    CHECK(!result.highLowRvas.empty());
    return result;
}

static bool Valid(const std::vector<std::uint8_t>& mapped, std::uintptr_t base = ce::PreferredBase)
{
    return allocation::ValidateMappedImage(mapped.data(), mapped.size(), base);
}

static void MutateReject(std::vector<std::uint8_t>& mapped, std::size_t offset, std::size_t count,
                         std::uintptr_t base = ce::PreferredBase, unsigned bits = 8)
{
    CHECK(offset <= mapped.size() && count <= mapped.size() - offset);
    for (std::size_t i = 0; i < count; ++i)
    {
        for (unsigned bit = 0; bit < bits; ++bit)
        {
            const auto original = mapped[offset + i];
            mapped[offset + i] ^= static_cast<std::uint8_t>(1u << bit);
            ++mutations;
            CHECK(!Valid(mapped, base));
            mapped[offset + i] = original;
            CHECK(Valid(mapped, base));
        }
    }
}

static void CheckGuardMetadata(const DiskImage& disk)
{
    std::size_t guardedBytes = 0;
    std::size_t guardedRelocations = 0;
    for (const auto& guard : allocation::detail::GuardedRanges)
    {
        CHECK(allocation::detail::ValidMetadata(guard));
        guardedBytes += guard.size;
        guardedRelocations += guard.relocationCount;
        std::vector<std::uint32_t> expected;
        for (std::size_t i = 0; i < guard.relocationCount; ++i)
            expected.push_back(guard.rva + guard.relocationOffsets[i]);
        const auto begin = std::lower_bound(disk.highLowRvas.begin(), disk.highLowRvas.end(), guard.rva);
        const auto end = std::lower_bound(disk.highLowRvas.begin(), disk.highLowRvas.end(),
            static_cast<std::uint32_t>(guard.rva + guard.size));
        CHECK(expected == std::vector<std::uint32_t>(begin, end));
    }
    CHECK(guardedBytes == 2585);
    CHECK(guardedRelocations == 103);
}

static void CheckExactContract(const std::vector<std::uint8_t>& mapped, std::uint32_t base)
{
    static_assert(allocation::SelectionRva == 0x5277F0);
    static_assert(allocation::SelectionCallerRva == 0x1C2ADB);
    static_assert(allocation::CollectRva == 0x527D5C);
    static_assert(allocation::FinalizeRva == 0x52805D);
    static_assert(allocation::ComparatorRva == 0x5253F0);
    static_assert(allocation::LightArrayPointerRva == 0xC3EED8);
    static_assert(allocation::LightCountRva == 0x110E240);
    static_assert(allocation::DynamicSlotCount == 7);
    static_assert(allocation::LightSourceStride == 0x80);
    static_assert(allocation::DynamicPassStride == 0x110);
    static_assert(allocation::CollectionIndexStackOffset == 0x14);
    static_assert(allocation::CollectionByteOffsetStackOffset == 0x18);
    static_assert(allocation::CollectionFlagsStackOffset == 0x1C);
    static_assert(allocation::CollectionDistanceStackOffset == 0x20);
    static_assert(allocation::SelectedIndicesStackOffset == 0x7C);

    const std::uint8_t collect[]{0x8B,0x7C,0x24,0x1C,0x89,0x54,0x24,0x24,0x33,0xF6};
    const std::uint8_t finalize[]{0x8D,0x7C,0x24,0x7C,0x89,0x7C,0x24,0x18,0xBE};
    CHECK(ce::Match(mapped.data(), mapped.size(), allocation::CollectRva, collect, sizeof(collect)));
    CHECK(ce::Match(mapped.data(), mapped.size(), allocation::FinalizeRva, finalize, sizeof(finalize)));
    CHECK(mapped[allocation::SelectionCallerRva] == 0xE8);
    CHECK(static_cast<std::uint32_t>(allocation::SelectionCallerRva + 5 +
        ce::Read32(mapped.data() + allocation::SelectionCallerRva + 1)) == allocation::SelectionRva);
    CHECK(mapped[0x527D73] == 0xE8);
    CHECK(static_cast<std::uint32_t>(0x527D78 + ce::Read32(mapped.data() + 0x527D74)) == allocation::ComparatorRva);
    CHECK(ce::Read32(mapped.data() + 0x528066) == base + allocation::DynamicPassFirstPositionRva);
    for (const auto operand : {0x527BAB, 0x527C11, 0x527DD4, 0x527E34, 0x527F34, 0x52807E})
        CHECK(ce::Read32(mapped.data() + operand) == base + allocation::LightArrayPointerRva);
    for (const auto operand : {0x527BA0, 0x527DEE})
        CHECK(ce::Read32(mapped.data() + operand) == base + allocation::LightCountRva);
    CHECK(ce::Read32(mapped.data() + 0x5280BB) == base + allocation::DynamicPassFirstPositionRva +
        allocation::DynamicSlotCount * allocation::DynamicPassStride);
}

static void CheckLayoutSnapshot()
{
    // Fill a raw record at literal, independently audited byte offsets and
    // verify a memcpy snapshot. No pointer to a live light is involved.
    std::array<std::uint8_t, 0x80> raw{};
    const float direction[3]{0.25f, -0.5f, 0.75f};
    const float position[3]{123.5f, -45.0f, 8.0f};
    const float radius = 20.0f;
    const float cone = 0.8f;
    const std::uint32_t type = 2, flags = 0x504, key = 0x12345679;
    const std::int32_t cache = -1, interior = 14, room = 9;
    std::memcpy(raw.data(), direction, sizeof(direction));
    std::memcpy(raw.data() + 0x20, position, sizeof(position));
    std::memcpy(raw.data() + 0x44, &type, 4);
    std::memcpy(raw.data() + 0x48, &flags, 4);
    std::memcpy(raw.data() + 0x54, &radius, 4);
    std::memcpy(raw.data() + 0x5C, &cone, 4);
    std::memcpy(raw.data() + 0x60, &key, 4);
    std::memcpy(raw.data() + 0x64, &cache, 4);
    std::memcpy(raw.data() + 0x68, &interior, 4);
    std::memcpy(raw.data() + 0x6C, &room, 4);
    allocation::LightSource32 light{};
    std::memcpy(&light, raw.data(), raw.size());
    CHECK(light.direction[0] == 0.25f && light.direction[1] == -0.5f && light.direction[2] == 0.75f);
    CHECK(light.position[0] == 123.5f && light.position[1] == -45.0f && light.position[2] == 8.0f);
    CHECK(light.type == 2 && light.flags == flags && light.key == key);
    CHECK(light.radius == radius && light.outerCone == cone);
    CHECK(light.staticCacheIndex == -1 && light.interior == 14 && light.room == 9);
}

static void MalformedBoundsTests(std::vector<std::uint8_t>& mapped)
{
    std::array<std::uint8_t, 64> shortBacking{};
    CHECK(!allocation::ValidateMappedImage(nullptr, ce::ImageSize, ce::PreferredBase));
    for (const auto length : {std::size_t(0), std::size_t(1), std::size_t(63), std::size_t(64),
        static_cast<std::size_t>(ce::ImageSize - 1), static_cast<std::size_t>(ce::ImageSize + 1),
        std::numeric_limits<std::size_t>::max()})
        CHECK(!allocation::ValidateMappedImage(shortBacking.data(), length, ce::PreferredBase));

    const auto pe = ce::Read32(mapped.data() + 0x3C);
    for (const auto offset : {0u, 0x40u, ce::ImageSize - 0x7Fu, ce::ImageSize - 4u, UINT32_MAX})
    {
        Write32(mapped, 0x3C, offset);
        CHECK(!Valid(mapped));
    }
    Write32(mapped, 0x3C, pe);
    CHECK(Valid(mapped));
    CHECK(!Valid(mapped, UINT32_MAX - (ce::ImageSize - 1) + 1));
    CHECK(!Valid(mapped, std::numeric_limits<std::uintptr_t>::max()));

    for (const auto& original : allocation::detail::GuardedRanges)
    {
        auto shifted = original;
        ++shifted.rva;
        CHECK(!allocation::detail::MatchRelocatedRange(mapped.data(), mapped.size(), ce::PreferredBase, shifted));
        shifted.rva = original.rva - 1;
        CHECK(!allocation::detail::MatchRelocatedRange(mapped.data(), mapped.size(), ce::PreferredBase, shifted));
        auto outside = original;
        outside.rva = UINT32_MAX;
        CHECK(!allocation::detail::MatchRelocatedRange(mapped.data(), mapped.size(), ce::PreferredBase, outside));
        outside = original;
        outside.size = std::numeric_limits<std::size_t>::max();
        CHECK(!allocation::detail::MatchRelocatedRange(mapped.data(), mapped.size(), ce::PreferredBase, outside));
        outside = original;
        outside.size = 0;
        CHECK(!allocation::detail::MatchRelocatedRange(mapped.data(), mapped.size(), ce::PreferredBase, outside));
        outside = original;
        outside.bytes = nullptr;
        CHECK(!allocation::detail::MatchRelocatedRange(mapped.data(), mapped.size(), ce::PreferredBase, outside));
        CHECK(!allocation::detail::MatchRelocatedRange(nullptr, mapped.size(), ce::PreferredBase, original));
        CHECK(!allocation::detail::MatchRelocatedRange(shortBacking.data(), shortBacking.size(), ce::PreferredBase, original));
        CHECK(!allocation::detail::MatchRelocatedRange(mapped.data(), original.rva + original.size - 1,
            ce::PreferredBase, original));
    }

    auto malformed = allocation::detail::GuardedRanges[0];
    auto offsets = allocation::detail::SelectionRelocations;
    malformed.relocationOffsets = nullptr;
    CHECK(!allocation::detail::MatchRelocatedRange(mapped.data(), mapped.size(), ce::PreferredBase, malformed));
    malformed.relocationOffsets = offsets.data();
    offsets[0] = static_cast<std::uint16_t>(malformed.size - 3);
    CHECK(!allocation::detail::MatchRelocatedRange(mapped.data(), mapped.size(), ce::PreferredBase, malformed));
    offsets = allocation::detail::SelectionRelocations;
    offsets[1] = offsets[0];
    CHECK(!allocation::detail::MatchRelocatedRange(mapped.data(), mapped.size(), ce::PreferredBase, malformed));
    offsets = allocation::detail::SelectionRelocations;
    std::swap(offsets[0], offsets[1]);
    CHECK(!allocation::detail::MatchRelocatedRange(mapped.data(), mapped.size(), ce::PreferredBase, malformed));
    offsets = allocation::detail::SelectionRelocations;
    ++offsets[0];
    CHECK(!allocation::detail::MatchRelocatedRange(mapped.data(), mapped.size(), ce::PreferredBase, malformed));

    // No hidden alignment requirement in the raw-image validation functions.
    std::vector<std::uint8_t> unaligned(mapped.size() + 1);
    std::copy(mapped.begin(), mapped.end(), unaligned.begin() + 1);
    CHECK(allocation::ValidateMappedImage(unaligned.data() + 1, mapped.size(), ce::PreferredBase));
}

static void RelocationTests(const DiskImage& disk)
{
    for (const std::uint32_t base : {0x00100000u, 0x50000000u, UINT32_MAX - (ce::ImageSize - 1)})
    {
        auto relocated = disk.mapped;
        CHECK(!Valid(relocated, base));
        const auto delta = static_cast<std::uint32_t>(base - ce::PreferredBase);
        for (const auto rva : disk.highLowRvas)
            Write32(relocated, rva, ce::Read32(relocated.data() + rva) + delta);
        CHECK(Valid(relocated, base));
        CHECK(!Valid(relocated, ce::PreferredBase));
        CheckExactContract(relocated, base);
        for (const auto& guard : allocation::detail::GuardedRanges)
        {
            for (std::size_t i = 0; i < guard.relocationCount; ++i)
            {
                const auto rva = guard.rva + guard.relocationOffsets[i];
                const auto correct = ce::Read32(relocated.data() + rva);
                Write32(relocated, rva, ce::Read32(disk.mapped.data() + rva));
                CHECK(!Valid(relocated, base)); // one unrelocated address cannot slip through
                Write32(relocated, rva, correct);
                CHECK(Valid(relocated, base));
            }
            // At the ordinary ASLR base, test every bit of every guarded byte
            // again, including all four bytes of every relocated operand.
            if (base == 0x50000000u)
                MutateReject(relocated, guard.rva, guard.size, base);
        }
    }
}

int main(int argc, char** argv)
{
    CHECK(argc == 2);
    auto disk = ReadAsData(argv[1]);
    CHECK(ce::ValidateMappedImage(disk.mapped.data(), disk.mapped.size(), ce::PreferredBase));
    CHECK(Valid(disk.mapped));
    CheckGuardMetadata(disk);
    CheckExactContract(disk.mapped, ce::PreferredBase);
    CheckLayoutSnapshot();

    for (const auto& guard : allocation::detail::GuardedRanges)
        MutateReject(disk.mapped, guard.rva, guard.size);
    // Additional header fields and the entire existing CE adapter guard remain
    // prerequisites; a valid new selection body alone must never pass.
    MutateReject(disk.mapped, 0x3C, 4);
    MutateReject(disk.mapped, 0x148 + 6, 2);
    MutateReject(disk.mapped, 0x148 + 20, 2);
    MutateReject(disk.mapped, 0x148 + 24 + 28, 4);
    for (const auto range : {std::array<std::size_t, 2>{0,2}, {0x148,4}, {0x148+4,2},
        {0x148+8,4}, {0x148+24,2}, {0x148+24+56,4}, {ce::SubmitRva,25}, {0x769F26,6}, {0x4D0F82,6}})
        MutateReject(disk.mapped, range[0], range[1]);
    for (const auto call : ce::CallRvas)
        MutateReject(disk.mapped, call - 9, 17);

    MalformedBoundsTests(disk.mapped);
    RelocationTests(disk);
    std::cout << "PASS: " << checks << " checks, " << mutations
        << " rejected one-bit mutations; 2,585 allocation-guard bytes, 103 exact relocated operands.\n"
        << "Executable was read from disk only; no game code executed or process accessed.\n";
}
