module;

#include <common.hxx>

export module addonfonts;

import common;

namespace AddonFonts
{
    static constexpr size_t Capacity = 256;
    static constexpr size_t FirstCustom = 9;
    static constexpr size_t BatchBytes = 170 * 36;

    struct Descriptor
    {
        uint8_t metrics[576];
        void* texture;
        float width, height, rowHeight, top, bottom;
    };
    static_assert(sizeof(Descriptor) == 600);

    enum class Storage { Descriptors, Maps, Counts, Batches, End };
    enum class Role { Other, Init, Reload, Values, Load, Shutdown, Enqueue };
    struct Reference { uint32_t operand; Storage storage; uint32_t offset; };
    struct Function
    {
        Role role;
        const char* pattern;
        std::vector<Reference> references;
        uintptr_t address = 0;
    };

    // Operand offsets are relative to individually matched functions. CE and the
    // legacy executables arrange these arrays differently; no image-wide scan is used.
    static std::vector<Function> ceFunctions =
    {
        { Role::Enqueue, "A0 ? ? ? ? 56 0F B6 F0 3C 03 72 05 BE 03 00 00 00 8B 44 24 08 8B 14 B5 ? ? ? ? F3 0F 7E 10", {
            { 0x19, Storage::Counts, 0x0 }, { 0x4B, Storage::Batches, 0x0 }, { 0x54, Storage::Batches, 0x8 }, { 0x5D, Storage::Batches, 0x10 }, { 0x66, Storage::Batches, 0x18 }, { 0x6D, Storage::Batches, 0x20 }, { 0x77, Storage::Counts, 0x0 }
        } },
        { Role::Other, "8B 54 24 04 83 C2 20 66 83 FA 20 75 1E 80 3D ? ? ? ? 6A 74 0F 80 3D ? ? ? ? 00 75 06 B8 FD 00 00 00", {
            { 0x43, Storage::Descriptors, 0x220 }, { 0x49, Storage::Descriptors, 0x224 }, { 0x51, Storage::Descriptors, 0x230 }, { 0x57, Storage::Descriptors, 0x234 }, { 0x5F, Storage::Descriptors, 0x228 }, { 0x65, Storage::Descriptors, 0x22C }, { 0x74, Storage::Descriptors, 0xFF }, { 0x84, Storage::Descriptors, 0x238 }, { 0x8B, Storage::Descriptors, 0x23C }, { 0x97, Storage::Descriptors, 0xFF }, { 0xD4, Storage::Descriptors, 0xFF }, { 0x124, Storage::Descriptors, 0xFF }, { 0x136, Storage::Descriptors, 0xFF }
        } },
        { Role::Other, "51 80 3D ? ? ? ? 00 0F 84 ? ? ? ? 8B 54 24 08 53 8A 1D ? ? ? ? 80 FB 02 75 59 0F B7 C2", {
            { 0x23, Storage::Descriptors, 0x6D8 }, { 0x2B, Storage::Descriptors, 0x6DC }, { 0x35, Storage::Descriptors, 0x6C8 }, { 0x3D, Storage::Descriptors, 0x6F4 }, { 0x4E, Storage::Descriptors, 0x6E0 }, { 0x56, Storage::Descriptors, 0x6E4 }, { 0x60, Storage::Descriptors, 0x6CC }, { 0x68, Storage::Descriptors, 0x6F4 }, { 0xE0, Storage::Maps, 0x0 }, { 0x10D, Storage::Descriptors, 0x0 }, { 0x11D, Storage::Descriptors, 0x244 }, { 0x13C, Storage::Descriptors, 0x200 }, { 0x144, Storage::Descriptors, 0x244 }
        } },
        { Role::Other, "51 55 56 E8 ? ? ? ? 8D 14 C0 8A 0C D5 ? ? ? ? 0F B6 2C D5 ? ? ? ? 0F B6 F1 8B C6 69 C0 96 00 00 00", {
            { 0x34, Storage::Descriptors, 0x204 }, { 0x59, Storage::Descriptors, 0x228 }, { 0x61, Storage::Descriptors, 0x22C }, { 0x6B, Storage::Descriptors, 0x218 }, { 0x98, Storage::Descriptors, 0x230 }, { 0xA0, Storage::Descriptors, 0x234 }, { 0xAA, Storage::Descriptors, 0x21C }, { 0x142, Storage::Maps, 0x0 }, { 0x16D, Storage::Descriptors, 0x0 }, { 0x1AD, Storage::Descriptors, 0x200 }
        } },
        { Role::Other, "55 8B EC 83 E4 F8 B8 B4 10 00 00 E8 ? ? ? ? A1 ? ? ? ? 33 C4 89 84 24 B0 10 00 00 53 56", {
            { 0x1F4, Storage::Descriptors, 0x564 }, { 0x388, Storage::Descriptors, 0x564 }
        } },
        { Role::Other, "8A 0D ? ? ? ? 0F B6 D1 8B C2 69 C0 96 00 00 00 57 0F B6 3D ? ? ? ? 03 C7 80 7C 24 0C 00", {
            { 0x25, Storage::Descriptors, 0x204 }, { 0x4A, Storage::Descriptors, 0x6D8 }, { 0x52, Storage::Descriptors, 0x6DC }, { 0x5C, Storage::Descriptors, 0x6C8 }, { 0x87, Storage::Descriptors, 0x6E0 }, { 0x8F, Storage::Descriptors, 0x6E4 }, { 0x99, Storage::Descriptors, 0x6CC }, { 0x124, Storage::Maps, 0x0 }, { 0x14A, Storage::Descriptors, 0x0 }, { 0x18A, Storage::Descriptors, 0x200 }
        } },
        { Role::Init, "81 EC 8C 00 00 00 C7 05 ? ? ? ? FF FF FF FF 53 55 56 57 C6 05 ? ? ? ? 00 E8 ? ? ? ?", {
            { 0x290, Storage::Descriptors, 0x240 }, { 0x296, Storage::Descriptors, 0x244 }, { 0x2A0, Storage::Descriptors, 0x248 }, { 0x2AA, Storage::Descriptors, 0x24C }, { 0x2B4, Storage::Descriptors, 0x250 }, { 0x2BE, Storage::Descriptors, 0x254 }, { 0x2DE, Storage::Descriptors, 0x498 }, { 0x2E4, Storage::Descriptors, 0x49C }, { 0x2EE, Storage::Descriptors, 0x4A0 }, { 0x2F8, Storage::Descriptors, 0x4A4 }, { 0x302, Storage::Descriptors, 0x4A8 }, { 0x30C, Storage::Descriptors, 0x4AC }, { 0x33D, Storage::Descriptors, 0x6F0 }, { 0x343, Storage::Descriptors, 0x6F4 }, { 0x34D, Storage::Descriptors, 0x6F8 }, { 0x357, Storage::Descriptors, 0x6FC }, { 0x361, Storage::Descriptors, 0x700 }, { 0x36B, Storage::Descriptors, 0x704 }, { 0x691, Storage::Batches, 0x0 }, { 0x6A7, Storage::Counts, 0x0 }, { 0x6AF, Storage::Counts, 0x8 }
        } },
        { Role::Load, "81 EC 8C 00 00 00 55 8B AC 24 94 00 00 00 57 8B 3D ? ? ? ? 3B 2D ? ? ? ? 75 25 80 3D ? ? ? ? 01", {
            { 0x20B, Storage::Descriptors, 0x708 }
        } },
        { Role::Values, "81 EC 3C 01 00 00 A1 ? ? ? ? 33 C4 89 84 24 38 01 00 00 A0 ? ? ? ? 53 55 33 DB 33 ED 56", {
            { 0x429, Storage::Descriptors, 0x95C }, { 0x5F2, Storage::Descriptors, 0x204 }, { 0x5FC, Storage::Descriptors, 0x208 }, { 0x606, Storage::Descriptors, 0x20C }, { 0x6BE, Storage::Descriptors, 0xFD }, { 0x6D5, Storage::Descriptors, 0xD0 }, { 0x6E9, Storage::Maps, 0x0 }, { 0x721, Storage::Descriptors, 0x6F4 }, { 0x72B, Storage::Descriptors, 0x6F8 }, { 0x7B5, Storage::Descriptors, 0x220 }, { 0x7C3, Storage::Descriptors, 0x224 }, { 0x838, Storage::Descriptors, 0x228 }, { 0x845, Storage::Descriptors, 0x22C }, { 0x8C0, Storage::Descriptors, 0x230 }, { 0x8CD, Storage::Descriptors, 0x234 }, { 0x942, Storage::Descriptors, 0x238 }, { 0x94F, Storage::Descriptors, 0x23C }, { 0x9D3, Storage::Descriptors, 0x204 }, { 0x9E7, Storage::Descriptors, 0x208 }, { 0x9FB, Storage::Descriptors, 0x20C }, { 0xB25, Storage::Descriptors, 0xFF }, { 0xC85, Storage::Descriptors, 0x0 }, { 0xD32, Storage::Descriptors, 0x200 }, { 0xDA9, Storage::Descriptors, 0x218 }, { 0xE22, Storage::Descriptors, 0x21C }
        } },
        { Role::Other, "83 EC 30 83 3D ? ? ? ? FF 75 0D 80 3D ? ? ? ? 03 0F 84 ? ? ? ? F3 0F 10 05 ? ? ? ?", {
            { 0x3E2, Storage::Descriptors, 0x240 }, { 0x458, Storage::Descriptors, 0x6D4 }, { 0x479, Storage::Maps, 0x0 }, { 0x5C5, Storage::Descriptors, 0x248 }, { 0x5CD, Storage::Descriptors, 0x24C }, { 0x5D5, Storage::Descriptors, 0x254 }, { 0x5DD, Storage::Descriptors, 0x250 }, { 0x639, Storage::Descriptors, 0x240 }
        } },
        { Role::Other, "83 EC 30 53 55 56 57 8B 7C 24 44 57 E8 ? ? ? ? 8B 5C 24 4C 8B 35 ? ? ? ? F3 0F 10 03 83 C4 04", {
            { 0x572, Storage::Descriptors, 0x240 }
        } },
        { Role::Reload, "81 EC 94 00 00 00 8D 4C 24 04 68 ? ? ? ? E8 ? ? ? ? 6A 00 B9 ? ? ? ? C6 05 ? ? ? ? 01", {
            { 0x62, Storage::Descriptors, 0x240 }, { 0x6C, Storage::Descriptors, 0x498 }, { 0x76, Storage::Descriptors, 0x6F0 }, { 0x303, Storage::Descriptors, 0x240 }, { 0x309, Storage::Descriptors, 0x244 }, { 0x313, Storage::Descriptors, 0x248 }, { 0x31D, Storage::Descriptors, 0x24C }, { 0x327, Storage::Descriptors, 0x250 }, { 0x331, Storage::Descriptors, 0x254 }, { 0x351, Storage::Descriptors, 0x498 }, { 0x357, Storage::Descriptors, 0x49C }, { 0x361, Storage::Descriptors, 0x4A0 }, { 0x36B, Storage::Descriptors, 0x4A4 }, { 0x375, Storage::Descriptors, 0x4A8 }, { 0x37F, Storage::Descriptors, 0x4AC }, { 0x3B0, Storage::Descriptors, 0x6F0 }, { 0x3B6, Storage::Descriptors, 0x6F4 }, { 0x3C0, Storage::Descriptors, 0x6F8 }, { 0x3CA, Storage::Descriptors, 0x6FC }, { 0x3D4, Storage::Descriptors, 0x700 }, { 0x3DE, Storage::Descriptors, 0x704 }, { 0x41E, Storage::Descriptors, 0x948 }, { 0x555, Storage::Descriptors, 0x708 }
        } },
        { Role::Other, "56 8B 74 24 08 57 85 F6 78 57 83 3C B5 ? ? ? ? 00 8D 3C B5 ? ? ? ? 0F 86 ? ? ? ? 8B C6", {
            { 0xD, Storage::Counts, 0x0 }, { 0x15, Storage::Counts, 0x0 }, { 0x2B, Storage::Descriptors, 0x240 }, { 0x3E, Storage::Batches, 0x0 }, { 0x63, Storage::Counts, 0x0 }, { 0x68, Storage::Batches, 0x0 }, { 0x6D, Storage::Descriptors, 0x240 }, { 0xAE, Storage::End, 0x0 }
        } },
        { Role::Other, "55 8B EC 83 E4 F8 81 EC C4 00 00 00 A1 ? ? ? ? 33 C4 89 84 24 C0 00 00 00 80 3D ? ? ? ? 00", {
            { 0x230, Storage::Descriptors, 0x240 }, { 0x6AA, Storage::Descriptors, 0x240 }
        } },
        { Role::Shutdown, "6A FF E8 ? ? ? ? 68 ? ? ? ? E8 ? ? ? ? 50 E8 ? ? ? ? 83 C4 0C 83 3D ? ? ? ? FF", {
            { 0x52, Storage::Batches, 0x0 }
        } },
    };

