module;

#include <common.hxx>

export module turnindicators;

import common;
import comvars;
import natives;
import settings;

// Configuration
constexpr float STEER_THRESHOLD = 0.15f;    // Minimum steering to trigger blinkers
constexpr float ACTIVATION_DELAY = 600.0f;  // 600ms delay before activation
constexpr float TURN_OFF_DELAY = 200.0f;    // 200ms delay before turning off after steering stops
constexpr float BLINK_INTERVAL = 400.0f;    // 500ms blink interval (on/off cycle)

// Control variables
static float turn_start_timer = 0.0f;
static float turn_stop_timer = 0.0f;
static int current_blinker = 0; // 0 = off, 1 = left, 2 = right
static int last_turn_direction = 0; // Track last turn direction
static bool is_turning = false;
static bool blinkers_active = false;

// Blinking logic variables
static float blink_timer = 0.0f;
static bool blink_state = false; // true = lights on, false = lights off
static bool should_stop_blinking = false;
static float final_blink_timer = 0.0f;

enum Lights
{
    LIGHT_FRONT_LEFT = 54,
    LIGHT_FRONT_RIGHT = 55,
    LIGHT_REAR_LEFT = 56,
    LIGHT_REAR_RIGHT = 57
};

SafetyHookInline sh_sub_A3FF30{};
void __fastcall sub_A3FF30(void* _this, void* edx, int light, float a3, int a4, int a5)
{
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

    return sh_sub_A3FF30.unsafe_fastcall(_this, edx, light, a3, a4, a5);
}

