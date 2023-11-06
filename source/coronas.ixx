module;

#include <common.hxx>

export module coronas;

import common;

static constexpr auto NewLimitExponent = 14;

void IncreaseCoronaLimit()
{
    using namespace injector;

    auto nCoronasLimit = static_cast<uint32_t>(3 * pow(2.0, NewLimitExponent)); // 49152, default 3 * pow(2, 8) = 768

    static std::vector<uint32_t> aCoronas;
    static std::vector<uint32_t> aCoronas2;
    aCoronas.resize(nCoronasLimit * 0x3C * 4);
    aCoronas2.resize(nCoronasLimit * 0x3C * 4);

    int32_t counter1 = 0;
    int32_t counter2 = 0;

    uintptr_t range_start = (uintptr_t)hook::get_pattern("33 C0 C7 80 ? ? ? ? ? ? ? ? 83 C0 40 3D ? ? ? ? 72 EC C7 05 ? ? ? ? ? ? ? ? C3");
    uintptr_t range_end = (uintptr_t)hook::get_pattern("5E C3 FF 05 ? ? ? ? 5E C3");

    uintptr_t dword_temp = (uintptr_t)*hook::pattern("89 82 ? ? ? ? F3 0F 11 82 ? ? ? ? F3 0F 10 44 24 ? F3 0F 11 8A ? ? ? ? 8B 41 0C 0F B6 4C 24 ? 89 82").get(0).get<uint32_t*>(2);

    for (size_t i = dword_temp; i <= (dword_temp + 0x3C); i++)
    {
        auto GoThroughPatterns = [&](const char* pattern_str, int32_t pos) -> void
        {
            auto patternl = hook::range_pattern(range_start, range_end, pattern_str);
            for (size_t j = 0; j < patternl.size(); j++)
            {
                if (*patternl.get(j).get<uintptr_t>(pos) == i)
                {
                    AdjustPointer(patternl.get(j).get<uint32_t>(pos), &aCoronas[0], dword_temp, dword_temp + 0x3C);
                    counter1++;
                }
            }
        };

        GoThroughPatterns("83 8A", 2);
        GoThroughPatterns("88 82", 2);
        GoThroughPatterns("89 82", 2);
        GoThroughPatterns("89 82", 2);
        GoThroughPatterns("89 8A", 2);
        GoThroughPatterns("8B 82", 2);
        GoThroughPatterns("BE", 1);
        GoThroughPatterns("C7 80", 2);
        GoThroughPatterns("C7 82", 2);
        GoThroughPatterns("F3 0F 11 82", 4);
        GoThroughPatterns("F3 0F 11 8A", 4);
    }

    range_start = (uintptr_t)hook::get_pattern("8B 44 24 1C F3 0F 10 4C 24 ? F3 0F 10 46 ? F3 0F 59 05 ? ? ? ? 8D 0C 40");
    range_end = (uintptr_t)hook::get_pattern("0F 29 85 ? ? ? ? 0F 28 9D ? ? ? ? F3 0F 5C C2 F3 0F 11 5C 24 ? 0F 28 9D ? ? ? ? F3 0F 11 5C 24 ? 0F 28 D0");

    dword_temp = (uintptr_t)*hook::pattern("F3 0F 11 89 ? ? ? ? F3 0F 10 4C 24 ? F3 0F 11 89").get(0).get<uint32_t*>(4);

    for (size_t i = dword_temp; i <= (dword_temp + 0x1B); i++)
    {
        auto GoThroughPatterns = [&](const char* pattern_str, int32_t pos) -> void
        {
            auto patternl = hook::range_pattern(range_start, range_end, pattern_str);
            for (size_t j = 0; j < patternl.size(); j++)
            {
                if (*patternl.get(j).get<uintptr_t>(pos) == i)
                {
                    AdjustPointer(patternl.get(j).get<uint32_t>(pos), &aCoronas2[0], dword_temp, dword_temp + 0x1B);
                    counter2++;
                }
            }
        };

        GoThroughPatterns("0F 28 81", 3);
        GoThroughPatterns("0F B6 81", 3);
        GoThroughPatterns("0F B6 B1", 3);
        GoThroughPatterns("80 B8", 2);
        GoThroughPatterns("88 81", 2);
        GoThroughPatterns("88 91", 2);
        GoThroughPatterns("89 81", 2);
        GoThroughPatterns("BE", 1);
        GoThroughPatterns("F3 0F 10 81", 4);
        GoThroughPatterns("F3 0F 10 89", 4);
        GoThroughPatterns("F3 0F 11 81", 4);
        GoThroughPatterns("F3 0F 11 89", 4);
        GoThroughPatterns("F3 0F 11 91", 4);
    }

    if (counter1 != 24 || counter2 != 18)
        MessageBoxW(0, L"Fusion Fix", L"Fusion Fix is not fully compatible with this version of the game", 0);

    auto p = hook::pattern("BF FF 02 00 00");
    AdjustPointer(p.get_first(-4), &aCoronas[0], dword_temp, dword_temp + 0x3C);
    p = hook::pattern("BF FF 05 00 00");
    AdjustPointer(p.get_first(-4), &aCoronas2[0], dword_temp, dword_temp + 0x1B);

    auto pattern = hook::pattern("C1 E1 ? 03 4C 24 18 C1");
    WriteMemory<uint8_t>(pattern.get(0).get<uintptr_t>(2), NewLimitExponent, true);
    pattern = hook::pattern("C1 E1 ? 03 CF C1 E1");
    WriteMemory<uint8_t>(pattern.get(0).get<uintptr_t>(2), NewLimitExponent, true);
    pattern = hook::pattern("C1 E0 ? 03 C2 C1 E0 05 80 B8");
    WriteMemory<uint8_t>(pattern.get(0).get<uintptr_t>(2), NewLimitExponent, true);

    pattern = hook::pattern("81 FE ? ? ? ? 0F 8D ? ? ? ? 8B 44 24 08 8B 4C 24 1C F3 0F 10 44 24 ? C1 E2 06");
    WriteMemory<uint32_t>(pattern.get(0).get<uintptr_t>(2), nCoronasLimit, true);
    pattern = hook::pattern("3D ? ? ? ? 0F 8D ? ? ? ? 8B 44 24 1C F3 0F 10 4C 24");
    WriteMemory<uint32_t>(pattern.get(0).get<uintptr_t>(1), nCoronasLimit, true);
    pattern = hook::pattern("3D ? ? ? ? 72 EC C7 05 ? ? ? ? ? ? ? ? C3");
    WriteMemory<uint32_t>(pattern.get(0).get<uintptr_t>(1), nCoronasLimit * 64, true);
}

