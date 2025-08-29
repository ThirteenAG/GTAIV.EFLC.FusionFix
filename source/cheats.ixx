module;

#include <common.hxx>

export module cheats;

import common;
import comvars;
import natives;
import settings;
import snow;

constexpr size_t MAX_CHEAT_LENGTH = 32;
char CheatString[MAX_CHEAT_LENGTH] = { 0 };

std::vector<std::pair<const char*, std::function<void()>>> ReversedCheats =
{
    { "wonstitel", []() { SnowCheat(); }}, // letitsnow
    { "ecafyracs", []() { HallCheat(); }}, // scaryface
};

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

                FusionFix::onGameProcessEvent() += []() {
                    int keyIndex = 0;
                    while (keyIndex < 255)
                    {
                        int pressedKey;
                        if (keyIndex != 8 && Natives::GetAsciiJustPressed(keyIndex, &pressedKey))
                        {
                            memmove(CheatString + 1, CheatString, MAX_CHEAT_LENGTH - 2);
                            CheatString[0] = static_cast<char>(pressedKey);
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
                FusionFix::onGameInitEvent() += []() { SnowCheat(false); };

            if (bHalloweenCheat)
                FusionFix::onGameInitEvent() += []() { HallCheat(false); };

            NativeOverride::RegisterPhoneCheat("7665550100", [] {
                SnowCheat();
            });

            NativeOverride::RegisterPhoneCheat("2665550100", [] {
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

            NativeOverride::RegisterPhoneCheat("4585550100", []
            {
                if (*_dwCurrentEpisode == 0)
                {
                    Ped PlayerPed = 0;
                    Natives::GetPlayerChar(Natives::ConvertIntToPlayerindex(Natives::GetPlayerId()), &PlayerPed);
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

            NativeOverride::RegisterPhoneCheat("2885550100", []
            {
                if (*_dwCurrentEpisode == 0)
                {
                    Ped PlayerPed = 0;
                    Natives::GetPlayerChar(Natives::ConvertIntToPlayerindex(Natives::GetPlayerId()), &PlayerPed);
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
        };
    }
} Cheats;