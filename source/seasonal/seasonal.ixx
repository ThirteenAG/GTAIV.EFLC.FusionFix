module;

#include <common.hxx>
#include <d3dx9tex.h>

export module seasonal;

import common;
import comvars;
import timecycext;
import settings;
import natives;

import seasonal.halloween;
import seasonal.snow;

export enum class SeasonalType : std::uint8_t {
    None = 0,
    Halloween = 1,
    Snow = 2,

    COUNT
};

export namespace SeasonalManager {
    auto Init() -> void;
    auto GetCurrent() -> const SeasonalType;
    auto Set(const SeasonalType type) -> void;
    auto DisableTimedEvents() -> void;
    auto IsTimedEventsDisabled() -> bool;
}

class None : public ISeasonal
{
public:
    static auto& Instance() { static None s; return static_cast<ISeasonal&>(s); }

    auto Init() -> void override {}
    auto Enable() -> void override {}
    auto Disable() -> void override {}
};

auto initialized = false;

auto isTimedEventDisabled = false;

auto currentEventType = SeasonalType::None;

std::function<void()> unsub_onGameProcessEvent;

const auto registry = std::array{
    &None::Instance(),
    &Halloween::Instance(),
    &Snow::Instance(),
};
static_assert(registry.size() == std::to_underlying(SeasonalType::COUNT));

auto Get(const SeasonalType type)
{
    return registry[std::to_underlying(type)];
}

auto onGameProcessEvent() -> void
{
    const auto now = std::chrono::system_clock::now();
    const auto now_c = std::chrono::system_clock::to_time_t(now);
    const auto date = std::localtime(&now_c);

    if ((date->tm_mon == 0 && date->tm_mday <= 2) || (date->tm_mon == 11 && date->tm_mday >= 30))
    {
        if (currentEventType != SeasonalType::Snow)
        {
            SeasonalManager::Set(SeasonalType::Snow);
            Natives::PrintHelp((char*)"WinterEvent1");
        }
    }
    else if (currentEventType == SeasonalType::Snow)
    {
        SeasonalManager::Set(SeasonalType::None);
        Natives::PrintHelp((char*)"WinterEvent0");
    }
    else if ((date->tm_mon == 10 && date->tm_mday <= 1) || (date->tm_mon == 9 && date->tm_mday >= 30))
    {
        if (currentEventType != SeasonalType::Halloween)
        {
            SeasonalManager::Set(SeasonalType::Halloween);
            Natives::PrintHelp((char*)"HalloweenEvent1");
        }
    }
    else if (currentEventType == SeasonalType::Halloween)
    {
        SeasonalManager::Set(SeasonalType::None);
        Natives::PrintHelp((char*)"HalloweenEvent0");
    }
}

auto SeasonalManager::GetCurrent() -> const SeasonalType
{
    return currentEventType;
}

auto SeasonalManager::Set(const SeasonalType type) -> void
{
    if (!initialized)
        return;

    auto current = Get(currentEventType);

    // Disable/Enable currently depends on new currentEventType
    currentEventType = type;

    current->Disable();
    Get(currentEventType)->Enable();
}

auto SeasonalManager::Init() -> void
{
    //if (!GetD3DX9_43DLL()) return;

    if (initialized)
        return;

    initialized = true;

    for (const auto& seasonalEvent : registry) {
        seasonalEvent->Init();
    }

    if (FusionFixSettings("PREF_TIMEDEVENTS"))
    {
        unsub_onGameProcessEvent = FusionFix::onGameProcessEvent() += onGameProcessEvent;
    }

    FusionFixSettings.SetCallback("PREF_TIMEDEVENTS", [](int32_t value)
    {
        if (value)
        {
            unsub_onGameProcessEvent = FusionFix::onGameProcessEvent() += onGameProcessEvent;
        }
        else
        {
            unsub_onGameProcessEvent();
            unsub_onGameProcessEvent = nullptr;

            if (GetCurrent() != SeasonalType::None)
            {
                Set(SeasonalType::None);
            }
        }
    });
}

auto SeasonalManager::DisableTimedEvents() -> void
{
    if (unsub_onGameProcessEvent)
        unsub_onGameProcessEvent();

    FusionFixSettings.RemoveCallback("PREF_TIMEDEVENTS");

    isTimedEventDisabled = true;
}

auto SeasonalManager::IsTimedEventsDisabled() -> bool
{
    return isTimedEventDisabled;
}

class Initializer
{
public:
    Initializer()
    {
        FusionFix::onInitEventAsync() += []()
        {
            SeasonalManager::Init();
        };
    }
} Initializer;

extern "C"
{
    bool __declspec(dllexport) IsSnowEnabled()
    {
        return currentEventType == SeasonalType::Snow;
    }

    bool __declspec(dllexport) IsHallEnabled()
    {
        return currentEventType == SeasonalType::Halloween;
    }

    bool __declspec(dllexport) IsWeatherSnow()
    {
        return true;
    }

    void __declspec(dllexport) ToggleSnow()
    {
        constexpr auto type = SeasonalType::Snow;
        SeasonalManager::Set(currentEventType == type ? SeasonalType::None : type);
    }
}