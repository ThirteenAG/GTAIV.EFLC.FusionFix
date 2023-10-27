module;

#include <common.hxx>

export module mousefix;

import common;
import settings;

uint32_t dword_F43AD8;
uint32_t dword_1826D48;
uint32_t dword_1826D34;
uint32_t dword_1826D4C;
uint32_t dword_1826D6C;
double __cdecl fnMouseFix(void*, int32_t axis)
{
    auto address = *(int32_t*)(dword_F43AD8 + 0xC);
    if (*(BYTE*)(address + 0x1C4) & 8)
    {
        *(int32_t*)(dword_1826D48) = 0;
        *(int32_t*)(dword_1826D34) = 0;
        *(int32_t*)(dword_1826D4C) = 0;
        *(int32_t*)(dword_1826D6C) = 0;
    }
    if (axis == 0)
    {
        return (double)(*(int32_t*)(dword_1826D48)) * 0.0078125;
    }
    else if (axis == 1)
    {
        return (double)(*(int32_t*)(dword_1826D34)) * 0.0078125;
    }

    return 0.0;
}

int* dword_1160FE8 = nullptr;
int __cdecl sub_8B8120(int a1)
{
    if (dword_1160FE8[a1] > 0)
        return dword_1160FE8[a1] * 2;
    return 1;
}

class MouseFix
{
public:
    MouseFix()
    {
        FusionFix::onInitEvent() += []()
        {
            CIniReader iniReader("");

            bool bMouseFix = iniReader.ReadInteger("MISC", "MouseFix", 1) != 0;

            if (bMouseFix)
            {
                auto pattern = hook::pattern("B9 ? ? ? ? E8 ? ? ? ? 85 C0 0F 84 ? ? ? ? F3 0F 10 9C 24 ? ? ? ? F3 0F 10 A4 24 ? ? ? ? F3 0F 10 AC 24");
                if (!pattern.empty())
                {
                    dword_F43AD8 = *pattern.get_first<uint32_t>(1);
                    pattern = hook::pattern("74 3D C7 05");
                    dword_1826D48 = *pattern.get_first<int32_t>(4);
                    dword_1826D34 = *pattern.get_first<int32_t>(14);
                    dword_1826D4C = *pattern.get_first<int32_t>(24);
                    dword_1826D6C = *pattern.get_first<int32_t>(34);
                    pattern = hook::pattern("51 8B 54 24 0C C7 04 24 ? ? ? ? 85 D2 75 0D 39 15 ? ? ? ? 75 17 D9 04 24 59 C3");
                    injector::MakeJMP(pattern.get_first(0), fnMouseFix, true);
                }
                else
                {
                    auto pattern = hook::pattern("B9 ? ? ? ? E8 ? ? ? ? 85 C0 0F 84 ? ? ? ? 80 7D 08 00");
                    dword_F43AD8 = *pattern.get_first<uint32_t>(1);
                    pattern = hook::pattern("53 33 DB 38 5C 24 08 74 26");
                    dword_1826D48 = *pattern.get_first<int32_t>(11);
                    dword_1826D34 = *pattern.get_first<int32_t>(11 + 6);
                    dword_1826D4C = *pattern.get_first<int32_t>(11 + 6 + 6);
                    dword_1826D6C = *pattern.get_first<int32_t>(11 + 6 + 6 + 6);
                    pattern = hook::pattern("51 8B 44 24 0C 85 C0 0F 57 D2 56 8B 35");
                    injector::MakeJMP(pattern.get_first(0), fnMouseFix, true);
                }

                //ped cam behaves the same way as aiming cam
                static constexpr float f255 = 127.5f * 2.0f;
                pattern = hook::pattern("F3 0F 59 05 ? ? ? ? F3 0F 2C C0 83 C4 0C");
                if (!pattern.empty())
                    injector::WriteMemory(pattern.get_first(4), &f255, true);
                else {
                    pattern = hook::pattern("51 E8 ? ? ? ? D8 0D");
                    injector::WriteMemory(pattern.get_first(8), &f255, true);
                }

                //sensitivity adjustment
                pattern = find_pattern("8B 44 24 04 8B 0C 85 ? ? ? ? B8", "8B 44 24 04 8B 04 85 ? ? ? ? 85 C0 7F 05");
                if (!pattern.empty())
                {
                    dword_1160FE8 = *pattern.get_first<int*>(7);
                    injector::MakeJMP(pattern.get_first(0), sub_8B8120, true);
                }

                //sniper scope is slow
                static constexpr float f01 = 0.1f;
                pattern = hook::pattern("F3 0F 59 15 ? ? ? ? F3 0F 58 97");
                if (!pattern.empty())
                {
                    injector::WriteMemory(pattern.get_first(4), &f01, true);
                    injector::WriteMemory(pattern.get_first(28), &f01, true);
                }

                //first person vehicle view is slow
                pattern = hook::pattern("F3 0F 59 15 ? ? ? ? F3 0F 58 96");
                if (!pattern.empty())
                {
                    injector::WriteMemory(pattern.get_first(4), &f01, true);
                    injector::WriteMemory(pattern.get_first(28), &f01, true);
                }
            }
        };
    }
} MouseFix;