#include "threat_world.hpp"
#include <cmath>
#include <algorithm>

namespace air_defense {

ThreatWorld::ThreatWorld()
    : rng_(std::random_device{}()),
      azimuth_dist_(0.f, 360.f),
      speed_dist_(threat_speed_min_mps, threat_speed_max_mps),
      rcs_dist_(threat_rcs_min_m2, threat_rcs_max_m2) {}

void ThreatWorld::tick(float dt_sec) {
    // Update spawn timer
    spawn_timer_sec_ += dt_sec;
    if (spawn_timer_sec_ >= spawn_interval_sec) {
        spawn_threat();
        spawn_timer_sec_ = 0.f;
    }

    // Update threat positions
    update_threat_positions(dt_sec);
    
    // Update explosion timers for destroyed threats
    for (auto& threat : threats_) {
        if (threat.destroyed) {
            threat.explosion_timer += dt_sec;
        }
    }

    // Remove destroyed threats after explosion animation completes
    threats_.erase(std::remove_if(threats_.begin(), threats_.end(),
                                  [](const Threat& t) { return t.destroyed && t.explosion_timer > 0.5f; }),
                   threats_.end());
}

std::vector<radar::AircraftState> ThreatWorld::truth_snapshot() const {
    std::vector<radar::AircraftState> out;
    for (const auto& threat : threats_) {
        if (!threat.destroyed) {
            radar::AircraftState state;
            state.id = threat.id;
            state.x_m = threat.x_m;
            state.y_m = threat.y_m;
            state.vx_mps = threat.vx_mps;
            state.vy_mps = threat.vy_mps;
            state.rcs_m2 = threat.rcs_m2;
            state.callsign = threat.callsign;
            out.push_back(state);
        }
    }
    return out;
}

Threat* ThreatWorld::threat_by_id(std::int32_t id) {
    for (auto& t : threats_) {
        if (t.id == id) return &t;
    }
    return nullptr;
}

void ThreatWorld::spawn_threat() {
    // Random azimuth in all directions
    float azimuth = azimuth_dist_(rng_);

    // Random speed toward battery
    float speed = speed_dist_(rng_);

    // Random RCS (smaller than aircraft)
    float rcs = rcs_dist_(rng_);

    // Spawn at outer detection range, heading toward battery (0,0)
    float rad = (azimuth - 90.f) * 3.14159265f / 180.f;
    float x = detection_range_m * std::cos(rad);
    float y = detection_range_m * std::sin(rad);

    // Velocity vector points toward origin (battery)
    float dist_to_origin = std::hypot(x, y);
    float vx = -(x / dist_to_origin) * speed;
    float vy = -(y / dist_to_origin) * speed;

    Threat threat;
    threat.id = next_threat_id_++;
    threat.x_m = x;
    threat.y_m = y;
    threat.vx_mps = vx;
    threat.vy_mps = vy;
    threat.rcs_m2 = rcs;
    threat.destroyed = false;
    threat.callsign = "THR" + std::to_string(threat.id - 1000);

    threats_.push_back(threat);
}

void ThreatWorld::destroy_threat(std::int32_t id) {
    Threat* t = threat_by_id(id);
    if (t) {
        t->destroyed = true;
    }
}

void ThreatWorld::update_threat_positions(float dt_sec) {
    for (auto& threat : threats_) {
        if (!threat.destroyed) {
            threat.x_m += threat.vx_mps * dt_sec;
            threat.y_m += threat.vy_mps * dt_sec;
        }
    }
}

} // namespace air_defense