    static std::vector<Function> legacyFunctions =
    {
        { Role::Other, "51 80 3D ? ? ? ? 00 0F 84 ? ? ? ? 80 3D ? ? ? ? 02 8B 4C 24 08 75 3F 0F B7 C1 3B 05 ? ? ? ?", {
            { 0x20, Storage::Descriptors, 0x6D8 }, { 0x28, Storage::Descriptors, 0x6DC }, { 0x30, Storage::Descriptors, 0x6C8 }, { 0x36, Storage::Descriptors, 0x6F4 }, { 0x3E, Storage::Descriptors, 0x6E0 }, { 0x46, Storage::Descriptors, 0x6E4 }, { 0x4E, Storage::Descriptors, 0x6CC }, { 0x54, Storage::Descriptors, 0x6F4 }, { 0xB8, Storage::Maps, 0x0 }, { 0xE6, Storage::Descriptors, 0x0 }, { 0xF4, Storage::Descriptors, 0x244 }, { 0x109, Storage::Descriptors, 0x200 }, { 0x10F, Storage::Descriptors, 0x244 }
        } },
        { Role::Other, "51 A0 ? ? ? ? 0F B6 15 ? ? ? ? 0F B6 C8 56 8B F1 69 F6 96 00 00 00 03 F2 80 7C 24 10 00", {
            { 0x25, Storage::Descriptors, 0x204 }, { 0x54, Storage::Descriptors, 0x6D8 }, { 0x5C, Storage::Descriptors, 0x6DC }, { 0x64, Storage::Descriptors, 0x6C8 }, { 0x6F, Storage::Descriptors, 0x6E0 }, { 0x77, Storage::Descriptors, 0x6E4 }, { 0x7F, Storage::Descriptors, 0x6CC }, { 0xD6, Storage::Maps, 0x0 }, { 0x101, Storage::Descriptors, 0x0 }, { 0x117, Storage::Descriptors, 0x200 }
        } },
        { Role::Other, "56 8B 74 24 08 85 F6 57 7C 5E 83 3C B5 ? ? ? ? 00 0F 86 ? ? ? ? 8B C6 69 C0 58 02 00 00", {
            { 0xD, Storage::Counts, 0x0 }, { 0x22, Storage::Descriptors, 0x240 }, { 0x31, Storage::Counts, 0x0 }, { 0x3F, Storage::Batches, 0x0 }, { 0x5E, Storage::Counts, 0x0 }, { 0x6A, Storage::Counts, 0x0 }, { 0x6F, Storage::Batches, 0x0 }, { 0x74, Storage::Descriptors, 0x240 }, { 0xB7, Storage::End, 0x0 }
        } },
        { Role::Other, "8B 54 24 04 83 C2 20 66 83 FA 20 75 1C 80 3D ? ? ? ? 6A 74 0E 80 3D ? ? ? ? 00 75 05 66 B8 FD 00", {
            { 0x45, Storage::Descriptors, 0x220 }, { 0x4B, Storage::Descriptors, 0x224 }, { 0x53, Storage::Descriptors, 0x230 }, { 0x59, Storage::Descriptors, 0x234 }, { 0x61, Storage::Descriptors, 0x228 }, { 0x67, Storage::Descriptors, 0x22C }, { 0x77, Storage::Descriptors, 0xFF }, { 0x8D, Storage::Descriptors, 0x238 }, { 0x93, Storage::Descriptors, 0x23C }, { 0xA5, Storage::Descriptors, 0xFF }, { 0xE8, Storage::Descriptors, 0xFF }, { 0x127, Storage::Descriptors, 0xFF }, { 0x145, Storage::Descriptors, 0xFF }
        } },
        { Role::Other, "83 EC 08 56 57 E8 ? ? ? ? 8D 04 C0 03 C0 03 C0 8A 8C 00 ? ? ? ? 0F B6 BC 00 ? ? ? ?", {
            { 0x3B, Storage::Descriptors, 0x204 }, { 0x62, Storage::Descriptors, 0x228 }, { 0x6A, Storage::Descriptors, 0x22C }, { 0x72, Storage::Descriptors, 0x218 }, { 0x7D, Storage::Descriptors, 0x230 }, { 0x85, Storage::Descriptors, 0x234 }, { 0x8D, Storage::Descriptors, 0x21C }, { 0xEF, Storage::Maps, 0x0 }, { 0x11B, Storage::Descriptors, 0x0 }, { 0x14E, Storage::Descriptors, 0x200 }
        } },
        { Role::Enqueue, "A0 ? ? ? ? 3C 03 0F B6 C8 72 05 B9 03 00 00 00 8B 44 24 04 F3 0F 7E 00 F3 0F 7E 48 08 8B 44 24 08", {
            { 0x32, Storage::Counts, 0x0 }, { 0x49, Storage::Batches, 0x0 }, { 0x6E, Storage::Counts, 0x0 }
        } },
        { Role::Other, "83 EC 34 83 3D ? ? ? ? FF 75 0D 80 3D ? ? ? ? 03 0F 84 ? ? ? ? F3 0F 10 0D ? ? ? ?", {
            { 0x39D, Storage::Descriptors, 0x240 }, { 0x413, Storage::Descriptors, 0x6D4 }, { 0x434, Storage::Maps, 0x0 }, { 0x54C, Storage::Descriptors, 0x254 }, { 0x567, Storage::Descriptors, 0x24C }, { 0x572, Storage::Descriptors, 0x248 }, { 0x592, Storage::Descriptors, 0x250 }, { 0x5BC, Storage::Descriptors, 0x240 }
        } },
        { Role::Other, "83 EC 24 53 8B 5C 24 2C 55 56 57 53 E8 ? ? ? ? 8B 74 24 40 F3 0F 10 06 8B F8 A1 ? ? ? ?", {
            { 0x4D2, Storage::Descriptors, 0x240 }
        } },
        { Role::Values, "81 EC 38 01 00 00 A1 ? ? ? ? 33 C4 89 84 24 34 01 00 00 A0 ? ? ? ? 53 55 56 57 33 ED 33 DB", {
            { 0x3E1, Storage::Descriptors, 0x204 }, { 0x3E9, Storage::Descriptors, 0x208 }, { 0x3F1, Storage::Descriptors, 0x20C }, { 0x47B, Storage::Descriptors, 0xFD }, { 0x48D, Storage::Descriptors, 0xD0 }, { 0x49F, Storage::Maps, 0x0 }, { 0x4E2, Storage::Descriptors, 0x6F4 }, { 0x4EA, Storage::Descriptors, 0x6F8 }, { 0x552, Storage::Descriptors, 0x220 }, { 0x558, Storage::Descriptors, 0x224 }, { 0x5AA, Storage::Descriptors, 0x228 }, { 0x5B0, Storage::Descriptors, 0x22C }, { 0x602, Storage::Descriptors, 0x230 }, { 0x608, Storage::Descriptors, 0x234 }, { 0x65A, Storage::Descriptors, 0x238 }, { 0x660, Storage::Descriptors, 0x23C }, { 0x6B7, Storage::Descriptors, 0x204 }, { 0x6C5, Storage::Descriptors, 0x208 }, { 0x6D3, Storage::Descriptors, 0x20C }, { 0x7C8, Storage::Descriptors, 0xFF }, { 0x8B8, Storage::Descriptors, 0x0 }, { 0x934, Storage::Descriptors, 0x200 }, { 0x984, Storage::Descriptors, 0x218 }, { 0x9D1, Storage::Descriptors, 0x21C }
        } },
        { Role::Load, "81 EC 8C 00 00 00 A1 ? ? ? ? 33 C4 89 84 24 88 00 00 00 8B 84 24 90 00 00 00 55 83 CD FF 3B 05 ? ? ? ?", {
            { 0x17F, Storage::Descriptors, 0x708 }
        } },
        { Role::Init, "81 EC 8C 00 00 00 A1 ? ? ? ? 33 C4 89 84 24 88 00 00 00 53 55 56 83 CD FF 57 89 2D ? ? ? ?", {
            { 0x18F, Storage::Descriptors, 0x244 }, { 0x197, Storage::Descriptors, 0x248 }, { 0x1A7, Storage::Descriptors, 0x24C }, { 0x1B7, Storage::Descriptors, 0x250 }, { 0x1CB, Storage::Descriptors, 0x240 }, { 0x1D3, Storage::Descriptors, 0x254 }, { 0x1FA, Storage::Descriptors, 0x49C }, { 0x202, Storage::Descriptors, 0x4A0 }, { 0x212, Storage::Descriptors, 0x4A4 }, { 0x222, Storage::Descriptors, 0x4A8 }, { 0x22F, Storage::Descriptors, 0x498 }, { 0x237, Storage::Descriptors, 0x4AC }, { 0x24C, Storage::Descriptors, 0x4B0 }, { 0x56E, Storage::Batches, 0x0 }, { 0x582, Storage::Counts, 0x0 }, { 0x58A, Storage::Counts, 0x8 }
        } },
        { Role::Reload, "81 EC 94 00 00 00 A1 ? ? ? ? 33 C4 89 84 24 90 00 00 00 53 55 56 57 68 ? ? ? ? 8D 4C 24 14", {
            { 0x73, Storage::Descriptors, 0x240 }, { 0x79, Storage::Descriptors, 0x498 }, { 0x7F, Storage::Descriptors, 0x6F0 }, { 0x1F3, Storage::Descriptors, 0x244 }, { 0x1FB, Storage::Descriptors, 0x248 }, { 0x20B, Storage::Descriptors, 0x24C }, { 0x21B, Storage::Descriptors, 0x250 }, { 0x22F, Storage::Descriptors, 0x240 }, { 0x237, Storage::Descriptors, 0x254 }, { 0x25E, Storage::Descriptors, 0x49C }, { 0x266, Storage::Descriptors, 0x4A0 }, { 0x276, Storage::Descriptors, 0x4A4 }, { 0x286, Storage::Descriptors, 0x4A8 }, { 0x293, Storage::Descriptors, 0x498 }, { 0x29B, Storage::Descriptors, 0x4AC }, { 0x2B0, Storage::Descriptors, 0x4B0 }, { 0x2EF, Storage::Descriptors, 0x948 }, { 0x3C7, Storage::Descriptors, 0x708 }
        } },
        { Role::Shutdown, "56 83 CE FF 56 E8 ? ? ? ? 68 ? ? ? ? E8 ? ? ? ? 50 E8 ? ? ? ? 83 C4 0C 39 35 ? ? ? ?", {
            { 0x4F, Storage::Batches, 0x0 }
        } },
        { Role::Other, "55 8B EC 83 E4 F8 81 EC BC 00 00 00 A1 ? ? ? ? 33 C4 89 84 24 B8 00 00 00 80 3D ? ? ? ? 00", {
            { 0x1FD, Storage::Descriptors, 0x240 }, { 0x5E7, Storage::Descriptors, 0x240 }
        } },
    };



