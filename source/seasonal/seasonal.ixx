module;

#include <common.hxx>
#include <d3dx9tex.h>

export module seasonal;

import common;
import comvars;
import timecycext;
import settings;

import seasonal.halloween;
import seasonal.snow;

export enum class SeasonalType : std::uint8_t {
    None = 0,
    Halloween = 1,
    Snow = 2,

    COUNT
};

class None : public ISeasonal
{
public:
    static auto& Instance() { static None s; return static_cast<ISeasonal&>(s); }

    auto Init() -> void override {}
    auto Enable() -> void override {}
    auto Disable() -> void override {}
};

namespace SeasonalManager
{
    namespace 
    {
        auto initialized = false;

        auto registry = std::array{
            &None::Instance(),
            &Halloween::Instance(),
            &Snow::Instance(),
        };
        static_assert(registry.size() == std::to_underlying(SeasonalType::COUNT));

        auto Get(const SeasonalType type)
        {
            return registry[std::to_underlying(type)];
        }

        auto currentEventType = SeasonalType::None;

        auto Init() -> void
        {
            if (initialized)
                return;

            initialized = true;

            for (const auto& seasonalEvent : registry) {
                seasonalEvent->Init();
            }
        }
    }

    export auto GetCurrent() -> const SeasonalType
    {
        return currentEventType;
    }

    export auto Set(const SeasonalType type) -> void
    {
        if (!initialized)
            return;

        Get(currentEventType)->Disable();

        currentEventType = type;

        Get(currentEventType)->Enable();
    }
}

class Initializer
{
public:
    Initializer()
    {
        FusionFix::onInitEventAsync() += []()
        {
            SeasonalManager::Init();

            //if (!GetD3DX9_43DLL()) return;

            auto now = std::chrono::system_clock::now();
            auto now_c = std::chrono::system_clock::to_time_t(now);
            auto date = std::localtime(&now_c);

            if ((date->tm_mon == 0 && date->tm_mday <= 2) || (date->tm_mon == 11 && date->tm_mday >= 29))
            {
                FusionFix::onGameProcessEvent() += []()
                {
                    if (!FusionFixSettings("PREF_TIMEDEVENTS"))
                        return;

                    auto now = std::chrono::system_clock::now();
                    auto now_c = std::chrono::system_clock::to_time_t(now);
                    auto date = std::localtime(&now_c);

                    if ((date->tm_mon == 0 && date->tm_mday <= 2) || (date->tm_mon == 11 && date->tm_mday >= 30))
                    {
                        static bool bOnce = false;
                        if (bOnce) return;
                        bOnce = true;
                        
                        if (SeasonalManager::GetCurrent() != SeasonalType::Snow)
                            SeasonalManager::Set(SeasonalType::Snow);
                    }
                    else if (SeasonalManager::GetCurrent() == SeasonalType::Snow)
                    {
                        static bool bOnce = false;
                        if (bOnce) return;
                        bOnce = true;

                        SeasonalManager::Set(SeasonalType::None);
                    }
                };
            }
            else if (date->tm_mon == 9 && date->tm_mday == 31)
            {
                FusionFix::onGameProcessEvent() += []()
                {
                    if (!FusionFixSettings("PREF_TIMEDEVENTS"))
                        return;

                    auto now = std::chrono::system_clock::now();
                    auto now_c = std::chrono::system_clock::to_time_t(now);
                    auto date = std::localtime(&now_c);

                    if ((date->tm_mon == 10 && date->tm_mday <= 1) || (date->tm_mon == 9 && date->tm_mday >= 30))
                    {
                        static bool bOnce = false;
                        if (bOnce) return;
                        bOnce = true;

                        if (SeasonalManager::GetCurrent() != SeasonalType::Halloween)
                            SeasonalManager::Set(SeasonalType::Halloween);
                    }
                    else if (SeasonalManager::GetCurrent() == SeasonalType::Halloween)
                    {
                        static bool bOnce = false;
                        if (bOnce) return;
                        bOnce = true;

                        SeasonalManager::Set(SeasonalType::None);
                    }
                };
            }
        };
    }
} Initializer;

extern "C"
{
    bool __declspec(dllexport) IsSnowEnabled()
    {
        return SeasonalManager::GetCurrent() == SeasonalType::Snow;
    }

    bool __declspec(dllexport) IsHallEnabled()
    {
        return SeasonalManager::GetCurrent() == SeasonalType::Halloween;
    }

    bool __declspec(dllexport) IsWeatherSnow()
    {
        return true;
    }

    void __declspec(dllexport) ToggleSnow()
    {
        constexpr auto type = SeasonalType::Snow;
        SeasonalManager::Set(SeasonalManager::GetCurrent() == type ? SeasonalType::None : type);
    }
}
