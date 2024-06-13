module;

#include <common.hxx>

export module consoleshadows;

import common;
import settings;
import comvars;

void* fnAE3DE0 = nullptr;
void* fnAE3310 = nullptr;
int __cdecl sub_AE3DE0(int a1, int a2)
{
    static auto console_shadows = FusionFixSettings.GetRef("PREF_CONSOLE_SHADOWS");
    if (console_shadows->get() || bMoreShadows)
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

//injector::hook_back<void(__cdecl*)(int, int, int, float*, int*, float*, int, int, int, int, float, float, float, int, int, int, int, int)> hbsub_ABCCD0;
//void __cdecl sub_ABCCD0(int a1, int a2, int a3, float* a4, int* a5, float* a6, int a7, int a8, int a9, int a10, float a11, float a12, float a13, int a14, int a15, int a16, int a17, int a18)
//{
//    static auto console_shadows = FusionFixSettings.GetRef("PREF_CONSOLE_SHADOWS");
//    if (console_shadows->get())
//        return;
//    else
//        return hbsub_ABCCD0.fun(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14, a15, a16, a17, a18);
//}
//
//injector::hook_back<bool(*)()> hbsub_924D90;
//bool sub_924D90()
//{
//    static auto console_shadows = FusionFixSettings.GetRef("PREF_CONSOLE_SHADOWS");
//    if (console_shadows->get())
//        return false;
//    else
//        return hbsub_924D90.fun();
//};

class ConsoleShadows
{
public:
    ConsoleShadows()
    {
        FusionFix::onInitEvent() += []()
        {
            CIniReader iniReader("");
            bMoreShadows = iniReader.ReadInteger("SHADOWS", "MoreShadows", 0) != 0;

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

            // Toggle: Disable headlight shadows to avoid flickering/self-shadowing & enable player/ped shadows while in vehicles.
            {
                auto pattern = hook::pattern("74 76 FF 75 30 FF 75 2C FF 75 28 83 EC 0C 80 7D 38 00");
                if (!pattern.empty())
                {
                    static raw_mem NoHeadlightShadowsCB(pattern.get_first(0), { 0xEB });
                    pattern = hook::pattern("68 ? ? ? ? 6A 02 6A 00 E8 ? ? ? ? 83 C4 40 8B E5 5D C3 68");
                    static raw_mem NoHeadlightShadowsCB2(pattern.count(2).get(1).get<void*>(1), { 0x00, 0x01, 0x00, 0x00 });
                    pattern = hook::pattern("8B E5 5D C3 68 ? ? ? ? 6A 02 6A 00 E8 ? ? ? ? 83 C4 40 8B E5 5D C3");
                    static raw_mem NoHeadlightShadowsCB3(pattern.count(2).get(1).get<void*>(5), { 0x00, 0x01, 0x00, 0x00 });
                    auto pattern = hook::pattern("75 14 F6 86 ? ? ? ? ? 74 0B 80 7C 24 ? ? 0F 84 ? ? ? ? C6 44 24");
                    static raw_mem CarPartShadowsCB(pattern.get_first(0), { 0xEB });
                    pattern = hook::pattern("75 12 8B 86 ? ? ? ? C1 E8 0B 25 ? ? ? ? 89 44 24 0C 85 D2");
                    static raw_mem CarPartShadowsCB2(pattern.get_first(0), { 0xEB });
                    FusionFixSettings.SetCallback("PREF_CONSOLE_SHADOWS", [](int32_t value) {
                        if (value)
                        {
                            NoHeadlightShadowsCB.Write();
                            NoHeadlightShadowsCB2.Write();
                            NoHeadlightShadowsCB3.Write();
                            CarPartShadowsCB.Write();
                            CarPartShadowsCB2.Write();
                        }
                        else
                        {
                            NoHeadlightShadowsCB.Restore();
                            NoHeadlightShadowsCB2.Restore();
                            NoHeadlightShadowsCB3.Restore();
                            CarPartShadowsCB.Restore();
                            CarPartShadowsCB2.Restore();
                        }
                    });
                    if (FusionFixSettings("PREF_CONSOLE_SHADOWS"))
                    {
                        NoHeadlightShadowsCB.Write();
                        NoHeadlightShadowsCB2.Write();
                        NoHeadlightShadowsCB3.Write();
                        CarPartShadowsCB.Write();
                        CarPartShadowsCB2.Write();
                    }
                }
                else
                {
                    pattern = hook::pattern("0F 84 ? ? ? ? 80 7D 28 00 74 4C 8B 45 20 8B 0D");
                    static raw_mem NoHeadlightShadowsCB(pattern.get_first(0), { 0x90, 0xE9 });
                    pattern = hook::pattern("68 ? ? ? ? 6A 02 6A 00 E8 ? ? ? ? 83 C4 40 5B 8B E5 5D C3 8B 15");
                    static raw_mem NoHeadlightShadowsCB2(pattern.get_first(1), { 0x00, 0x01, 0x00, 0x00 });
                    pattern = hook::pattern("8D 44 24 50 50 68 ? ? ? ? 6A 02 6A 00 E8 ? ? ? ? 83 C4 40 5B 8B E5 5D C3");
                    static raw_mem NoHeadlightShadowsCB3(pattern.get_first(6), { 0x00, 0x01, 0x00, 0x00 });
                    pattern = hook::pattern("75 17 F6 86 ? ? ? ? ? 74 0E 80 7C 24 ? ? 0F 84");
                    static raw_mem CarPartShadowsCB(pattern.get_first(0), { 0xEB });
                    pattern = hook::pattern("75 0F 8B 86 ? ? ? ? C1 E8 0B 24 01 88 44 24 0E");
                    static raw_mem CarPartShadowsCB2(pattern.get_first(0), { 0xEB });
                    FusionFixSettings.SetCallback("PREF_CONSOLE_SHADOWS", [](int32_t value) {
                        if (value)
                        {
                            NoHeadlightShadowsCB.Write();
                            NoHeadlightShadowsCB2.Write();
                            NoHeadlightShadowsCB3.Write();
                            CarPartShadowsCB.Write();
                            CarPartShadowsCB2.Write();
                        }
                        else
                        {
                            NoHeadlightShadowsCB.Restore();
                            NoHeadlightShadowsCB2.Restore();
                            NoHeadlightShadowsCB3.Restore();
                            CarPartShadowsCB.Restore();
                            CarPartShadowsCB2.Restore();
                        }
                    });
                    if (FusionFixSettings("PREF_CONSOLE_SHADOWS"))
                    {
                        NoHeadlightShadowsCB.Write();
                        NoHeadlightShadowsCB2.Write();
                        NoHeadlightShadowsCB3.Write();
                        CarPartShadowsCB.Write();
                        CarPartShadowsCB2.Write();
                    }
                }
            }

            // disable shadows for console shadows option
            //auto pattern = find_pattern("E8 ? ? ? ? 8B 3D ? ? ? ? 84 C0 75 59", "E8 ? ? ? ? 84 C0 A1 ? ? ? ? 75 59");
            //hbsub_924D90.fun = injector::MakeCALL(pattern.get_first(0), static_cast<bool(*)()>(sub_924D90)).get();
            //pattern = find_pattern("E8 ? ? ? ? 8B 0D ? ? ? ? 84 C0 75 4F", "E8 ? ? ? ? 8B 15 ? ? ? ? 84 C0 8B 44 24 1C");
            //hbsub_924D90.fun = injector::MakeCALL(pattern.get_first(0), static_cast<bool(*)()>(sub_924D90)).get();

            // disable heli searchlight (becomes bugged)
            //pattern = find_pattern("E8 ? ? ? ? F3 0F 10 2D ? ? ? ? F3 0F 10 9C 24", "E8 ? ? ? ? F3 0F 10 15 ? ? ? ? F3 0F 10 1D ? ? ? ? F3 0F 10 44 24");
            //hbsub_ABCCD0.fun = injector::MakeCALL(pattern.get_first(0), sub_ABCCD0).get();
        };
    }
} ConsoleShadows;