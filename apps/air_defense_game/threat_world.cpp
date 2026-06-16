#include "threat_world.hpp"
#include "ballistics.hpp"
#include "game_config.hpp"
#include <cmath>
#include <algorithm>

namespace air_defense {

ThreatWorld::ThreatWorld()
    : rng_(std::random_device{}()),
      azimuth_dist_(0.f, 360.f),
      speed_dist_(threat_speed_min_mps, threat_speed_max_mps),
      alt_dist_(threat_altitude_min_m, threat_altitude_max_m),
      rcs_dist_(threat_rcs_min_m2, threat_rcs_max_m2) {}

int ThreatWorld::active_threat_count() const {
    int n = 0;
    for (const auto& t : threats_) {
        if (!t.destroyed) ++n;
    }
    return n;
}

void ThreatWorld::tick(float dt_sec) {
    spawn_timer_sec_ += dt_sec;
    if (spawn_timer_sec_ >= spawn_interval_sec && active_threat_count() < max_active_threats) {
        spawn_threat();
        spawn_timer_sec_ = 0.f;
    }

    update_threat_positions(dt_sec);

    for (auto& threat : threats_) {
        if (threat.destroyed) {
            threat.explosion_timer += dt_sec;
        }
    }

    threats_.erase(std::remove_if(threats_.begin(), threats_.end(),
                                  [](const Threat& t) {
                                      return t.destroyed && t.explosion_timer > GameConfig::kExplosionDurationSec;
                                  }),
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

const Threat* ThreatWorld::threat_by_id(std::int32_t id) const {
    for (const auto& t : threats_) {
        if (t.id == id) return &t;
    }
    return nullptr;
}

void ThreatWorld::spawn_threat() {
    float azimuth = azimuth_dist_(rng_);
    float speed = speed_dist_(rng_);
    float altitude = alt_dist_(rng_);
    float rcs = rcs_dist_(rng_);

    float rad = (azimuth - 90.f) * 3.14159265f / 180.f;
    float x = detection_range_m * std::cos(rad);
    float y = detection_range_m * std::sin(rad);
    float z = altitude;

    float horiz = std::hypot(x, y);
    float ux = -x / horiz;
    float uy = -y / horiz;

    // Ballistic reentry: fast horizontal + downward vertical component
    float dive_angle = 0.35f + (rcs_dist_(rng_) * 0.1f);  // ~20-40 deg dive
    float v_horiz = speed * std::cos(dive_angle);
    float v_down = speed * std::sin(dive_angle);

    Threat threat;
    threat.id = next_threat_id_++;
    threat.x_m = x;
    threat.y_m = y;
    threat.z_m = z;
    threat.vx_mps = ux * v_horiz;
    threat.vy_mps = uy * v_horiz;
    threat.vz_mps = -v_down;
    threat.rcs_m2 = rcs;
    threat.destroyed = false;
    threat.callsign = "BM" + std::to_string(threat.id - 1000);

    threats_.push_back(threat);
}

void ThreatWorld::destroy_threat(std::int32_t id) {
    if (Threat* t = threat_by_id(id)) {
        t->destroyed = true;
    }
}

void ThreatWorld::update_threat_positions(float dt_sec) {
    const float g = Ballistics::kGravityMps2;
    for (auto& threat : threats_) {
        if (threat.destroyed) continue;

        if (threat.z_m < 15000.f && threat.z_m > 500.f) {
            float phase = static_cast<float>(threat.id) * 0.17f + threat.explosion_timer;
            float lateral = threat_terminal_maneuver_g * g * dt_sec;
            threat.vx_mps += lateral * std::sin(phase * 3.1f);
            threat.vy_mps += lateral * std::cos(phase * 2.7f);
            float spd = threat.speed_mps();
            if (spd > 100.f) {
                float drag_decel = Ballistics::kAirDensityScale * spd * spd;
                drag_decel = std::min(drag_decel, 15.f);
                float scale = std::max(0.f, spd - drag_decel * dt_sec) / spd;
                threat.vx_mps *= scale;
                threat.vy_mps *= scale;
                threat.vz_mps *= scale;
            }
        }

        threat.x_m += threat.vx_mps * dt_sec;
        threat.y_m += threat.vy_mps * dt_sec;
        threat.z_m += threat.vz_mps * dt_sec;
        threat.vz_mps -= g * dt_sec;
        if (threat.z_m < 0.f) {
            threat.z_m = 0.f;
            threat.vz_mps = 0.f;
        }
    }
}

} // namespace air_defense
