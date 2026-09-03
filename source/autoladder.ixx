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

static injector::hook_back<void*(__cdecl*)(void*, int32_t*, bool)> hbScanForLadderToClimb;

static void* __cdecl ScanForLadderToClimb(void* pPed, int32_t* pOutSection, bool bPlayer)
{
    if (*CTimer::m_snTimeInMilliseconds - nLastLadderExitTime < nRegrabCooldownMs)
        return nullptr;

    fLadderScanRadius = fAutoClimbRadius;
    auto pLadder = hbScanForLadderToClimb.fun(pPed, pOutSection, bPlayer);
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

            fAutoClimbRadius = iniReader.ReadFloat("MISC", "AutoClimbLaddersRange", 1.5f);

            auto pattern = find_pattern("76 ? 8A 81 ? ? ? ? 32 C2 3C ? 76 ? F7 87 ? ? ? ? ? ? ? ? 74");
            if (pattern.empty())
                return;

            static auto loc_A72375 = resolve_next_displacement(pattern.get_first(2)).value();
            static auto AlwaysScanForLadder = safetyhook::create_mid(pattern.get_first(), [](SafetyHookContext& regs)
            {
                static auto acl = FusionFixSettings.GetRef("PREF_AUTOCLIMBLADDERS");
                if (acl->get())
                    return_to(loc_A72375);
            });

            auto scanCall = find_pattern("6A 01 8D 44 24 2C 50 57 E8 ? ? ? ?");
            auto ladderExit = find_pattern("56 57 8B 7C 24 0C 57 8B F1 E8 ? ? ? ? 57 8B CE E8 ? ? ? ? 8B 8F 80 0A 00 00"); // CTaskSimpleClimbLadder::CleanUpBeforeExit
            auto scanRadius = find_pattern("F3 0F 10 05 ? ? ? ? F3 0F 51 CC");
            if (scanCall.empty() || ladderExit.empty() || scanRadius.empty() || !CTimer::m_snTimeInMilliseconds)
                return;

            hbScanForLadderToClimb.fun = injector::MakeCALL(scanCall.get_first(8), ScanForLadderToClimb, true).get();

            injector::WriteMemory<float*>(scanRadius.get_first(4), &fLadderScanRadius, true);

            static auto LadderExitHook = safetyhook::create_mid(ladderExit.get_first(), [](SafetyHookContext& regs)
            {
                nLastLadderExitTime = *CTimer::m_snTimeInMilliseconds;
            });
        };
    }
} AutoLadder;