    static std::array<Descriptor, Capacity> descriptors{};
    static std::array<std::array<uint16_t, 768>, Capacity> maps{};
    static std::array<uint32_t, Capacity> counts{};
    static std::array<std::array<uint8_t, BatchBytes>, Capacity> batches{};
    static std::array<bool, Capacity> defined{};
    static void* dictionary = nullptr;
    static uint8_t* styles = nullptr;
    static bool legacy = false;
    static uintptr_t nextLine = 0;
    static int32_t (__cdecl* getContext)() = nullptr;
    static uint32_t (__cdecl* hashString)(const char*, uint32_t) = nullptr;
    static void* (__thiscall* findTexture)(void*, uint32_t) = nullptr;
    static SafetyHookInline valuesHook, styleHook, shutdownHook, loadedHook, loadHook;
    static SafetyHookMid idHook, completeHook, initialDictionaryHook, reloadDictionaryHook;

    static void Reset()
    {
        defined.fill(false);
        counts.fill(0);
        for (size_t i = FirstCustom; i < Capacity; ++i)
        {
            descriptors[i] = {};
            maps[i].fill(0);
        }
    }

    static bool Loaded(uint32_t id)
    {
        return id >= FirstCustom && id < Capacity && defined[id] && descriptors[id].texture;
    }

