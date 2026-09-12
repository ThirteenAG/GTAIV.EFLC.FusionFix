module;

#include <common.hxx>

export module ikeeponwalking;

import common;
import comvars;
import settings;
import natives;

uint8_t* (__fastcall* CControl__GetSprintValue)(uint8_t* _this, void* edx) = nullptr;

bool IsSprintDown(const uint8_t* ioValue)
{
    return static_cast<uint8_t>(ioValue[6] ^ ioValue[4]) > 0x7F;
}

bool WasSprintDown(const uint8_t* ioValue)
{
    return static_cast<uint8_t>(ioValue[7] ^ ioValue[4]) > 0x7F;
}

bool bRunState = true;

class IKeepOnWalking
{
public:
    IKeepOnWalking()
    {
        FusionFix::onInitEventAsync() += []()
        {
            auto pattern = find_pattern("E8 ? ? ? ? 8A 48 ? 32 48 ? 80 F9 ? 76 ? 8B 86", "E8 ? ? ? ? 8A 48 ? 32 48 ? F3 0F 10 05");
            CControl__GetSprintValue = (decltype(CControl__GetSprintValue))injector::GetBranchDestination(pattern.get_first()).as_int();

            pattern = hook::pattern("D9 44 24 18 5F 5B 5D");
            static uint32_t nRetValOffset = 0x18;
            if (pattern.empty())
            {
                pattern = hook::pattern("D9 44 24 1C 5E 5B 5D");
                nRetValOffset = 0x1C;
            }
            static uintptr_t loc_A2A60F = (uintptr_t)pattern.get_first(0);

            pattern = hook::pattern("80 F9 7F 76 57");
            struct SprintHook
            {
                void operator()(injector::reg_pack& regs)
                {
                    if (IsSprintDown((uint8_t*)regs.eax))
                    {
                        return; // Sprint held - fall through to the games own ramp
                    }

                    auto& retVal = *(float*)(regs.esp + nRetValOffset);
                    if (retVal > 1.0f)
                    {
                        retVal = 1.0f;
                    }

                    static auto sprintPref = FusionFixSettings.GetRef("PREF_SPRINT");
                    static auto alwaysrunPref = FusionFixSettings.GetRef("PREF_ALWAYSRUN");
                    if (sprintPref->get() && alwaysrunPref->get() && bRunState) // Hold-to-sprint only
                    {
                        retVal = 1.0f;
                    }

                    force_return_address(loc_A2A60F);
                }
            }; injector::MakeInline<SprintHook>(pattern.get_first(0));

            pattern = find_pattern("77 5F 8B 8E", "77 46 8B 8F");
            static raw_mem GamepadCB(pattern.get_first(0), { 0x90, 0x90 }); // NOP
            static auto ApplyAlwaysRun = [](int32_t value)
            {
                *CTaskSimpleMovePlayer::ms_bDefaultNoSprintingInInteriors = !value;
                if (value)
                {
                    GamepadCB.Write();
                }
                else
                {
                    GamepadCB.Restore();
                }
            };
            FusionFixSettings.SetCallback("PREF_ALWAYSRUN", ApplyAlwaysRun);
            ApplyAlwaysRun(FusionFixSettings("PREF_ALWAYSRUN"));

            pattern = find_pattern("0F 2F C3 F3 0F 11 44 24 ? F3 0F 11 44 24", "0F 2F 05 ? ? ? ? F3 0F 11 4C 24 ? F3 0F 11 44 24 ? 0F 86");
            static auto SprintHook2 = safetyhook::create_mid(pattern.get_first(), [](SafetyHookContext& regs)
            {
                static auto sprintPref = FusionFixSettings.GetRef("PREF_SPRINT");
                if (!sprintPref->get()) // Toggle-to-sprint - leave the game alone
                {
                    return;
                }

                static auto alwaysrunPref = FusionFixSettings.GetRef("PREF_ALWAYSRUN");
                if (!alwaysrunPref->get())
                {
                    return;
                }

                auto ioValue = CControl__GetSprintValue((uint8_t*)regs.ebp, 0);
                bool pressed = IsSprintDown(ioValue);
                bool wasPressed = WasSprintDown(ioValue);

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
                        // Hold - allow sprint, dont cap
                        isHold = true;
                    }
                    else
                    {
                        // Tap window - cap to prevent sprint early return
                        regs.xmm0.f32[0] = std::min(regs.xmm0.f32[0], 1.0f);
                    }
                }
                else if (wasPressed && !isHold)
                {
                    // Falling edge after tap - toggle
                    bRunState = !bRunState;
                }
            });
        };
    }
} IKeepOnWalking;