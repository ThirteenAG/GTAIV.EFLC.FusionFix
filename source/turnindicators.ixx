module;

#include <common.hxx>
#include <deque>

export module turnindicators;

import common;
import comvars;
import natives;
import settings;

// Configuration
float STEER_THRESHOLD = 0.50f;    // Minimum steering to trigger blinkers
float ACTIVATION_DELAY = 400.0f;  // ms delay before activation
float BLINK_INTERVAL = 400.0f;    // ms blink interval (on/off cycle)

// Struct to hold all vehicle state data
struct VehicleState
{
    enum CrashState
    {
        NO_CRASH = 0,
        JUST_CRASHED_STILL_IN_CAR = 1,
        CRASHED_AND_EXITED = 2
    };

    CrashState crash_state = NO_CRASH;
    int current_blinker = 0;
    float turn_start_timer = 0.0f;
    float turn_stop_timer = 0.0f;
    int last_turn_direction = 0;
    bool is_turning = false;
    bool blinkers_active = false;
    bool should_stop_blinking = false;
    bool blink_state = false;
    float blink_timer = 0.0f;
    float final_blink_timer = 0.0f;
};

// Queue to store last 5 player vehicles with their states
static std::deque<std::pair<Vehicle, VehicleState>> recent_vehicles;
static const size_t MAX_RECENT_VEHICLES = 5;

// Helper function to find vehicle in queue
VehicleState* findVehicle(Vehicle veh)
{
    for (auto& pair : recent_vehicles)
    {
        if (pair.first == veh)
        {
            return &pair.second;
        }
    }
    return nullptr;
}

VehicleState* addOrUpdateVehicle(Vehicle veh, const VehicleState& state = VehicleState{})
{
    // Check if vehicle already exists
    for (auto it = recent_vehicles.begin(); it != recent_vehicles.end(); ++it)
    {
        if (it->first == veh)
        {
            // Move to front and keep existing state
            VehicleState existing_state = it->second;
            recent_vehicles.erase(it);
            recent_vehicles.push_front({ veh, existing_state });
            return &recent_vehicles.front().second;
        }
    }

    // Add new vehicle to front
    recent_vehicles.push_front({ veh, state });

    // Remove oldest if we exceed max size
    if (recent_vehicles.size() > MAX_RECENT_VEHICLES)
    {
        // Turn off any active lights on the vehicle being removed
        Vehicle old_veh = recent_vehicles.back().first;
        VehicleState& old_state = recent_vehicles.back().second;
        if (old_state.blinkers_active || old_state.current_blinker != 0)
        {
            Natives::SetVehIndicatorlights(old_veh, false);
        }
        if (old_state.crash_state != VehicleState::NO_CRASH)
        {
            Natives::SetVehHazardlights(old_veh, false);
        }
        recent_vehicles.pop_back();
    }

    return &recent_vehicles.front().second;
};

enum Lights
{
    LIGHT_FRONT_LEFT = 54,
    LIGHT_FRONT_RIGHT = 55,
    LIGHT_REAR_LEFT = 56,
    LIGHT_REAR_RIGHT = 57
};

SafetyHookInline sh_sub_A3FF30{};
void __fastcall sub_A3FF30(void* pVehicleStruct, void* edx, int light, float a3, int a4, int a5)
{
    // Find the current player vehicle in our queue
    for (auto& pair : recent_vehicles)
    {
        if (pair.first == CVehicle::GetVehiclePool()->GetIndex(pVehicleStruct))
        {
            int current_blinker = pair.second.current_blinker;
    
            if (current_blinker == 1) // Left
            {
                if (light == LIGHT_FRONT_RIGHT || light == LIGHT_REAR_RIGHT)
                    return;
            }
            else if (current_blinker == 2) // Right
            {
                if (light == LIGHT_FRONT_LEFT || light == LIGHT_REAR_LEFT)
                    return;
            }
            break;
        }
    }

    return sh_sub_A3FF30.unsafe_fastcall(pVehicleStruct, edx, light, a3, a4, a5);
}