    static void Bind()
    {
        if (!dictionary) return;
        for (size_t id = FirstCustom; id < Capacity; ++id)
        {
            if (!defined[id]) continue;
            auto& font = descriptors[id];
            const auto name = "font" + std::to_string(id);
            font.texture = findTexture(dictionary, hashString(name.c_str(), 0));
            font.width = descriptors[0].width;
            font.height = descriptors[0].height;
            font.rowHeight = descriptors[0].rowHeight;
            font.top = descriptors[0].top;
            font.bottom = descriptors[0].bottom;
            if (!font.texture)
                OutputDebugStringA(("FusionFix fonts: missing " + name + " texture\n").c_str());
        }
    }

    static int32_t __cdecl LoadValues(int32_t id)
    {
        if (id == -1) Reset();
        const auto result = valuesHook.ccall<int32_t>(id);
        if (id == -1) Bind();
        return result;
    }

    static int32_t __cdecl SetStyle(uint32_t id)
    {
        if (id < FirstCustom) return styleHook.ccall<int32_t>(id);
        const auto result = styleHook.ccall<int32_t>(0);
        if (Loaded(id))
        {
            const auto context = getContext();
            auto style = styles + context * 72;
            style[0] = static_cast<uint8_t>(id);
            style[1] = 0;
        }
        return result;
    }