void IncreaseCoronaLimitEFLC()
{
    using namespace injector;

    auto nCoronasLimit = static_cast<uint32_t>(3 * pow(2.0, NewLimitExponent)); // 49152, default 3 * pow(2, 8) = 768

    static std::vector<uint32_t> aCoronas;
    static std::vector<uint32_t> aCoronas2;
    aCoronas.resize(nCoronasLimit * 0x3C * 4);
    aCoronas2.resize(nCoronasLimit * 0x3C * 4);

    int32_t counter1 = 0;
    int32_t counter2 = 0;

    uintptr_t range_start = (uintptr_t)hook::get_pattern("BE ? ? ? ? 83 7E 18 00");
    uintptr_t range_end = (uintptr_t)hook::get_pattern("5E C3 83 05 ? ? ? ? ? 5E C3");

    uintptr_t dword_temp = (uintptr_t)*hook::pattern("D9 98 ? ? ? ? F3 0F 10 44 24 20 F3 0F 11 88").get(0).get<uint32_t*>(2);

    for (size_t i = dword_temp; i <= (dword_temp + 0x3C); i++)
    {
        auto GoThroughPatterns = [&](const char* pattern_str, int32_t pos) -> void
        {
            auto patternl = hook::range_pattern(range_start, range_end, pattern_str);
            for (size_t j = 0; j < patternl.size(); j++)
            {
                if (*patternl.get(j).get<uintptr_t>(pos) == i)
                {
                    AdjustPointer(patternl.get(j).get<uint32_t>(pos), &aCoronas[0], dword_temp, dword_temp + 0x3C);
                    counter1++;
                }
            }
        };

        GoThroughPatterns("D9 98", 2);
        GoThroughPatterns("F3 0F 11 80", 4);
        GoThroughPatterns("F3 0F 11 88", 4);
        GoThroughPatterns("83 88 ? ? ? ? 10", 2);
        GoThroughPatterns("83 88 ? ? ? ? 06", 2);
        GoThroughPatterns("BE", 1);
        GoThroughPatterns("89 88", 2);
        GoThroughPatterns("88 90", 2);
        GoThroughPatterns("88 88", 2);
        GoThroughPatterns("8B 90", 2);
    }

    range_start = (uintptr_t)hook::get_pattern("D9 46 20 F3 0F 10 54 24 ? 0F B6 56 30"); //0x7E10AB
    range_end = (uintptr_t)hook::get_pattern("F3 0F 59 C3 F3 0F 5C F0 F3 0F 59 CB"); //0x7E1671

    dword_temp = (uintptr_t)*hook::pattern("F3 0F 11 90 ? ? ? ? F3 0F 10 54 24 24 F3 0F").get(0).get<uint32_t*>(4);

    for (size_t i = dword_temp; i <= (dword_temp + 0x1B); i++)
    {
        auto GoThroughPatterns = [&](const char* pattern_str, int32_t pos) -> void
        {
            auto patternl = hook::range_pattern(range_start, range_end, pattern_str);
            for (size_t j = 0; j < patternl.size(); j++)
            {
                if (*patternl.get(j).get<uintptr_t>(pos) == i)
                {
                    AdjustPointer(patternl.get(j).get<uint32_t>(pos), &aCoronas2[0], dword_temp, dword_temp + 0x1B);
                    counter2++;
                }
            }
        };

        GoThroughPatterns("0F 28 89", 3);
        GoThroughPatterns("88 90", 2);
        GoThroughPatterns("88 88", 2);
        GoThroughPatterns("F3 0F 11 90", 4);
        GoThroughPatterns("F3 0F 11 80", 4);
        GoThroughPatterns("F3 0F 10 81", 4);
        GoThroughPatterns("F3 0F 11 88", 4);
        GoThroughPatterns("F3 0F 10 A1", 4);
        GoThroughPatterns("F3 0F 10 99", 4);
        GoThroughPatterns("D9 98", 2);
        GoThroughPatterns("80 B9 ? ? ? ? 00", 2);
        GoThroughPatterns("0F B6 B1", 3);
        GoThroughPatterns("0F B6 81", 3);
        GoThroughPatterns("0F B6 91", 3);
    }

    if (counter1 != 24 || counter2 != 18)
        MessageBoxW(0, L"Fusion Fix", L"Fusion Fix is not fully compatible with this version of the game", 0);

    auto pattern = hook::pattern("C1 E0 ? 03 C3 C1 E0");
    WriteMemory<uint8_t>(pattern.get(0).get<uintptr_t>(2), NewLimitExponent, true);
    pattern = hook::pattern("C1 E1 ? 03 CF C1 E1");
    WriteMemory<uint8_t>(pattern.get(0).get<uintptr_t>(2), NewLimitExponent, true);
    pattern = hook::pattern("C1 E1 ? 03 C8 C1 E1");
    WriteMemory<uint8_t>(pattern.get(0).get<uintptr_t>(2), NewLimitExponent, true);

    pattern = hook::pattern("81 FE ? ? ? ? 0F 8D ? ? ? ? 8B 4C 24 08 8A 54 24 0C");
    WriteMemory<uint32_t>(pattern.get(0).get<uintptr_t>(2), nCoronasLimit, true);
    pattern = hook::pattern("81 FF ? ? ? ? 0F 8D ? ? ? ? 8B 44 24 1C D9 46 20");
    WriteMemory<uint32_t>(pattern.get(0).get<uintptr_t>(2), nCoronasLimit, true);
    pattern = hook::pattern("3D ? ? ? ? 72 ? 89 0D ? ? ? ? C3");
    WriteMemory<uint32_t>(pattern.get(0).get<uintptr_t>(1), nCoronasLimit * 64, true);
}

class Coronas
{
public:
    Coronas()
    {
        FusionFix::onInitEventAsync() += []()
        {
            auto pattern = hook::pattern("81 FE ? ? ? ? 0F 8D ? ? ? ? 8B 44 24 08 8B 4C 24 1C F3 0F 10 44 24 ? C1 E2 06");
            if (!pattern.empty())
            {
                auto i = injector::ReadMemory<uint32_t>(pattern.get(0).get<uintptr_t>(2), true);
                if (i == 768)
                    IncreaseCoronaLimit();
            }
            else
            {
                pattern = hook::pattern("81 FE ? ? ? ? 0F 8D ? ? ? ? 8B 4C 24 08 8A 54 24 0C");
                auto i = injector::ReadMemory<uint32_t>(pattern.get(0).get<uintptr_t>(2), true);
                if (i == 768)
                    IncreaseCoronaLimitEFLC();
            }
        };
    }
} Coronas;