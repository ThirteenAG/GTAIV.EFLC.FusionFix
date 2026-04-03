module;

#include <common.hxx>

export module ikeeponwalking;

import common;
import comvars;
import settings;
import natives;

uint8_t* (__fastcall* sub_8F0080)(uint8_t* _this, void* edx) = nullptr;

namespace CTaskSimpleMovePlayer
{
    GameRef<bool> ms_bDefaultNoSprintingInInteriors;
}

bool bRunState = true;

class IKeepOnWalking
{
public:
    IKeepOnWalking()
    {
        FusionFix::onInitEventAsync() += []()
        {
            auto pattern = find_pattern("80 3D ? ? ? ? ? 74 ? F7 46 ? ? ? ? ? 74", "80 3D ? ? ? ? ? 74 ? F7 47 ? ? ? ? ? 74");
            CTaskSimpleMovePlayer::ms_bDefaultNoSprintingInInteriors.SetAddress(*pattern.get_first<bool*>(2));

            pattern = find_pattern("E8 ? ? ? ? 8A 48 ? 32 48 ? 80 F9 ? 76 ? 8B 86", "E8 ? ? ? ? 8A 48 ? 32 48 ? F3 0F 10 05");
            sub_8F0080 = (decltype(sub_8F0080))injector::GetBranchDestination(pattern.get_first()).as_int();

            pattern = hook::pattern("D9 44 24 18 5F 5B 5D");
            static auto flag = false;
            if (!pattern.empty())
                flag = true;
            else
                pattern = hook::pattern("D9 44 24 1C 5E 5B 5D");
            static uintptr_t loc_A2A60F = (uintptr_t)pattern.get_first(0);
            pattern = hook::pattern("80 F9 7F 76 57");
            struct SprintHook
            {
                void operator()(injector::reg_pack& regs)
                {
                    if ((*(uint8_t*)(regs.eax + 4) ^ *(uint8_t*)(regs.eax + 6)) <= 127)
                    {
                        if (*(float*)(regs.esp + (flag ? 0x14 : 0x1C)) > 1.0f)
                            *(float*)(regs.esp + (flag ? 0x18 : 0x1C)) = 1.0f;

                        force_return_address(loc_A2A60F);
                    }

                    static auto alwaysrunPref = FusionFixSettings.GetRef("PREF_ALWAYSRUN");
                    auto bShouldRun = alwaysrunPref->get();

                    if (bShouldRun && bRunState)
                        *(float*)(regs.esp + (flag ? 0x18 : 0x1C)) = 1.0f;
                }
            }; injector::MakeInline<SprintHook>(pattern.get_first(0));

            pattern = find_pattern("77 5F 8B 8E", "77 46 8B 8F");
            static raw_mem GamepadCB(pattern.get_first(0), { 0x90, 0x90 }); // NOP
            FusionFixSettings.SetCallback("PREF_ALWAYSRUN", [](int32_t value)
            {
                if (value)
                {
                    CTaskSimpleMovePlayer::ms_bDefaultNoSprintingInInteriors = false;
                    GamepadCB.Write();
                }
                else
                {
                    CTaskSimpleMovePlayer::ms_bDefaultNoSprintingInInteriors = true;
                    GamepadCB.Restore();
                }
            });

            if (FusionFixSettings("PREF_ALWAYSRUN"))
            {
                CTaskSimpleMovePlayer::ms_bDefaultNoSprintingInInteriors = false;
                GamepadCB.Write();
            }

            pattern = find_pattern("0F 2F C3 F3 0F 11 44 24 ? F3 0F 11 44 24", "0F 2F 05 ? ? ? ? F3 0F 11 4C 24 ? F3 0F 11 44 24 ? 0F 86");
            static auto SprintHook2 = safetyhook::create_mid(pattern.get_first(), [](SafetyHookContext& regs)
            {
                static auto alwaysrunPref = FusionFixSettings.GetRef("PREF_ALWAYSRUN");
                if (!alwaysrunPref->get())
                    return;

                auto ret = sub_8F0080((uint8_t*)regs.ebp, 0);

                uint8_t sprintCur = ret[6];
                uint8_t sprintPrev = ret[7];
                bool pressed = sprintCur > 127;
                bool wasPressed = sprintPrev > 127;

                static int32_t sprintPressStart = 0;
                static bool isHold = false;
                static constexpr int32_t kHoldThreshold = 200;

                if (pressed && !wasPressed)
                {
                    sprintPressStart = *CTimer::m_snTimeInMilliseconds;
                    isHold = false;
                }

                if (pressed)
                {
                    if (*CTimer::m_snTimeInMilliseconds - sprintPressStart >= kHoldThreshold)
                    {
                        // Hold — allow sprint, don't cap
                        isHold = true;
                    }
                    else
                    {
                        // Tap window — cap to prevent sprint early return
                        regs.xmm0.f32[0] = std::min(regs.xmm0.f32[0], 1.0f);
                    }
                }
                else if (wasPressed && !isHold)
                {
                    // Falling edge after tap — toggle
                    bRunState = !bRunState;
                }
            });
        };
    }
} IKeepOnWalking;