    static bool __cdecl IsLoaded(int32_t id)
    {
        return id >= static_cast<int32_t>(FirstCustom) ? Loaded(id) : loadedHook.ccall<bool>(id);
    }

    static void __cdecl Load(int32_t id)
    {
        if (id < static_cast<int32_t>(FirstCustom)) loadHook.ccall<void>(id);
    }

    static int32_t __cdecl Shutdown()
    {
        Reset();
        dictionary = nullptr;
        return shutdownHook.ccall<int32_t>();
    }

    static void FontId(SafetyHookContext& regs)
    {
        // At the return from sscanf, its arguments and getconfigline's arguments
        // still occupy 0x14 bytes. SafetyHook exposes the original engine ESP.
        const auto id = *reinterpret_cast<int32_t*>(regs.esp + 0x14 + (legacy ? 0x1C : 0xA8));
        if (regs.eax != 1 || id < 0 || id >= Capacity || (id >= 4 && id < FirstCustom))
        {
            *reinterpret_cast<uint8_t*>(regs.esp + 0x14 + 0x13) = 0;
            regs.esp += 0x14;
            regs.eip = nextLine;
        }
    }

    static void FontComplete(SafetyHookContext& regs)
    {
        const auto id = legacy ? regs.eax : regs.ebx;
        if (id < Capacity) defined[id] = true;
        *reinterpret_cast<uint8_t*>(regs.esp + 0x13) = 0;
        // WHITESPACE ends this font. Do not increment the font index or stop at
        // the stock count: subsequent FONT_ID blocks may be sparse or unordered.
        regs.eip = nextLine;
    }

