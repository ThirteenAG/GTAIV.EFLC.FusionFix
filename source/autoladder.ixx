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

namespace CTaskComplexClimbLadder
{
    static injector::hook_back<int(__cdecl*)(int, DWORD*, char)> hbScanForLadderToClimb;
    static int __cdecl ScanForLadderToClimb(int a1, DWORD* a2, char a3)
    {
        if (*CTimer::m_snTimeInMilliseconds - nLastLadderExitTime < nRegrabCooldownMs)
            return 0;

        fLadderScanRadius = fAutoClimbRadius;

        auto pLadder = hbScanForLadderToClimb.fun(a1, a2, a3);

        fLadderScanRadius = 4.0f;

        return pLadder;
    }
}

class AutoLadder
{
public:
    AutoLadder()
    {
        FusionFix::onInitEventAsync() += []()
        {
            CIniReader iniReader("");

            // [MISC]
            fAutoClimbRadius = iniReader.ReadFloat("MISC", "AutoClimbLaddersRange", 1.5f);

            auto pattern = find_pattern("76 ? 8A 81 ? ? ? ? 32 C2 3C ? 76 ? F7 87 ? ? ? ? ? ? ? ? 74", "76 ? 8A 96 ? ? ? ? 32 D0 80 FA ? 76 ? F7 83 ? ? ? ? ? ? ? ? 74 ? 8B 44 24");
            static auto loc_A72369 = resolve_next_displacement(pattern.get_first(2)).value();
            static auto CTaskComplexPlayerOnFoot__HandlePlayerInput_Hook = safetyhook::create_mid(pattern.get_first(0), [](SafetyHookContext& regs)
            {
                static auto AutoClimbLadders = FusionFixSettings.GetRef("PREF_AUTOCLIMBLADDERS");
                if (AutoClimbLadders->get())
                    return_to(loc_A72369);
            });

            pattern = find_pattern("E8 ? ? ? ? 83 C4 ? 89 44 24 ? F6 87", "E8 ? ? ? ? 83 C4 ? 89 44 24 ? EB ? 8B 74 24");
            CTaskComplexClimbLadder::hbScanForLadderToClimb.fun = injector::MakeCALL(pattern.get_first(0), CTaskComplexClimbLadder::ScanForLadderToClimb).get();

            pattern = find_pattern("F3 0F 10 05 ? ? ? ? F3 0F 51 CC", "F3 0F 10 25 ? ? ? ? 0F 28 C1 0F 28 D3");
            injector::WriteMemory<float*>(pattern.get_first(4), &fLadderScanRadius, true);

            pattern = find_pattern("56 57 8B 7C 24 ? 57 8B F1 E8 ? ? ? ? 57 8B CE E8 ? ? ? ? 8B 8F", "56 8B 74 24 ? 57 56 8B F9 E8 ? ? ? ? 56 8B CF E8 ? ? ? ? 8B 8E");
            static auto CTaskSimpleClimbLadder__CleanUpBeforeExit_Hook = safetyhook::create_mid(pattern.get_first(0), [](SafetyHookContext& regs)
            {
                nLastLadderExitTime = *CTimer::m_snTimeInMilliseconds;
            });
        };
    }
} AutoLadder;