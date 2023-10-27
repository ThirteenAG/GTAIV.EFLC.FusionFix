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

injector::hook_back<void(__cdecl*)(int, int, int, float*, int*, float*, int, int, int, int, float, float, float, int, int, int, int, int)> hbsub_ABCCD0;
void __cdecl sub_ABCCD0(int a1, int a2, int a3, float* a4, int* a5, float* a6, int a7, int a8, int a9, int a10, float a11, float a12, float a13, int a14, int a15, int a16, int a17, int a18)
{
    static auto console_shadows = FusionFixSettings.GetRef("PREF_CONSOLE_SHADOWS");
    if (console_shadows->get())
        return;
    else
        return hbsub_ABCCD0.fun(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14, a15, a16, a17, a18);
}

injector::hook_back<bool(*)()> hbsub_924D90;
bool sub_924D90()
{
    static auto console_shadows = FusionFixSettings.GetRef("PREF_CONSOLE_SHADOWS");
    if (console_shadows->get())
        return false;
    else
        return hbsub_924D90.fun();
};

class ConsoleShadows
{
public:
    ConsoleShadows()
    {
        FusionFix::onInitEvent() += []()
        {
            CIniReader iniReader("");
            bMoreShadows = iniReader.ReadInteger("SHADOWS", "MoreShadows", 0) != 0;

            // render more shadows here
            auto pattern = hook::pattern("E8 ? ? ? ? 83 C4 08 57 56");
            fnAE3DE0 = injector::GetBranchDestination(pattern.get_first()).get();
            injector::MakeCALL(pattern.get_first(), sub_AE3DE0, true);
            pattern = find_pattern("55 8B EC 83 E4 F0 83 EC 28 80 3D ? ? ? ? ? 56 57 74 27", "55 8B EC 83 E4 F0 83 EC 24 53 56 57 33 FF 80 3D");
            fnAE3310 = pattern.get_first();
            
            // disable shadows for console shadows option
            pattern = find_pattern("E8 ? ? ? ? 8B 3D ? ? ? ? 84 C0 75 59", "E8 ? ? ? ? 84 C0 A1 ? ? ? ? 75 59");
            hbsub_924D90.fun = injector::MakeCALL(pattern.get_first(0), static_cast<bool(*)()>(sub_924D90)).get();
            pattern = find_pattern("E8 ? ? ? ? 8B 0D ? ? ? ? 84 C0 75 4F", "E8 ? ? ? ? 8B 15 ? ? ? ? 84 C0 8B 44 24 1C");
            hbsub_924D90.fun = injector::MakeCALL(pattern.get_first(0), static_cast<bool(*)()>(sub_924D90)).get();
            
            // disable heli searchlight (becomes bugged)
            pattern = find_pattern("E8 ? ? ? ? F3 0F 10 2D ? ? ? ? F3 0F 10 9C 24", "E8 ? ? ? ? F3 0F 10 15 ? ? ? ? F3 0F 10 1D ? ? ? ? F3 0F 10 44 24");
            hbsub_ABCCD0.fun = injector::MakeCALL(pattern.get_first(0), sub_ABCCD0).get();
        };
    }
} ConsoleShadows;