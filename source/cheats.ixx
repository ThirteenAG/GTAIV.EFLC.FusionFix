module;

#include <common.hxx>

export module cheats;

import common;
import comvars;
import natives;
import settings;
import seasonal;

constexpr size_t MAX_CHEAT_LENGTH = 32;
char CheatString[MAX_CHEAT_LENGTH] = { 0 };

auto SnowCheat(bool bPrintHelp = true) -> void
{
    if (!SeasonalManager::IsTimedEventsDisabled())
        SeasonalManager::DisableTimedEvents();

    if (SeasonalManager::GetCurrent() == SeasonalType::Snow)
    {
        SeasonalManager::Set(SeasonalType::None);

        if (bPrintHelp)
            Natives::PrintHelp((char*)"WinterEvent0");
    }
    else
    {
        SeasonalManager::Set(SeasonalType::Snow);

        if (bPrintHelp)
            Natives::PrintHelp((char*)"WinterEvent1");
    }
}

auto HallCheat(bool bPrintHelp = true) -> void
{
    if (!SeasonalManager::IsTimedEventsDisabled())
        SeasonalManager::DisableTimedEvents();

    if (SeasonalManager::GetCurrent() == SeasonalType::Halloween)
    {
        SeasonalManager::Set(SeasonalType::None);

        if (bPrintHelp)
            Natives::PrintHelp((char*)"HalloweenEvent0");
    }
    else
    {
        SeasonalManager::Set(SeasonalType::Halloween);

        if (bPrintHelp)
            Natives::PrintHelp((char*)"HalloweenEvent1");
    }
}

std::vector<std::pair<const char*, std::function<void()>>> ReversedCheats =
{
    { "wonstitel", []() { SnowCheat(); }}, // letitsnow
    { "ecafyracs", []() { HallCheat(); }}, // scaryface
};

char VirtualKeyToChar(int vkCode)
{
    if (vkCode >= 'A' && vkCode <= 'Z')
    {
        return static_cast<char>(vkCode + 32);
    }

    if (vkCode >= '0' && vkCode <= '9')
    {
        return static_cast<char>(vkCode);
    }

    switch (vkCode)
    {
        case VK_SPACE:
            return ' ';
        default:
            return '\0';
    }
}

#pragma pack(push, 8)
struct modelNameId
{
    const char* pszName = nullptr;
    int nModelId = 0;
    modelNameId* pPrev = nullptr;
};
#pragma pack(pop)

namespace ModelNameId
{
    modelNameId Laundromat;
    modelNameId PoliceStockade;
    modelNameId RomansTaxi;
    modelNameId Romero;
    modelNameId PoliceMaverick;
    modelNameId PolicePredator;
    modelNameId Packer2;
    modelNameId PrisonBus;
    modelNameId Regina;
    modelNameId Tampa;
    modelNameId Yankee2;
    modelNameId BatiCustom;
    modelNameId Caddy;
    modelNameId Hexer;
    modelNameId StretchE;
    modelNameId Slamvan;
    modelNameId PoliceBike;
    modelNameId PoliceBuffalo;
    modelNameId PoliceStinger;
    modelNameId Brickade;
    modelNameId Skylift;
    modelNameId Swift;

    modelNameId* (__fastcall* ModelNameId)(modelNameId* _this, void* edx, const char* pszModelName) = nullptr;

    SafetyHookInline shinitialize = {};
    void __fastcall initialize(void* CModelInfoStore, void* edx)
    {
        if (!Laundromat.pszName)
            ModelNameId(&Laundromat, 0, "burrito2");

        if (!PoliceStockade.pszName)
            ModelNameId(&PoliceStockade, 0, "pstockade");

        if (!RomansTaxi.pszName)
            ModelNameId(&RomansTaxi, 0, "rom");

        if (!Romero.pszName)
            ModelNameId(&Romero, 0, "romero");

        if (!PoliceMaverick.pszName)
            ModelNameId(&PoliceMaverick, 0, "polmav");

        if (!PolicePredator.pszName)
            ModelNameId(&PolicePredator, 0, "predator");

        if (!Packer2.pszName)
            ModelNameId(&Packer2, 0, "packer2");

        if (!PrisonBus.pszName)
            ModelNameId(&PrisonBus, 0, "pbus");

        if (!Regina.pszName)
            ModelNameId(&Regina, 0, "regina");

        if (!Tampa.pszName)
            ModelNameId(&Tampa, 0, "tampa");

        if (!Yankee2.pszName)
            ModelNameId(&Yankee2, 0, "yankee2");

        if (!BatiCustom.pszName)
            ModelNameId(&BatiCustom, 0, "bati2");

        if (!Caddy.pszName)
            ModelNameId(&Caddy, 0, "caddy");

        if (!Hexer.pszName)
            ModelNameId(&Hexer, 0, "hexer");

        if (!StretchE.pszName)
            ModelNameId(&StretchE, 0, "limo2");

        if (!Slamvan.pszName)
            ModelNameId(&Slamvan, 0, "slamvan");

        if (!PoliceBike.pszName)
            ModelNameId(&PoliceBike, 0, "policeb");

        if (!PoliceBuffalo.pszName)
            ModelNameId(&PoliceBuffalo, 0, "police3");

        if (!PoliceStinger.pszName)
            ModelNameId(&PoliceStinger, 0, "police4");

        if (!Brickade.pszName)
            ModelNameId(&Brickade, 0, "avan");

        if (!Skylift.pszName)
            ModelNameId(&Skylift, 0, "skylift");

        if (!Swift.pszName)
            ModelNameId(&Swift, 0, "swift");

        return shinitialize.unsafe_fastcall(CModelInfoStore, edx);
    }
}

