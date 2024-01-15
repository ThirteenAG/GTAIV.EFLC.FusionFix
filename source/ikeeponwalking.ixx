module;

#include <common.hxx>

export module ikeeponwalking;

import common;
import settings;

class IKeepOnWalking
{
public:
    IKeepOnWalking()
    {
        FusionFix::onInitEvent() += []()
        {
            CIniReader iniReader("");
            static int32_t nWalkKey = iniReader.ReadInteger("MISC", "WalkKey", VK_MENU);
            auto pattern = hook::pattern("D9 44 24 18 5F 5B 5D");
            static auto flag = false;
            if (!pattern.empty())
                flag = true;
            else
                pattern = hook::pattern("D9 44 24 1C 5E 5B 5D");
            static uintptr_t loc_A2A60F = (uintptr_t)pattern.get_first(0);
            pattern = hook::pattern("80 F9 7F 76 57");
            struct SprintHook
            {
                void operator()(SafetyHookContext& regs)
                {
                    if ((*(uint8_t*)(regs.eax + 4) ^ *(uint8_t*)(regs.eax + 6)) <= 127)
                    {
                        if (*(float*)(regs.esp + (flag ? 0x14 : 0x1C)) > 1.0f)
                            *(float*)(regs.esp + (flag ? 0x18 : 0x1C)) = 1.0f;

                        *(uintptr_t*)(regs.esp - 4) = loc_A2A60F;
                    }

                    static auto alwaysrun = FusionFixSettings.GetRef("PREF_ALWAYSRUN");                    
                    if (!FusionFixSettings.Get("PREF_SPRINT")) // toggle
                    {
                        if (alwaysrun->get())
                        {
                            static auto bRunState = true;
                            static auto oldWalkKeyState = GetAsyncKeyState(nWalkKey);
                            auto curWalkKeyState = GetAsyncKeyState(nWalkKey);
                            if (curWalkKeyState != oldWalkKeyState)
                                bRunState = !bRunState;
                            oldWalkKeyState = curWalkKeyState;
                    
                            if (bRunState)
                                *(float*)(regs.esp + (flag ? 0x18 : 0x1C)) = 1.0f;
                        }
                    }
                    else if (alwaysrun->get() && !GetAsyncKeyState(nWalkKey)) // hold
                        *(float*)(regs.esp + (flag ? 0x18 : 0x1C)) = 1.0f;
                }
            }; injector::MakeInline2<SprintHook>(pattern.get_first(0));

            pattern = find_pattern("77 5F 8B 8E", "77 46 8B 8F");
            static raw_mem GamepadCB(pattern.get_first(0), { 0x90, 0x90 }); // NOP
            FusionFixSettings.SetCallback("PREF_ALWAYSRUN", [](int32_t value) {
                if (value)
                    GamepadCB.Write();
                else
                    GamepadCB.Restore();
            });
            if (FusionFixSettings("PREF_ALWAYSRUN"))
                GamepadCB.Write();
        };
    }
} IKeepOnWalking;