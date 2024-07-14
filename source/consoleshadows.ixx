module;

#include <common.hxx>

export module consoleshadows;

import common;
import settings;
import comvars;

void* fnAE3DE0 = nullptr;
void* fnAE3310 = nullptr;
bool bHeadlightShadows = true;
bool bVehicleNightShadows = false;
int __cdecl sub_AE3DE0(int a1, int a2)
{
    if (bVehicleNightShadows && !bHeadlightShadows)
        injector::cstd<void(int, int, int, int, int)>::call(fnAE3310, a1, 0, 0, 0, a2);
    return injector::cstd<int(int, int)>::call(fnAE3DE0, a1, a2);
}

SafetyHookInline sh_grcSetRendersState{};
uint32_t* dword_17F58E4;
void __stdcall grcSetRenderStateHook()
{
    if (dword_17F58E4 && *dword_17F58E4)
    {
        return sh_grcSetRendersState.unsafe_stdcall();
    }
}

class ConsoleShadows
{
public:
    ConsoleShadows()
    {
        FusionFix::onInitEventAsync() += []()
        {
            CIniReader iniReader("");
            bHeadlightShadows = iniReader.ReadInteger("NIGHTSHADOWS", "HeadlightShadows", 1) != 0;
            bVehicleNightShadows = iniReader.ReadInteger("NIGHTSHADOWS", "VehicleNightShadows", 0) != 0;

            // Render dynamic shadows casted by vehicles from point lights.
            {
                auto pattern = hook::pattern("E8 ? ? ? ? 83 C4 08 57 56");
                fnAE3DE0 = injector::GetBranchDestination(pattern.get_first()).get();
                injector::MakeCALL(pattern.get_first(), sub_AE3DE0, true);
                pattern = find_pattern("55 8B EC 83 E4 F0 83 EC 28 80 3D ? ? ? ? ? 56 57 74 27", "55 8B EC 83 E4 F0 83 EC 24 53 56 57 33 FF 80 3D");
                fnAE3310 = pattern.get_first();
            }

            // Fix crash caused by the patches below.
            {
                auto pattern = find_pattern("89 35 ? ? ? ? E8 ? ? ? ? 5E C2 04 00", "89 1D ? ? ? ? 8B 07 8B 50 10 8B CF 89 1D");
                dword_17F58E4 = *pattern.get_first<uint32_t*>(2);
                pattern = find_pattern("51 53 55 8B 2D ? ? ? ? 56 0F B7 5D 1C 33 F6 85 DB 7E 3A 57 EB 09", "53 56 57 8B 3D ? ? ? ? 0F B7 5F 1C 33 F6 85 DB 7E 41 55 EB 0A");
                sh_grcSetRendersState = safetyhook::create_inline(pattern.get_first(0), grcSetRenderStateHook);
            }

            // Enable player/ped shadows while in vehicles
            if (bVehicleNightShadows && !bHeadlightShadows)
            {
                auto pattern = hook::pattern("75 14 F6 86 ? ? ? ? ? 74 0B 80 7C 24 ? ? 0F 84 ? ? ? ? C6 44 24");
                if (!pattern.empty())
                {
                    injector::WriteMemory<uint8_t>(pattern.get_first(0), 0xEB, true);
                    pattern = hook::pattern("75 12 8B 86 ? ? ? ? C1 E8 0B 25 ? ? ? ? 89 44 24 0C 85 D2");
                    injector::WriteMemory<uint8_t>(pattern.get_first(0), 0xEB, true);
                }
                else
                {
                    pattern = hook::pattern("75 17 F6 86 ? ? ? ? ? 74 0E 80 7C 24 ? ? 0F 84");
                    injector::WriteMemory<uint8_t>(pattern.get_first(0), 0xEB, true);
                    pattern = hook::pattern("75 0F 8B 86 ? ? ? ? C1 E8 0B 24 01 88 44 24 0E");
                    injector::WriteMemory<uint8_t>(pattern.get_first(0), 0xEB, true);
                }

            }

            // Disable headlight shadows to avoid flickering/self-shadowing.
            if (!bHeadlightShadows)
            {
                auto pattern = hook::pattern("74 76 FF 75 30 FF 75 2C FF 75 28 83 EC 0C 80 7D 38 00");
                if (!pattern.empty())
                {
                    injector::WriteMemory<uint8_t>(pattern.get_first(0), 0xEB, true);
                    pattern = hook::pattern("68 ? ? ? ? 6A 02 6A 00 E8 ? ? ? ? 83 C4 40 8B E5 5D C3 68");
                    injector::WriteMemory(pattern.count(2).get(1).get<void*>(1), 0x100, true);
                    pattern = hook::pattern("8B E5 5D C3 68 ? ? ? ? 6A 02 6A 00 E8 ? ? ? ? 83 C4 40 8B E5 5D C3");
                    injector::WriteMemory(pattern.count(2).get(1).get<void*>(5), 0x100, true);
                }
                else
                {
                    pattern = hook::pattern("0F 84 ? ? ? ? 80 7D 28 00 74 4C 8B 45 20 8B 0D");
                    injector::WriteMemory<uint16_t>(pattern.get_first(0), 0xE990, true);
                    pattern = hook::pattern("68 ? ? ? ? 6A 02 6A 00 E8 ? ? ? ? 83 C4 40 5B 8B E5 5D C3 8B 15");
                    injector::WriteMemory(pattern.get_first(1), 0x100, true);
                    pattern = hook::pattern("8D 44 24 50 50 68 ? ? ? ? 6A 02 6A 00 E8 ? ? ? ? 83 C4 40 5B 8B E5 5D C3");
                    injector::WriteMemory(pattern.get_first(6), 0x100, true);
                }
            }
        };
    }
} ConsoleShadows;