#include "battery_controller.hpp"

namespace air_defense {

BatteryController::BatteryController()
    : selected_target_id_(-1),
      ammo_count_(12),
      reload_timer_sec_(0.f),
      battery_health_(100.f) {}

void BatteryController::tick(float dt_sec) {
    // Update reload timer
    if (reload_timer_sec_ > 0.f) {
        reload_timer_sec_ -= dt_sec;
        if (reload_timer_sec_ < 0.f) {
            reload_timer_sec_ = 0.f;
        }
    }

    // Reload logic: after firing all ammo, start reload cycle
    if (ammo_count_ == 0 && reload_timer_sec_ <= 0.f) {
        // Magazine reload
        ammo_count_ = max_ammo;
        reload_timer_sec_ = reload_time_sec;
    }
}

void BatteryController::handle_target_selection(std::int32_t track_id) {
    selected_target_id_ = track_id;
}

void BatteryController::handle_fire_command() {
    if (can_fire()) {
        ammo_count_--;
        if (ammo_count_ == 0) {
            reload_timer_sec_ = reload_time_sec;
        }
    }
}

bool BatteryController::can_fire() const {
    return ammo_count_ > 0 && reload_timer_sec_ <= 0.f && battery_health_ > 0.f;
}

void BatteryController::take_damage(float damage) {
    battery_health_ -= damage;
    if (battery_health_ < 0.f) {
        battery_health_ = 0.f;
    }
}

void BatteryController::reset() {
    selected_target_id_ = -1;
    ammo_count_ = max_ammo;
    reload_timer_sec_ = 0.f;
    battery_health_ = battery_max_health;
    total_intercepted = 0;
    total_threats_reached = 0;
}

} // namespace air_defense
