module;

#include <common.hxx>

export module autoladder;

import common;
import comvars;
import settings;

static constexpr int32_t nRegrabCooldownMs = 2500;

static float fLadderScanRadius = 4.0f;
static float fAutoClimbRadius = 1.5f;

static int32_t nLastLadderExitTime = -nRegrabCooldownMs;

static void* (__cdecl* orgScanForLadderToClimb)(void* pPed, int32_t* pOutSection, bool bPlayer) = nullptr;

static void* __cdecl ScanForLadderToClimb(void* pPed, int32_t* pOutSection, bool bPlayer)
{
    if (*CTimer::m_snTimeInMilliseconds - nLastLadderExitTime < nRegrabCooldownMs)
        return nullptr;

    fLadderScanRadius = fAutoClimbRadius;
    auto pLadder = orgScanForLadderToClimb(pPed, pOutSection, bPlayer);
    fLadderScanRadius = 4.0f;
    return pLadder;
}

class AutoLadder
{
public:
    AutoLadder()
    {
        FusionFix::onInitEventAsync() += []()
        {
            CIniReader iniReader("");
            if (!iniReader.ReadInteger("MISC", "AutoClimbLadders", 0))
                return;

            fAutoClimbRadius = iniReader.ReadFloat("MISC", "AutoClimbLaddersRange", 1.5f);

            auto pattern = find_pattern("8A 81 CE 26 00 00 32 C2 3C 7F 76 ? 8A 81 CF 26 00 00 32 C2 3C 7F 76 ? F7 87 70 02 00 00 00 00 02 80 74");
            if (pattern.empty())
                return;

            static raw_mem AlwaysScanForLadder(pattern.get_first(10), { 0xEB, 0x18 });

            auto scanCall = find_pattern("6A 01 8D 44 24 2C 50 57 E8 ? ? ? ?");
            auto ladderExit = find_pattern("56 57 8B 7C 24 0C 57 8B F1 E8 ? ? ? ? 57 8B CE E8 ? ? ? ? 8B 8F 80 0A 00 00"); // CTaskSimpleClimbLadder::CleanUpBeforeExit
            auto scanRadius = find_pattern("F3 0F 10 05 ? ? ? ? F3 0F 51 CC");
            if (scanCall.empty() || ladderExit.empty() || scanRadius.empty() || !CTimer::m_snTimeInMilliseconds)
                return;

            orgScanForLadderToClimb = (decltype(orgScanForLadderToClimb))injector::GetBranchDestination(scanCall.get_first(8)).as_int();
            injector::MakeCALL(scanCall.get_first(8), ScanForLadderToClimb, true);

            injector::WriteMemory<float*>(scanRadius.get_first(4), &fLadderScanRadius, true);

            static auto LadderExitHook = safetyhook::create_mid(ladderExit.get_first(), [](SafetyHookContext& regs)
            {
                nLastLadderExitTime = *CTimer::m_snTimeInMilliseconds;
            });

            AlwaysScanForLadder.Write();
        };
    }
} AutoLadder;
