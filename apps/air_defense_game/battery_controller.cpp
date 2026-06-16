#include "battery_controller.hpp"

namespace air_defense {

BatteryController::BatteryController()
    : selected_target_id_(-1),
      battery_health_(100.f) {}

void BatteryController::tick(float dt_sec) {
    // No reload logic needed - infinite ammo
}

void BatteryController::handle_target_selection(std::int32_t track_id) {
    selected_target_id_ = track_id;
}

bool BatteryController::can_fire() const {
    return battery_health_ > 0.f;
}

void BatteryController::take_damage(float damage) {
    battery_health_ -= damage;
    if (battery_health_ < 0.f) {
        battery_health_ = 0.f;
    }
}

void BatteryController::reset() {
    selected_target_id_ = -1;
    battery_health_ = battery_max_health;
    total_intercepted = 0;
    total_threats_reached = 0;
}

} // namespace air_defense
