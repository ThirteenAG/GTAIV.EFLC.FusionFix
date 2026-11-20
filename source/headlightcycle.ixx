module;

#include <common.hxx>
#include <unordered_map>

export module headlightcycle;

import common;
import comvars;
import natives;
import settings;

// Headlight cycle: keeps the game's automatic headlights and lets the game's headlight control cycle
//   Automatic -> High beam -> Low beam (forced on) -> Off -> Automatic
// The cycle follows the game's own headlight control, whatever triggers it (the H key, the controller, or a remapped
// key): the game toggles the high beam on every use of that control, so a change of the high beam that the cycle
// did not make is one step of the cycle. The game's controls are not read or replaced.
// FORCE_CAR_LIGHTS forces the headlights on/off but does not touch the high beam. The high beam is a flag in the
// vehicle (bit 0x02 of the byte at +0xF19 on 1.2.0.59, three bytes after the vehicle flags that the turn indicators
// use), so the cycle sets that bit itself every frame for the state it is in:
//   0 Automatic: FORCE_CAR_LIGHTS 0 (game default), high beam off
//   1 High beam: FORCE_CAR_LIGHTS 0, high beam on
//   2 Low beam : FORCE_CAR_LIGHTS 2 (on), high beam off
//   3 Off      : FORCE_CAR_LIGHTS 1 (off), high beam off
// Other game versions have the vehicle fields 0x50 bytes further, told apart by the same pattern as the turn indicators.
// The state is kept per vehicle, so getting back into a car resumes its cycle.

class HeadlightCycle
{
public:
    HeadlightCycle()
    {
        FusionFix::onInitEventAsync() += []()
        {
            SettingsReader reader;
            static bool bEnabled = reader.ReadInteger("HEADLIGHTS", "CycleHeadlights", 0) != 0;
            static bool bNotice = reader.ReadInteger("HEADLIGHTS", "ShowNotice", 1) != 0;
            if (!bEnabled)
                return;

            static ptrdiff_t highBeamOffset = find_pattern("55 8B EC 83 E4 ? 8B 45 ? 83 EC ? 8B 80").empty() ? 0xF19 + 0x50 : 0xF19;

            FusionFix::onGameProcessEvent() += []()
            {
                static std::unordered_map<Vehicle, int32_t> steps;
                static Vehicle lastCar = 0;
                static int32_t lastHighBeam = -1; // the high beam as the cycle last set it, -1 = not yet in this car

                Ped player = 0;
                Natives::GetPlayerChar(Natives::ConvertIntToPlayerIndex(Natives::GetPlayerId()), &player);
                Vehicle car = 0;
                if (player && Natives::IsCharInAnyCar(player))
                    Natives::GetCarCharIsUsing(player, &car);
                Ped driver = 0;
                if (car)
                    Natives::GetDriverOfCar(car, &driver);
                auto veh = (car && driver == player) ? (uint8_t*)FindPlayerVehicle(0) : nullptr;
                // Nothing while the player has no control (mission starts, cutscenes)
                if (veh && !Natives::IsPlayerControlOn(Natives::ConvertIntToPlayerIndex(Natives::GetPlayerId())))
                    veh = nullptr;
                if (!veh)
                {
                    lastCar = 0;
                    lastHighBeam = -1;
                    return;
                }
                if (car != lastCar)
                {
                    lastCar = car;
                    lastHighBeam = -1;
                }

                if (steps.size() > 32)
                    steps.clear();
                int32_t& step = steps[car];
                uint8_t& flags = veh[highBeamOffset];
                int32_t highBeam = (flags & 0x02) ? 1 : 0;

                // The game toggled the high beam: its headlight control was used
                if (lastHighBeam != -1 && highBeam != lastHighBeam)
                {
                    step = (step + 1) % 4;
                    static const int32_t force[4] = { 0, 0, 2, 1 };
                    Natives::ForceCarLights(car, force[step]);
                    if (bNotice)
                    {
                        static const char* names[4] = { "Headlights: automatic", "Headlights: high beam", "Headlights: low beam", "Headlights: off" };
                        Natives::PrintStringWithLiteralStringNow((char*)"STRING", (void*)names[step], 1500, true);
                    }
                }

                // The high beam follows the cycle
                lastHighBeam = (step == 1) ? 1 : 0;
                flags = lastHighBeam ? (flags | 0x02) : (flags & ~0x02);
            };
        };
    }
} HeadlightCycle;
