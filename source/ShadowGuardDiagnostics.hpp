#pragma once
#include "ShadowCasterCE.hpp"
#include <sstream>

// Startup-only diagnostics. Never relax a guard or read outside the mapped image.
namespace fusionfix::shadows::ce::diagnostics
{
    inline std::string Describe(const std::uint8_t* image, std::size_t size,
                                std::uintptr_t base)
    {
        std::ostringstream out;
        out << std::hex;
        if (!image || size != ImageSize || base > UINT32_MAX - (ImageSize - 1))
            return "invalid_image_span";
        out << "base=" << base << " base_adapter=" << ce::ValidateMappedImage(image,size,base)
            << " allocation_guard=" << allocation::ValidateMappedImage(image,size,base)
            << " caster_guard=" << casterguard::Validate(image,size,base);
        const auto pe = Read32(image+0x3C);
        out << " pe_offset=" << pe;
        if (pe <= size-0x80)
            out << " sections=" << (unsigned(image[pe+6]) | unsigned(image[pe+7])<<8)
                << " optional_size=" << (unsigned(image[pe+20]) | unsigned(image[pe+21])<<8)
                << " header_base=" << Read32(image+pe+24+28);
        auto describeRange = [&](const allocation::detail::GuardedRange& range)
        {
            if (!allocation::detail::ValidMetadata(range) || range.rva > size || range.size > size-range.rva)
            { out << " invalid_range=" << range.rva; return; }
            unsigned total = 0;
            std::size_t relocation = 0;
            for (std::size_t i = 0; i < range.size; ++i)
            {
                while (relocation < range.relocationCount && i >= std::size_t(range.relocationOffsets[relocation])+4)
                    ++relocation;
                auto expected = range.bytes[i];
                if (relocation < range.relocationCount && i >= range.relocationOffsets[relocation])
                {
                    const auto offset = range.relocationOffsets[relocation];
                    const auto relocated = static_cast<std::uint32_t>(base + Read32(range.bytes+offset)-PreferredBase);
                    expected = static_cast<std::uint8_t>(relocated >> (8*(i-offset)));
                }
                const auto actual = image[range.rva+i];
                if (actual != expected)
                {
                    if (total < 12) out << " mismatch=" << (range.rva+i) << ':' << unsigned(expected) << '>' << unsigned(actual);
                    ++total;
                }
            }
            if (total) out << " range=" << range.rva << " mismatches=" << total;
        };
        for (const auto& range : allocation::detail::GuardedRanges) describeRange(range);
        for (const auto& range : casterguard::Ranges) describeRange(range);
        return out.str();
    }
}
