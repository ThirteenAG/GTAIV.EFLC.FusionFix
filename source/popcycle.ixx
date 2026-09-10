module;

#include <common.hxx>

export module popcycle;

import common;
import comvars;

static constexpr int32_t nCatchUpTime = 8000;
static constexpr int32_t nBudgetDivisor = 5; // extra headroom = budget / 5

static int32_t nBoostEndTime = 0;
static bool bBoosting = false;

static void BudgetHeadroom(SafetyHookContext& regs)
{
    if (!bBoosting)
        return;

    if (*CTimer::m_snTimeInMilliseconds - nBoostEndTime >= 0)
    {
        bBoosting = false;
        return;
    }

    regs.eax += regs.eax / nBudgetDivisor;
}

class PopCycle
{
public:
    PopCycle()
    {
        FusionFix::onInitEventAsync() += []()
        {
            CIniReader iniReader("");
            if (!iniReader.ReadInteger("MISC", "FasterPopCycleTransitions", 1))
                return;

            auto budgets = hook::pattern("A1 ? ? ? ? B9 ? ? ? ? 0F 45 C1 05 ? ? ? ? 39 86");
            auto changed = hook::pattern("3B 05 ? ? ? ? 75 ? 3B 2D ? ? ? ? 75 ? 3B 1D ? ? ? ? 74 ? B9 ? ? ? ? E8");

            if (budgets.size() != 2 || changed.empty() || !CTimer::m_snTimeInMilliseconds)
                return;

            static auto VehicleBudgetHook = safetyhook::create_mid(budgets.get(0).get<void>(13), BudgetHeadroom);
            static auto PedBudgetHook = safetyhook::create_mid(budgets.get(1).get<void>(13), BudgetHeadroom);

            static auto PopCycleChangedHook = safetyhook::create_mid(changed.get_first(29), [](SafetyHookContext&)
            {
                bBoosting = true;
                nBoostEndTime = *CTimer::m_snTimeInMilliseconds + nCatchUpTime;
            });
        };
    }
} PopCycle;