class TurnIndicators
{
public:
    TurnIndicators()
    {
        FusionFix::onInitEventAsync() += []()
        {
            auto pattern = hook::pattern("55 8B EC 83 E4 ? 8B 45 ? 83 EC ? 8B 80");
            if (!pattern.empty())
            {
                sh_sub_A3FF30 = safetyhook::create_inline(pattern.get_first(0), sub_A3FF30);

                FusionFix::onGameProcessEvent() += []()
                {
                    static auto ti = FusionFixSettings.GetRef("PREF_TURNINDICATORS");
                    if (!ti->get() && current_blinker == 0)
                        return;

                    Ped PlayerPed = 0;
                    Vehicle PlayerCar = 0;
                    Natives::GetPlayerChar(Natives::ConvertIntToPlayerindex(Natives::GetPlayerId()), &PlayerPed);
                    if (PlayerPed)
                    {
                        Natives::GetCarCharIsUsing(PlayerPed, &PlayerCar);
                        if (PlayerCar)
                        {
                            auto pVehicleStruct = FindPlayerVehicle(0);
                            if (pVehicleStruct)
                            {
                                auto m_fSteerAngle = *reinterpret_cast<float*>(reinterpret_cast<uintptr_t>(pVehicleStruct) + 0x1088);

                                // Determine current turn direction
                                int current_turn_direction = 0;
                                if (m_fSteerAngle > STEER_THRESHOLD)
                                    current_turn_direction = 1; // Left (positive values = left turn)
                                else if (m_fSteerAngle < -STEER_THRESHOLD)
                                    current_turn_direction = 2; // Right (negative values = right turn)
                                // else = 0 (straight)

                                bool currently_turning = (current_turn_direction != 0);
                                bool direction_changed = (current_turn_direction != last_turn_direction &&
                                    current_turn_direction != 0); // Any change TO a direction

                                // Handle direction change (including from straight to turning)
                                if (direction_changed)
                                {
                                    // Turn off current blinkers immediately when direction changes
                                    if (blinkers_active)
                                    {
                                        Natives::SetVehIndicatorlights(PlayerCar, false);
                                        blinkers_active = false;
                                        current_blinker = 0;
                                        should_stop_blinking = false;
                                        blink_state = false;
                                        blink_timer = 0.0f;
                                    }

                                    // Start new turn timer for new direction
                                    turn_start_timer = 0.0f;
                                    turn_stop_timer = 0.0f;
                                    is_turning = true;
                                }
                                // Handle turn start (from straight to turning - now handled above)
                                else if (currently_turning && !is_turning)
                                {
                                    // This case is now covered by direction_changed
                                    turn_start_timer = 0.0f;
                                    turn_stop_timer = 0.0f;
                                    is_turning = true;
                                }
                                // Handle turn end
                                else if (!currently_turning && is_turning)
                                {
                                    // Stopped turning - reset timers and start graceful shutdown
                                    turn_stop_timer = 0.0f;
                                    is_turning = false;
                                    if (blinkers_active)
                                    {
                                        should_stop_blinking = true;
                                        final_blink_timer = 0.0f;
                                    }
                                }

                                // Update timers using Timestep
                                if (is_turning && current_turn_direction != 0 && !should_stop_blinking)
                                {
                                    turn_start_timer += 1000.0f * Natives::Timestep(); // Convert to milliseconds

                                    // Check if we should activate blinkers
                                    if (turn_start_timer >= ACTIVATION_DELAY && !blinkers_active)
                                    {
                                        current_blinker = current_turn_direction;
                                        blinkers_active = true;
                                        blink_timer = 0.0f;
                                        blink_state = true; // Start with lights on
                                    }
                                }

                                // Handle blinking logic
                                if (blinkers_active && !should_stop_blinking)
                                {
                                    blink_timer += 1000.0f * Natives::Timestep();

                                    if (blink_timer >= BLINK_INTERVAL)
                                    {
                                        blink_state = !blink_state; // Toggle blink state
                                        blink_timer = 0.0f;

                                        if (blink_state)
                                        {
                                            // Turn lights on
                                            Natives::SetVehIndicatorlights(PlayerCar, true);
                                        }
                                        else
                                        {
                                            // Turn lights off
                                            Natives::SetVehIndicatorlights(PlayerCar, false);
                                        }
                                    }
                                }
                                // Handle graceful shutdown (finish current blink cycle)
                                else if (should_stop_blinking && blinkers_active)
                                {
                                    final_blink_timer += 1000.0f * Natives::Timestep();

                                    // If lights are currently on, wait for the blink cycle to complete
                                    if (blink_state)
                                    {
                                        blink_timer += 1000.0f * Natives::Timestep();

                                        if (blink_timer >= BLINK_INTERVAL)
                                        {
                                            // Complete the blink cycle by turning lights off
                                            Natives::SetVehIndicatorlights(PlayerCar, false);

                                            // Completely turn off blinkers
                                            blinkers_active = false;
                                            current_blinker = 0;
                                            should_stop_blinking = false;
                                            blink_state = false;
                                            turn_start_timer = 0.0f;
                                            turn_stop_timer = 0.0f;
                                            blink_timer = 0.0f;
                                        }
                                    }
                                    else
                                    {
                                        // Lights are already off, we can stop immediately
                                        blinkers_active = false;
                                        current_blinker = 0;
                                        should_stop_blinking = false;
                                        turn_start_timer = 0.0f;
                                        turn_stop_timer = 0.0f;
                                        blink_timer = 0.0f;
                                    }

                                    // Safety timeout - force stop after reasonable time
                                    if (final_blink_timer >= BLINK_INTERVAL * 2)
                                    {
                                        Natives::SetVehIndicatorlights(PlayerCar, false);

                                        blinkers_active = false;
                                        current_blinker = 0;
                                        should_stop_blinking = false;
                                        blink_state = false;
                                        turn_start_timer = 0.0f;
                                        turn_stop_timer = 0.0f;
                                        blink_timer = 0.0f;
                                    }
                                }

                                // Update last turn direction
                                last_turn_direction = current_turn_direction;
                            }
                            else
                            {
                                // Reset state when not in vehicle
                                turn_start_timer = 0.0f;
                                turn_stop_timer = 0.0f;
                                current_blinker = 0;
                                last_turn_direction = 0;
                                is_turning = false;
                                blinkers_active = false;
                                should_stop_blinking = false;
                                blink_state = false;
                                blink_timer = 0.0f;
                                final_blink_timer = 0.0f;
                            }
                        }
                    }
                };
            }
        };
    }
} TurnIndicators;