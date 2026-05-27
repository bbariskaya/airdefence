#pragma once

#include "radar/radar_engine.hpp"
#include <cstdint>
#include <optional>

namespace air_defense {

/**
 * Battery controller manages player input, targeting, and firing constraints.
 */
class BatteryController {
public:
    BatteryController();

    void tick(float dt_sec);

    // Input handling (call each frame)
    void handle_target_selection(std::int32_t track_id);
    void handle_fire_command();

    // State getters
    std::int32_t selected_target_id() const { return selected_target_id_; }
    int ammo_count() const { return ammo_count_; }
    float reload_timer_sec() const { return reload_timer_sec_; }
    float battery_health() const { return battery_health_; }
    bool can_fire() const;

    // State setters
    void take_damage(float damage);
    void reset();

    // Configuration
    float battery_max_health = 100.f;
    int max_ammo = 12;
    float reload_time_sec = 2.5f;
    float damage_per_hit = 25.f;

    // Statistics
    int total_intercepted = 0;
    int total_threats_reached = 0;

private:
    std::int32_t selected_target_id_ = -1;
    int ammo_count_ = 12;
    float reload_timer_sec_ = 0.f;
    float battery_health_ = 100.f;
};

} // namespace air_defense