class TurnIndicators
{
public:
    TurnIndicators()
    {
        FusionFix::onInitEventAsync() += []()
        {
            struct VehicleFlags
            {
                unsigned char takesLessDamage : 1;
                unsigned char hasBeenOwnedByPlayer : 1;
                unsigned char needsHotWired : 1;
                unsigned char : 5;
            };

            static ptrdiff_t SteerAngleOffset = 0x1088;
            static ptrdiff_t pDriverOffset = 0xF50;
            static ptrdiff_t VehicleFlagsOffset = 0xF16;
            auto pattern = find_pattern("55 8B EC 83 E4 ? 8B 45 ? 83 EC ? 8B 80");
            if (!pattern.empty())
            {
                sh_sub_A3FF30 = safetyhook::create_inline(pattern.get_first(0), sub_A3FF30);
                SteerAngleOffset = 0x1088;
                pDriverOffset = 0xF50;
                VehicleFlagsOffset = 0xF16;
            }
            else
            {
                pattern = find_pattern("55 8B EC 83 E4 ? 8B 45 ? 83 EC ? 53 8B 5D ? 56 8B F1 8B 88");
                sh_sub_A3FF30 = safetyhook::create_inline(pattern.get_first(0), sub_A3FF30);
                SteerAngleOffset = 0x10D8;
                pDriverOffset = 0xFA0;
                VehicleFlagsOffset = 0xF66;
            }

            static uintptr_t prev_player_car = 0;
            pattern = find_pattern("F3 0F 11 82 ? ? ? ? 8A 44 24", "F3 0F 11 81 ? ? ? ? 8A 54 24");
            if (!pattern.empty())
            {
                injector::MakeNOP(pattern.get_first(), 8, true);
                static auto WheelResetHook1 = safetyhook::create_mid(pattern.get_first(), [](SafetyHookContext& regs)
                {
                    uintptr_t& pEntity = (SteerAngleOffset == 0x1088) ? regs.edx : regs.ecx;
                    static auto ti = FusionFixSettings.GetRef("PREF_TURNINDICATORS");
                    if (ti->get())
                    {
                        auto Driver = *(uintptr_t*)(pEntity + pDriverOffset);
                        if (!Driver || Driver == CPlayer::getLocalPlayerPed())
                        {
                            VehicleFlags* flags = reinterpret_cast<VehicleFlags*>(pEntity + VehicleFlagsOffset);
                            if (flags->hasBeenOwnedByPlayer)
                            {
                                prev_player_car = pEntity;
                                return;
                            }
                        }
                    }

                    *(float*)(pEntity + SteerAngleOffset) = regs.xmm0.f32[0];
                });
            }

            pattern = hook::pattern("C7 86 ? ? ? ? ? ? ? ? C7 86 ? ? ? ? ? ? ? ? E8 ? ? ? ? 84 C0 75 ? C7 86 ? ? ? ? ? ? ? ? F6 86");
            if (!pattern.empty())
                injector::MakeNOP(pattern.get_first(), 10, true);
            else
            {
                pattern = hook::pattern("F3 0F 11 86 ? ? ? ? F3 0F 11 86 ? ? ? ? E8 ? ? ? ? 84 C0 75 ? C7 86");
                injector::MakeNOP(pattern.get_first(), 8, true);
            }

            if (!pattern.empty())
            {
                static auto WheelResetHook2 = safetyhook::create_mid(pattern.get_first(), [](SafetyHookContext& regs)
                {
                    uintptr_t& pEntity = regs.esi;
                    static auto ti = FusionFixSettings.GetRef("PREF_TURNINDICATORS");
                    if (ti->get() && prev_player_car == pEntity)
                    {
                        auto Driver = *(uintptr_t*)(pEntity + pDriverOffset);
                        if (!Driver || Driver == CPlayer::getLocalPlayerPed())
                        {
                            VehicleFlags* flags = reinterpret_cast<VehicleFlags*>(pEntity + VehicleFlagsOffset);
                            if (flags->hasBeenOwnedByPlayer)
                                return;
                        }
                    }

                    *(float*)(pEntity + SteerAngleOffset) = 0.0f;
                });
            }

            pattern = hook::pattern("C7 81 ? ? ? ? ? ? ? ? C7 81 ? ? ? ? ? ? ? ? C7 81 ? ? ? ? ? ? ? ? C7 81 ? ? ? ? ? ? ? ? A1");
            if (!pattern.empty())
            {
                injector::MakeNOP(pattern.get_first(), 10, true);
            }
            else
            {
                pattern = hook::pattern("F3 0F 11 86 ? ? ? ? F3 0F 11 86 ? ? ? ? F3 0F 10 05 ? ? ? ? 33 C0");
                injector::MakeNOP(pattern.get_first(), 8, true);
            }

            if (!pattern.empty())
            {
                static auto WheelResetHook3 = safetyhook::create_mid(pattern.get_first(), [](SafetyHookContext& regs)
                {
                    uintptr_t& pEntity = (SteerAngleOffset == 0x1088) ? regs.ecx : regs.esi;
                    static auto ti = FusionFixSettings.GetRef("PREF_TURNINDICATORS");
                    if (ti->get())
                    {
                        auto Driver = *(uintptr_t*)(pEntity + pDriverOffset);
                        if (!Driver || Driver == CPlayer::getLocalPlayerPed())
                        {
                            VehicleFlags* flags = reinterpret_cast<VehicleFlags*>(pEntity + VehicleFlagsOffset);
                            if (flags->hasBeenOwnedByPlayer)
                            {
                                prev_player_car = pEntity;
                                return;
                            }
                        }
                    }

                    *(float*)(pEntity + SteerAngleOffset) = 0.0f;
                });
            }

            // CBike
            pattern = hook::pattern("C7 86 ? ? ? ? ? ? ? ? C7 86 ? ? ? ? ? ? ? ? FF 90");
            if (!pattern.empty())
            {
                injector::MakeNOP(pattern.get_first(), 10, true);
            }
            else
            {
                pattern = hook::pattern("F3 0F 11 86 ? ? ? ? F3 0F 11 86 ? ? ? ? FF D2 F3 0F 10 10");
                injector::MakeNOP(pattern.get_first(), 8, true);
            }

            if (!pattern.empty())
            {
                static auto WheelResetHook4 = safetyhook::create_mid(pattern.get_first(), [](SafetyHookContext& regs)
                {
                    uintptr_t& pEntity = regs.esi;
                    static auto ti = FusionFixSettings.GetRef("PREF_TURNINDICATORS");
                    if (ti->get())
                    {
                        auto Driver = *(uintptr_t*)(pEntity + pDriverOffset);
                        if (!Driver || Driver == CPlayer::getLocalPlayerPed())
                        {
                            VehicleFlags* flags = reinterpret_cast<VehicleFlags*>(pEntity + VehicleFlagsOffset);
                            if (flags->hasBeenOwnedByPlayer)
                            {
                                return;
                            }
                        }
                    }

                    *(float*)(pEntity + SteerAngleOffset) = 0.0f;
                });
            }

            pattern = find_pattern("F3 0F 11 86 ? ? ? ? 24 ? C7 86", "F3 0F 11 86 ? ? ? ? EB ? 0F 57 C9");
            if (!pattern.empty())
            {
                injector::MakeNOP(pattern.get_first(), 8, true);
                static auto WheelResetHook5 = safetyhook::create_mid(pattern.get_first(), [](SafetyHookContext& regs)
                {
                    uintptr_t& pEntity = regs.esi;
                    static auto ti = FusionFixSettings.GetRef("PREF_TURNINDICATORS");
                    if (ti->get())
                    {
                        auto Driver = *(uintptr_t*)(pEntity + pDriverOffset);
                        if (!Driver || Driver == CPlayer::getLocalPlayerPed())
                        {
                            VehicleFlags* flags = reinterpret_cast<VehicleFlags*>(pEntity + VehicleFlagsOffset);
                            if (flags->hasBeenOwnedByPlayer)
                            {
                                return;
                            }
                        }
                    }

                    *(float*)(pEntity + SteerAngleOffset) = regs.xmm0.f32[0];
                });
            }

            pattern = find_pattern("F3 0F 11 82 ? ? ? ? 8A 44 24", "F3 0F 11 86 ? ? ? ? F3 0F 11 86 ? ? ? ? F3 0F 10 05 ? ? ? ? 33 C0");
            if (!pattern.empty())
            {
                injector::MakeNOP(pattern.get_first(), 8, true);
                static auto WheelResetHook6 = safetyhook::create_mid(pattern.get_first(), [](SafetyHookContext& regs)
                {
                    uintptr_t& pEntity = (SteerAngleOffset == 0x1088) ? regs.edx : regs.esi;
                    static auto ti = FusionFixSettings.GetRef("PREF_TURNINDICATORS");
                    if (ti->get())
                    {
                        auto Driver = *(uintptr_t*)(pEntity + pDriverOffset);
                        if (!Driver || Driver == CPlayer::getLocalPlayerPed())
                        {
                            VehicleFlags* flags = reinterpret_cast<VehicleFlags*>(pEntity + VehicleFlagsOffset);
                            if (flags->hasBeenOwnedByPlayer)
                            {
                                return;
                            }
                        }
                    }

                    *(float*)(pEntity + SteerAngleOffset) = regs.xmm0.f32[0];
                });
            }

            pattern = find_pattern("E8 ? ? ? ? 84 C0 0F 84 ? ? ? ? 8B 07 8B CF 8B 80 ? ? ? ? FF D0 D9 5C 24", "E8 ? ? ? ? 84 C0 0F 84 ? ? ? ? 8B 16 8B 82 ? ? ? ? 8B CE FF D0 D8 25");
            static auto FlyThroughWindscreenHook = safetyhook::create_mid(pattern.get_first(), [](SafetyHookContext& regs)
            {
                static auto ti = FusionFixSettings.GetRef("PREF_TURNINDICATORS");
                if (ti->get())
                {
                    Ped CrashedPed = CPed::GetPedPool()->GetIndex((void*)regs.ecx);
                    if (CrashedPed)
                    {
                        Vehicle Car = 0;
                        Natives::GetCarCharIsUsing(CrashedPed, &Car);
                        if (Car)
                        {
                            VehicleState* state = addOrUpdateVehicle(Car);
                            Ped PlayerPed = 0;
                            Natives::GetPlayerChar(Natives::ConvertIntToPlayerIndex(Natives::GetPlayerId()), &PlayerPed);

                            if (CrashedPed == PlayerPed)
                            {
                                // Player crash - use the normal state that requires exit/enter cycle
                                state->crash_state = VehicleState::JUST_CRASHED_STILL_IN_CAR;
                            }
                            else
                            {
                                // NPC crash - immediately set to "exited" state so player can clear it
                                state->crash_state = VehicleState::CRASHED_AND_EXITED;
                            }

                            state->current_blinker = 0;
                            Natives::SetVehHazardlights(Car, true);
                        }
                    }
                }
            });

            FusionFix::onGameProcessEvent() += [&]() {
                static auto ti = FusionFixSettings.GetRef("PREF_TURNINDICATORS");
                static Vehicle last_player_car = 0;

                // Get current player vehicle
                Ped PlayerPed = 0;
                Vehicle PlayerCar = 0;
                Natives::GetPlayerChar(Natives::ConvertIntToPlayerIndex(Natives::GetPlayerId()), &PlayerPed);
                if (PlayerPed)
                {
                    Natives::GetCarCharIsUsing(PlayerPed, &PlayerCar);
                }

                // Check if player has fully entered the vehicle (can actually drive)
                auto pVehicleStruct = FindPlayerVehicle(0);
                bool player_fully_in_vehicle = (PlayerCar != 0 && pVehicleStruct != nullptr);

                // If player is in a vehicle, add/update it in our queue
                if (PlayerCar != 0)
                {
                    addOrUpdateVehicle(PlayerCar);
                }

                // Handle crash state transitions when player exits/enters vehicles
                if (PlayerCar != last_player_car)
                {
                    // Player changed vehicles or exited
                    if (last_player_car != 0)
                    {
                        // Player exited a vehicle - check if it was crashed
                        VehicleState* last_state = findVehicle(last_player_car);
                        if (last_state && last_state->crash_state == VehicleState::JUST_CRASHED_STILL_IN_CAR)
                        {
                            last_state->crash_state = VehicleState::CRASHED_AND_EXITED;
                        }
                    }

                    last_player_car = PlayerCar;
                }

                // Handle hazard light turn-off when player fully enters a previously crashed vehicle
                if (player_fully_in_vehicle)
                {
                    VehicleState* current_state = findVehicle(PlayerCar);
                    if (current_state && current_state->crash_state == VehicleState::CRASHED_AND_EXITED)
                    {
                        current_state->crash_state = VehicleState::NO_CRASH;
                        Natives::SetVehHazardlights(PlayerCar, false);
                    }
                }

                // Process all vehicles in our queue
                for (auto& pair : recent_vehicles)
                {
                    Vehicle veh = pair.first;
                    VehicleState& state = pair.second;

                    bool is_player_vehicle = (veh == PlayerCar);
                    bool player_in_vehicle = is_player_vehicle && player_fully_in_vehicle;

                    // Skip processing if feature is disabled and no active blinkers
                    if (!ti->get() && state.current_blinker == 0)
                    {
                        continue;
                    }

                    // Get vehicle structure (we already have it for player vehicle)
                    auto vehicleStruct = (is_player_vehicle) ? pVehicleStruct : nullptr;

                    if (player_in_vehicle && vehicleStruct)
                    {
                        // Process player vehicle with turn indicators
                        auto m_fSteerAngle = *reinterpret_cast<float*>(reinterpret_cast<uintptr_t>(vehicleStruct) + SteerAngleOffset);

                        // Determine current turn direction
                        int current_turn_direction = 0;
                        if (m_fSteerAngle > STEER_THRESHOLD)
                            current_turn_direction = 1; // Left
                        else if (m_fSteerAngle < -STEER_THRESHOLD)
                            current_turn_direction = 2; // Right

                        bool currently_turning = (current_turn_direction != 0);
                        bool direction_changed = (current_turn_direction != state.last_turn_direction &&
                            current_turn_direction != 0);

                        // Handle direction change
                        if (direction_changed)
                        {
                            if (state.blinkers_active)
                            {
                                Natives::SetVehIndicatorlights(veh, false);
                                state.blinkers_active = false;
                                state.current_blinker = 0;
                                state.should_stop_blinking = false;
                                state.blink_state = false;
                                state.blink_timer = 0.0f;
                            }

                            state.turn_start_timer = 0.0f;
                            state.turn_stop_timer = 0.0f;
                            state.is_turning = true;
                        }
                        else if (currently_turning && !state.is_turning)
                        {
                            state.turn_start_timer = 0.0f;
                            state.turn_stop_timer = 0.0f;
                            state.is_turning = true;
                        }
                        else if (!currently_turning && state.is_turning)
                        {
                            state.turn_stop_timer = 0.0f;
                            state.is_turning = false;
                            if (state.blinkers_active)
                            {
                                state.should_stop_blinking = true;
                                state.final_blink_timer = 0.0f;
                            }
                        }

                        // Update timers
                        if (state.is_turning && current_turn_direction != 0 && !state.should_stop_blinking)
                        {
                            state.turn_start_timer += 1000.0f * Natives::Timestep();

                            if (state.turn_start_timer >= ACTIVATION_DELAY && !state.blinkers_active)
                            {
                                state.current_blinker = current_turn_direction;
                                state.blinkers_active = true;
                                state.blink_timer = 0.0f;
                                state.blink_state = true;
                                Natives::SetVehIndicatorlights(veh, true);
                            }
                        }

                        // Handle blinking logic
                        if (state.blinkers_active && !state.should_stop_blinking)
                        {
                            state.blink_timer += 1000.0f * Natives::Timestep();

                            if (state.blink_timer >= BLINK_INTERVAL)
                            {
                                state.blink_state = !state.blink_state;
                                state.blink_timer = 0.0f;

                                if (state.blink_state)
                                {
                                    Natives::SetVehIndicatorlights(veh, true);
                                }
                                else
                                {
                                    Natives::SetVehIndicatorlights(veh, false);
                                }
                            }
                        }
                        else if (state.should_stop_blinking && state.blinkers_active)
                        {
                            state.final_blink_timer += 1000.0f * Natives::Timestep();

                            if (state.blink_state)
                            {
                                state.blink_timer += 1000.0f * Natives::Timestep();

                                if (state.blink_timer >= BLINK_INTERVAL)
                                {
                                    Natives::SetVehIndicatorlights(veh, false);

                                    state.blinkers_active = false;
                                    state.current_blinker = 0;
                                    state.should_stop_blinking = false;
                                    state.blink_state = false;
                                    state.turn_start_timer = 0.0f;
                                    state.turn_stop_timer = 0.0f;
                                    state.blink_timer = 0.0f;
                                }
                            }
                            else
                            {
                                state.blinkers_active = false;
                                state.current_blinker = 0;
                                state.should_stop_blinking = false;
                                state.turn_start_timer = 0.0f;
                                state.turn_stop_timer = 0.0f;
                                state.blink_timer = 0.0f;
                            }

                            // Safety timeout
                            if (state.final_blink_timer >= BLINK_INTERVAL * 2)
                            {
                                Natives::SetVehIndicatorlights(veh, false);

                                state.blinkers_active = false;
                                state.current_blinker = 0;
                                state.should_stop_blinking = false;
                                state.blink_state = false;
                                state.turn_start_timer = 0.0f;
                                state.turn_stop_timer = 0.0f;
                                state.blink_timer = 0.0f;
                            }
                        }

                        state.last_turn_direction = current_turn_direction;
                    }
                    else
                    {
                        // Vehicle is not occupied by player - continue processing blinkers
                        if (state.current_blinker != 0)
                        {
                            // Continue blinking process
                            if (state.blinkers_active && !state.should_stop_blinking)
                            {
                                state.blink_timer += 1000.0f * Natives::Timestep();

                                if (state.blink_timer >= BLINK_INTERVAL)
                                {
                                    state.blink_state = !state.blink_state;
                                    state.blink_timer = 0.0f;

                                    if (state.blink_state)
                                    {
                                        Natives::SetVehIndicatorlights(veh, true);
                                    }
                                    else
                                    {
                                        Natives::SetVehIndicatorlights(veh, false);
                                    }
                                }
                            }
                            else if (state.should_stop_blinking && state.blinkers_active)
                            {
                                state.final_blink_timer += 1000.0f * Natives::Timestep();

                                if (state.blink_state)
                                {
                                    state.blink_timer += 1000.0f * Natives::Timestep();

                                    if (state.blink_timer >= BLINK_INTERVAL)
                                    {
                                        Natives::SetVehIndicatorlights(veh, false);

                                        state.current_blinker = 0;
                                        state.blinkers_active = false;
                                        state.should_stop_blinking = false;
                                        state.blink_state = false;
                                    }
                                }
                                else
                                {
                                    state.current_blinker = 0;
                                    state.blinkers_active = false;
                                    state.should_stop_blinking = false;
                                }

                                // Safety timeout
                                if (state.final_blink_timer >= BLINK_INTERVAL * 2)
                                {
                                    Natives::SetVehIndicatorlights(veh, false);

                                    state.current_blinker = 0;
                                    state.blinkers_active = false;
                                    state.should_stop_blinking = false;
                                    state.blink_state = false;
                                }
                            }
                        }
                    }
                }
            };
        };
    }
} TurnIndicators;