void (__cdecl* SpawnCar)(int modelID) = nullptr;

class Cheats
{
public:
    Cheats()
    {
        FusionFix::onGameInitEvent() += []()
        {
            if (CText::hasViceCityStrings())
            {
                memset(CheatString, 0, sizeof(CheatString));

                FusionFix::onGameProcessEvent() += []()
                {
                    int keyIndex = 0;
                    while (keyIndex < 255)
                    {
                        int pressedKey;
                        if (keyIndex != 8 && Natives::GetAsciiJustPressed(keyIndex, &pressedKey))
                        {
                            memmove(CheatString + 1, CheatString, MAX_CHEAT_LENGTH - 2);
                            CheatString[0] = VirtualKeyToChar(keyIndex);
                            CheatString[MAX_CHEAT_LENGTH - 1] = '\0';
                        }

                        ++keyIndex;
                    }

                    if (CheatString[0] == '\0')
                        return;

                    for (const auto& it : ReversedCheats)
                    {
                        const char* revCheat = it.first;
                        size_t len = strlen(revCheat);
                        if (len > MAX_CHEAT_LENGTH)
                            continue;
                        if (strncmp(CheatString, revCheat, len) == 0)
                        {
                            it.second();
                            CheatString[0] = '\0';
                            break;
                        }
                    }
                };
            }
        };

        FusionFix::onInitEventAsync() += []()
        {
            CIniReader iniReader("");
            bool bSnowCheat = iniReader.ReadInteger("CHEATS", "SnowCheat", 0) != 0;
            bool bHalloweenCheat = iniReader.ReadInteger("CHEATS", "HalloweenCheat", 0) != 0;

            if (bSnowCheat)
                FusionFix::onGameInitEvent() += []() { SnowCheat(); };

            if (bHalloweenCheat)
                FusionFix::onGameInitEvent() += []() { HallCheat(); };

            NativeOverride::RegisterPhoneCheat("7665550100", []
            {
                SnowCheat();
            });

            NativeOverride::RegisterPhoneCheat("2665550100", []
            {
                HallCheat();
            });

            enum ePedComponent
            {
                PED_COMPONENT_HEAD,
                PED_COMPONENT_UPPER,
                PED_COMPONENT_LOWER,
                PED_COMPONENT_SUSE,
                PED_COMPONENT_HAND,
                PED_COMPONENT_FEET,
                PED_COMPONENT_JACKET,
                PED_COMPONENT_HAIR,
                PED_COMPONENT_SUS2,
                PED_COMPONENT_TEEF,
                PED_COMPONENT_FACE,
            };

            //IV Beta Gloves
            NativeOverride::RegisterPhoneCheat("4585550100", []
            {
                if (*_dwCurrentEpisode == 0)
                {
                    Ped PlayerPed = 0;
                    Natives::GetPlayerChar(Natives::ConvertIntToPlayerIndex(Natives::GetPlayerId()), &PlayerPed);
                    if (PlayerPed)
                    {
                        auto Gloves = Natives::GetCharDrawableVariation(PlayerPed, PED_COMPONENT_HAND);

                        if (!Gloves)
                        {
                            Natives::SetCharComponentVariation(PlayerPed, PED_COMPONENT_HAND, 1, 0);
                            Natives::PrintHelp((char*)"CHEAT1");
                        }
                        else
                        {
                            Natives::SetCharComponentVariation(PlayerPed, PED_COMPONENT_HAND, 0, 0);
                            Natives::PrintHelp((char*)"CHEAT2");
                        }
                    }
                }
            });

            //IV Beta Hair
            NativeOverride::RegisterPhoneCheat("2885550100", []
            {
                if (*_dwCurrentEpisode == 0)
                {
                    Ped PlayerPed = 0;
                    Natives::GetPlayerChar(Natives::ConvertIntToPlayerIndex(Natives::GetPlayerId()), &PlayerPed);
                    if (PlayerPed)
                    {
                        auto Hair = Natives::GetCharDrawableVariation(PlayerPed, PED_COMPONENT_HAIR);

                        if (!Hair)
                        {
                            Natives::SetCharComponentVariation(PlayerPed, PED_COMPONENT_HAIR, 1, 0);
                            Natives::PrintHelp((char*)"CHEAT1");
                        }
                        else
                        {
                            Natives::SetCharComponentVariation(PlayerPed, PED_COMPONENT_HAIR, 0, 0);
                            Natives::PrintHelp((char*)"CHEAT2");
                        }
                    }
                }
            });

            //TLAD Pants
            NativeOverride::RegisterPhoneCheat("7265550100", []
            {
                if (*_dwCurrentEpisode == 1)
                {
                    Ped PlayerPed = 0;
                    Natives::GetPlayerChar(Natives::ConvertIntToPlayerIndex(Natives::GetPlayerId()), &PlayerPed);
                    if (PlayerPed)
                    {
                        auto Pants = Natives::GetCharTextureVariation(PlayerPed, PED_COMPONENT_LOWER);

                        if (!Pants)
                        {
                            Natives::SetCharComponentVariation(PlayerPed, PED_COMPONENT_LOWER, 0, 1);
                            Natives::PrintHelp((char*)"CHEAT1");
                        }
                        else
                        {
                            Natives::SetCharComponentVariation(PlayerPed, PED_COMPONENT_LOWER, 0, 0);
                            Natives::PrintHelp((char*)"CHEAT2");
                        }
                    }
                }
            });

            auto pattern = find_pattern("8B 44 24 ? 89 01 C7 41 ? ? ? ? ? A1", "8B C1 8B 4C 24 ? 89 08 C7 40");
            ModelNameId::ModelNameId = (decltype(ModelNameId::ModelNameId))pattern.get_first();

            pattern = find_pattern("56 8B 35 ? ? ? ? 85 F6 74 ? EB 03 8D 49 ? 8D 46", "56 8B 35 ? ? ? ? 85 F6 74 ? EB 03 8D 49 ? 8D 46");
            ModelNameId::shinitialize = safetyhook::create_inline(pattern.get_first(), ModelNameId::initialize);

            pattern = find_pattern("55 8B EC 83 E4 F0 81 EC 58 01 00 00 56 57 8B 7D ? 85 FF", "55 8B EC 83 E4 F0 81 EC 84 01 00 00 53 56 57 8B 7D");
            SpawnCar = (decltype(SpawnCar))pattern.get_first();

            //Spawn Laundromat
            NativeOverride::RegisterPhoneCheat("1825551976", []
            {
                SpawnCar(ModelNameId::Laundromat.nModelId);
                Natives::PrintHelp((char*)"CHEAT1");
            });

            //Spawn Police Stockade
            NativeOverride::RegisterPhoneCheat("6725557240", []
            {
                SpawnCar(ModelNameId::PoliceStockade.nModelId);
                Natives::PrintHelp((char*)"CHEAT1");
            });

            //Spawn Roman's Taxi
            NativeOverride::RegisterPhoneCheat("2945552222", []
            {
                SpawnCar(ModelNameId::RomansTaxi.nModelId);
                Natives::PrintHelp((char*)"CHEAT1");
            });

            //Spawn Romero
            NativeOverride::RegisterPhoneCheat("3725558926", []
            {
                SpawnCar(ModelNameId::Romero.nModelId);
                Natives::PrintHelp((char*)"CHEAT1");
            });

            //Spawn Police Maverick
            NativeOverride::RegisterPhoneCheat("7765558200", []
            {
                SpawnCar(ModelNameId::PoliceMaverick.nModelId);
                Natives::PrintHelp((char*)"CHEAT1");
            });

            //Spawn Police Predator
            NativeOverride::RegisterPhoneCheat("5665553800", []
            {
                SpawnCar(ModelNameId::PolicePredator.nModelId);
                Natives::PrintHelp((char*)"CHEAT1");
            });

            //Spawn Packer2
            NativeOverride::RegisterPhoneCheat("5435550942", []
            {
                if (*_dwCurrentEpisode == 1)
                {
                    SpawnCar(ModelNameId::Packer2.nModelId);
                    Natives::PrintHelp((char*)"CHEAT1");
                }
            });

            //Spawn Prison Bus
            NativeOverride::RegisterPhoneCheat("2565551372", []
            {
                if (*_dwCurrentEpisode == 1)
                {
                    SpawnCar(ModelNameId::PrisonBus.nModelId);
                    Natives::PrintHelp((char*)"CHEAT1");
                }
            });

            //Spawn Regina
            NativeOverride::RegisterPhoneCheat("6255551374", []
            {
                if (*_dwCurrentEpisode == 1)
                {
                    SpawnCar(ModelNameId::Regina.nModelId);
                    Natives::PrintHelp((char*)"CHEAT1");
                }
            });

            //Spawn Tampa
            NativeOverride::RegisterPhoneCheat("4625554022", []
            {
                if (*_dwCurrentEpisode == 1)
                {
                    SpawnCar(ModelNameId::Tampa.nModelId);
                    Natives::PrintHelp((char*)"CHEAT1");
                }
            });

            //Spawn Yankee2
            NativeOverride::RegisterPhoneCheat("8265550184", []
            {
                if (*_dwCurrentEpisode == 1)
                {
                    SpawnCar(ModelNameId::Yankee2.nModelId);
                    Natives::PrintHelp((char*)"CHEAT1");
                }
            });

            //Spawn Bati Custom
            NativeOverride::RegisterPhoneCheat("2455550288", []
            {
                if (*_dwCurrentEpisode == 2)
                {
                    SpawnCar(ModelNameId::BatiCustom.nModelId);
                    Natives::PrintHelp((char*)"CHEAT1");
                }
            });

            //Spawn Caddy
            NativeOverride::RegisterPhoneCheat("9265550180", []
            {
                if (*_dwCurrentEpisode == 2)
                {
                    SpawnCar(ModelNameId::Caddy.nModelId);
                    Natives::PrintHelp((char*)"CHEAT1");
                }
            });

            //Spawn Hexer
            NativeOverride::RegisterPhoneCheat("2455550150", []
            {
                if (*_dwCurrentEpisode == 2)
                {
                    SpawnCar(ModelNameId::Hexer.nModelId);
                    Natives::PrintHelp((char*)"CHEAT1");
                }
            });

            //Spawn Stretch E
            NativeOverride::RegisterPhoneCheat("4965550450", []
            {
                if (*_dwCurrentEpisode == 2)
                {
                    SpawnCar(ModelNameId::StretchE.nModelId);
                    Natives::PrintHelp((char*)"CHEAT1");
                }
            });

            //Spawn Slamvan
            NativeOverride::RegisterPhoneCheat("8265550100", []
            {
                if (*_dwCurrentEpisode == 2)
                {
                    SpawnCar(ModelNameId::Slamvan.nModelId);
                    Natives::PrintHelp((char*)"CHEAT1");
                }
            });

            //Spawn Police Bike
            NativeOverride::RegisterPhoneCheat("6255556752", []
            {
                if (*_dwCurrentEpisode == 2)
                {
                    SpawnCar(ModelNameId::PoliceBike.nModelId);
                    Natives::PrintHelp((char*)"CHEAT1");
                }
            });

            //Spawn Police Buffalo
            NativeOverride::RegisterPhoneCheat("4725558624", []
            {
                if (*_dwCurrentEpisode == 2)
                {
                    SpawnCar(ModelNameId::PoliceBuffalo.nModelId);
                    Natives::PrintHelp((char*)"CHEAT1");
                }
            });

            //Spawn Police Stinger
            NativeOverride::RegisterPhoneCheat("2275557864", []
            {
                if (*_dwCurrentEpisode == 2)
                {
                    SpawnCar(ModelNameId::PoliceStinger.nModelId);
                    Natives::PrintHelp((char*)"CHEAT1");
                }
            });

            //Spawn Brickade
            NativeOverride::RegisterPhoneCheat("2725552826", []
            {
                if (*_dwCurrentEpisode == 2)
                {
                    SpawnCar(ModelNameId::Brickade.nModelId);
                    Natives::PrintHelp((char*)"CHEAT1");
                }
            });

            //Spawn Skylift
            NativeOverride::RegisterPhoneCheat("4675552492", []
            {
                if (*_dwCurrentEpisode == 2)
                {
                    SpawnCar(ModelNameId::Skylift.nModelId);
                    Natives::PrintHelp((char*)"CHEAT1");
                }
            });

            //Spawn Swift
            NativeOverride::RegisterPhoneCheat("9725550264", []
            {
                if (*_dwCurrentEpisode == 2)
                {
                    SpawnCar(ModelNameId::Swift.nModelId);
                    Natives::PrintHelp((char*)"CHEAT1");
                }
            });
        };
    }
} Cheats;