    static void CaptureDictionary(SafetyHookContext& regs) { dictionary = reinterpret_cast<void*>(regs.ecx); }

    static void Initialize()
    {
        auto& initial = *std::find_if(ceFunctions.begin(), ceFunctions.end(), [](auto& f) { return f.role == Role::Init; });
        legacy = hook::pattern(initial.pattern).empty();
        auto& functions = legacy ? legacyFunctions : ceFunctions;
        std::array<uintptr_t, 4> oldBases{};
        std::array<uintptr_t, 4> newBases = {
            reinterpret_cast<uintptr_t>(descriptors.data()), reinterpret_cast<uintptr_t>(maps.data()),
            reinterpret_cast<uintptr_t>(counts.data()), reinterpret_cast<uintptr_t>(batches.data())
        };
        std::array<uintptr_t, 7> roles{};
        for (auto& function : functions)
        {
            auto pattern = hook::pattern(function.pattern);
            if (pattern.size() != 1) return;
            function.address = reinterpret_cast<uintptr_t>(pattern.get_first());
            roles[static_cast<size_t>(function.role)] = function.address;
            for (const auto& ref : function.references)
            {
                if (ref.storage == Storage::End) continue;
                auto& base = oldBases[static_cast<size_t>(ref.storage)];
                const auto value = *reinterpret_cast<uintptr_t*>(function.address + ref.operand) - ref.offset;
                if (base && base != value) return;
                base = value;
            }
        }
        for (auto base : oldBases) if (!base) return;
        for (const auto& function : functions)
            for (const auto& ref : function.references)
                if (ref.storage == Storage::End &&
                    *reinterpret_cast<uintptr_t*>(function.address + ref.operand) != oldBases[0] + 4 * sizeof(Descriptor) + offsetof(Descriptor, texture))
                    return;

        auto selector = find_pattern(
            "53 56 E8 ? ? ? ? 8B 5C 24 0C 53 8D 34 C0 E8 ? ? ? ? 88 04 F5 ? ? ? ? 8D 43 FC 83 C4 04",
            "E8 ? ? ? ? 8B 54 24 04 8D 0C C0 03 C9 03 C9 52 03 C9 E8 ? ? ? ? 88 81 ? ? ? ? 8D 42 FC");
        auto loaded = hook::pattern("A1 ? ? ? ? 3B 44 24 04 75 0C 80 3D ? ? ? ? 00 75 03 B0 01 C3 32 C0 C3");
        auto renderLimit = hook::pattern("80 F9 04 0F 83 ? ? ? ?");
        auto queueLimit = find_pattern("80 3C ED ? ? ? ? 04 72 08 C6 04 ED ? ? ? ? 00",
            "80 BE ? ? ? ? 04 72 07 C6 86 ? ? ? ? 00 D9 86");
        if (selector.size() != 1 || loaded.size() != 1 || renderLimit.size() != 1 || queueLimit.size() != 1) return;

        const auto init = roles[static_cast<size_t>(Role::Init)];
        const auto reload = roles[static_cast<size_t>(Role::Reload)];
        const auto values = roles[static_cast<size_t>(Role::Values)];
        const auto enqueue = roles[static_cast<size_t>(Role::Enqueue)];
        const auto initialLookup = init + (legacy ? 0x178 : 0x27D);
        const auto reloadLookup = reload + (legacy ? 0x1DC : 0x2F0);
        if (*reinterpret_cast<uint8_t*>(initialLookup) != 0xE8 || *reinterpret_cast<uint8_t*>(reloadLookup) != 0xE8) return;
        findTexture = reinterpret_cast<decltype(findTexture)>(injector::GetBranchDestination(initialLookup).as_int());
        if (injector::GetBranchDestination(reloadLookup).as_int() != reinterpret_cast<uintptr_t>(findTexture)) return;
        hashString = reinterpret_cast<decltype(hashString)>(injector::GetBranchDestination(initialLookup - 11).as_int());
        getContext = reinterpret_cast<decltype(getContext)>(injector::GetBranchDestination(selector.get_first(legacy ? 0 : 2)).as_int());
        styles = *selector.get_first<uint8_t*>(legacy ? 26 : 23) - 1;
        nextLine = values + (legacy ? 0x9D5 : 0xE26);

        valuesHook = safetyhook::create_inline(values, LoadValues);
        styleHook = safetyhook::create_inline(selector.get_first(), SetStyle);
        shutdownHook = safetyhook::create_inline(roles[static_cast<size_t>(Role::Shutdown)], Shutdown);
        loadedHook = safetyhook::create_inline(loaded.get_first(), IsLoaded);
        loadHook = safetyhook::create_inline(roles[static_cast<size_t>(Role::Load)], Load);
        idHook = safetyhook::create_mid(values + (legacy ? 0x3B2 : 0x5CC), FontId);
        completeHook = safetyhook::create_mid(values + (legacy ? 0x4EE : 0x733), FontComplete);
        initialDictionaryHook = safetyhook::create_mid(initialLookup, CaptureDictionary);
        reloadDictionaryHook = safetyhook::create_mid(reloadLookup, CaptureDictionary);
        if (!valuesHook || !styleHook || !shutdownHook || !loadedHook || !loadHook || !idHook ||
            !completeHook || !initialDictionaryHook || !reloadDictionaryHook)
        {
            reloadDictionaryHook.reset(); initialDictionaryHook.reset();
            completeHook.reset(); idHook.reset(); loadHook.reset(); loadedHook.reset();
            shutdownHook.reset(); styleHook.reset(); valuesHook.reset();
            return;
        }

        std::memcpy(descriptors.data(), reinterpret_cast<void*>(oldBases[0]), 4 * sizeof(Descriptor));
        std::memcpy(maps.data(), reinterpret_cast<void*>(oldBases[1]), 4 * sizeof(maps[0]));
        for (const auto& function : functions)
            for (const auto& ref : function.references)
            {
                const auto replacement = ref.storage == Storage::End ?
                    newBases[0] + sizeof(descriptors) + offsetof(Descriptor, texture) :
                    newBases[static_cast<size_t>(ref.storage)] + ref.offset;
                injector::WriteMemory(function.address + ref.operand, replacement, true);
            }
        injector::MakeNOP(enqueue + (legacy ? 0xA : 9), legacy ? 7 : 9, true);
        injector::MakeNOP(renderLimit.get_first(), 9, true);
        injector::MakeNOP(queueLimit.get_first(), legacy ? 16 : 18, true);
        injector::MakeNOP(values + (legacy ? 0x3C1 : 0x5D4), 9, true);
    }

    class Installation
    {
    public:
        Installation() { FusionFix::onInitEvent() += Initialize; }
    } installation;

}

