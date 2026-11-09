module;

#include <common.hxx>

export module laddercam;

import common;
import comvars;
import settings;

namespace CCamFollowPed
{
    // While the player climbs a ladder without looking up or down, the camera tilts down to follow; with the setting
    // off the look input stays as it was
    SafetyHookInline shProcessLadderPitch;
    uintptr_t __fastcall ProcessLadderPitch(uintptr_t camera, void* edx, uintptr_t ped, uintptr_t intelligence, float* lookUpDown, float* lookLeftRight)
    {
        static auto LadderCameraTilt = FusionFixSettings.GetRef("PREF_LADDERCAMTILT");
        const float input = *lookUpDown;
        const auto result = shProcessLadderPitch.unsafe_fastcall<uintptr_t>(camera, edx, ped, intelligence, lookUpDown, lookLeftRight);
        if (!LadderCameraTilt->get())
            *lookUpDown = input;
        return result;
    }
}

class LadderCam
{
public:
    LadderCam()
    {
        FusionFix::onInitEventAsync() += []()
        {
            auto pattern = find_pattern("E8 ? ? ? ? 8B 84 24 ? ? ? ? 85 C0 74 11 8D 4C 24 18", "E8 ? ? ? ? 83 7C 24 58 00 74 13 8D 4C 24 14");
            CCamFollowPed::shProcessLadderPitch = safetyhook::create_inline(injector::GetBranchDestination(pattern.get_first(0)).as_int(), CCamFollowPed::ProcessLadderPitch);
        };
    }
} LadderCam;
