module;

#include <common.hxx>

export module skipintro;

import common;
import settings;

bool bSkipIntroNotNeeded = false;
injector::hook_back<void(__cdecl*)(int)> hbsub_7870A0;
void __cdecl sub_7870A0(int a1)
{
    static bool bOnce = false;
    if (!bOnce)
    {
        bSkipIntroNotNeeded = true;
        if (a1 == 0)
        {
            bool bNoLoad = (GetAsyncKeyState(VK_SHIFT) & 0xF000) != 0;
            if (!bNoLoad && FusionFixSettings("PREF_SKIP_MENU"))
                a1 = 6;

            bOnce = true;
        }
    }
    return hbsub_7870A0.fun(a1);
}

namespace CGame
{
    bool bAfterFirstRun = false;
    injector::hook_back<void(__fastcall*)(int, int, int)> hbshowLoadscreen;
    void __fastcall showLoadscreen(int dl, int cl, int a1)
    {
        if (bAfterFirstRun)
            return hbshowLoadscreen.fun(dl, cl, a1);
        else
            bAfterFirstRun = true;
    }
}

class SkipIntro
{
public:
    SkipIntro()
    {
        FusionFix::onInitEvent() += []()
        {
            bool bSkipIntro = FusionFixSettings("PREF_SKIP_INTRO") != 0;
            bool bSkipMenu = FusionFixSettings("PREF_SKIP_MENU") != 0;

            if (bSkipIntro)
            {
                auto pattern = find_pattern("8B 0D ? ? ? ? 8B 44 24 18 83 C4 0C 69 C9 ? ? ? ? 89 81 ? ? ? ? 8D 44 24 60 68 ? ? ? ? 50 E8 ? ? ? ? 83 C4 08 85 C0 0F 85", "8B 0D ? ? ? ? 8B 54 24 1C 83 C4 0C");
                static auto reg = *pattern.get_first<uint8_t>(7);
                static auto dword_15A6F0C = *pattern.get_first<int32_t*>(2);
                struct Loadsc
                {
                    void operator()(injector::reg_pack& regs)
                    {
                        bool less = false;
                        *(int32_t*)&regs.ecx = *dword_15A6F0C;
                        if (reg == 0x54) {
                            *(int32_t*)&regs.edx = *(int32_t*)(regs.esp + 0x1C);
                            less = *(int32_t*)&regs.edx < 8000;
                        }
                        else {
                            *(int32_t*)&regs.eax = *(int32_t*)(regs.esp + 0x18);
                            less = *(int32_t*)&regs.eax < 8000;
                        }
                        if (!bSkipIntroNotNeeded && FusionFixSettings("PREF_SKIP_INTRO") && less)
                        {
                            if (reg == 0x54)
                                regs.edx = 0;
                            else
                                regs.eax = 0;
                        }
                    }
                }; injector::MakeInline<Loadsc>(pattern.get_first(0), pattern.get_first(10));

                #ifdef _DEBUG
                // don't load loadscreens at the start
                pattern = hook::pattern("E8 ? ? ? ? 83 C4 04 E8 ? ? ? ? 6A 00 E8 ? ? ? ? 83 C4 04 C7 06");
                if (!pattern.count_hint(1).empty()) {
                    CGame::hbshowLoadscreen.fun = injector::MakeCALL(pattern.get_first(), CGame::showLoadscreen, true).get();
                    injector::MakeNOP(pattern.get_first(5), 3, true); // nop add esp, 4 since it's not fastcall
                }
                
                // don't wait for loadscreens at the start
                pattern = hook::pattern("80 3D ? ? ? ? 00 B9 01 00 00 00 0F 45 C1 80 3D");
                if (!pattern.count_hint(1).empty())
                    injector::WriteMemory<uint8_t>(pattern.get_first(-23), 0xEB, true);
                #endif
            }

            //if (bSkipMenu)
            {
                auto pattern = hook::pattern("E8 ? ? ? ? 83 C4 04 8B 8C 24 ? ? ? ? 5F 5E 5D 5B 33 CC E8 ? ? ? ? 81 C4 ? ? ? ? C3");
                if (pattern.size() == 5)
                    hbsub_7870A0.fun = injector::MakeCALL(pattern.count(5).get(1).get<void*>(0), sub_7870A0).get();
                else
                {
                    hbsub_7870A0.fun = injector::MakeCALL(hook::get_pattern("E8 ? ? ? ? 83 C4 04 5F 5E 5B 8B 8C 24 ? ? ? ? 33 CC E8 ? ? ? ? 8B E5 5D C3 83 F8 30", 0), sub_7870A0).get();
                }
            }
        };
    }
} SkipIntro;