module;

#include <common.hxx>
#include <algorithm>
#include <cmath>
#include <numbers>
#include <utility>

export module laddercam;

import common;
import comvars;
import settings;

namespace CCamFollowPed
{
    ptrdiff_t nHeading = 0;

    // A short turn of the camera heading, then held until released; the look input cancels it
    constexpr uint32_t nTurnTimeMs = 400;
    constexpr uint32_t nTurnRequestTimeoutMs = 500;
    constexpr uint32_t nTurnHoldTimeoutMs = 3000;
    float fTurnTarget = 0.0f, fTurnStart = 0.0f;
    uint32_t nTurnRequestTime = 0, nTurnStartTime = 0;
    bool bTurnRequested = false, bTurning = false, bTurnHeld = false;

    void ProcessTurn(uintptr_t camera, const float* lookLeftRight)
    {
        const uint32_t now = *CTimer::m_snTimeInMilliseconds;
        auto& heading = *reinterpret_cast<float*>(camera + nHeading);
        if (std::exchange(bTurnRequested, false) && now - nTurnRequestTime <= nTurnRequestTimeoutMs)
        {
            fTurnStart = heading;
            nTurnStartTime = now;
            bTurning = true;
        }
        if (!bTurning)
            return;
        if (*lookLeftRight != 0.0f)
        {
            bTurning = false;
            return;
        }
        const float t = std::min(static_cast<float>(now - nTurnStartTime) / nTurnTimeMs, 1.0f);
        heading = fTurnStart + std::remainder(fTurnTarget - fTurnStart, 2.0f * std::numbers::pi_v<float>) * t * t * (3.0f - 2.0f * t);
        bTurning = t < 1.0f || (bTurnHeld && now - nTurnStartTime < nTurnHoldTimeoutMs);
    }

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
        ProcessTurn(camera, lookLeftRight);
        return result;
    }
}

export namespace LadderCamera
{
    // Turns the player camera to a world heading (radians) and keeps it there until Release
    void TurnTo(float heading)
    {
        CCamFollowPed::fTurnTarget = heading;
        CCamFollowPed::nTurnRequestTime = *CTimer::m_snTimeInMilliseconds;
        CCamFollowPed::bTurnRequested = CCamFollowPed::bTurnHeld = true;
    }

    void Release()
    {
        CCamFollowPed::bTurnHeld = false;
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

            pattern = find_pattern("8D 86 ? ? ? ? 50 8D 86 ? ? ? ? 50 FF 74 24 18 E8", "8D 96 ? ? ? ? 52 81 C6 ? ? ? ? 56 50 8B CF E8");
            CCamFollowPed::nHeading = *pattern.get_first<int32_t>(2);
        };
    }
} LadderCam;
