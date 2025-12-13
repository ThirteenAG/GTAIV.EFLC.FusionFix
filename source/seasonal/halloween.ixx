module;

#include <common.hxx>

export module seasonal.halloween;

import comvars;
import timecycext;

export class Halloween : public ISeasonal
{
public:
    static auto& Instance() { static Halloween s; return static_cast<ISeasonal&>(s); }

    auto Init() -> void override {}

    auto Enable() -> void override
    {
        unsub_onCTimeCycleExtInit = CTimeCycleExt::onCTimeCycleExtInit() += onCTimeCycleExtInit;

        CTimeCycle::Initialise();
    }

    auto Disable() -> void override
    {
        unsub_onCTimeCycleExtInit();

        CTimeCycle::Initialise();
    }

private:
    static inline std::function<void()> unsub_onCTimeCycleExtInit;

    static auto onCTimeCycleExtInit() -> void
    {
        CTimeCycleExt::Initialise(currentEpisodePath() / "pc" / "data" / "halloweenext.dat");
    }
};