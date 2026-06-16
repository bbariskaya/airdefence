#pragma once

#include "radar/sensor_feed.hpp"
#include "game_config.hpp"
#include <cmath>
#include <random>
#include <string>
#include <vector>

namespace air_defense {

/**
 * Incoming ballistic threat with 3D reentry trajectory.
 */
struct Threat {
    std::int32_t id;
    float x_m;
    float y_m;
    float z_m = 0.f;       // altitude AGL
    float vx_mps;
    float vy_mps;
    float vz_mps = 0.f;
    float rcs_m2;
    std::string callsign;
    bool destroyed;
    float explosion_timer = 0.f;

    float speed_mps() const {
        return std::sqrt(vx_mps * vx_mps + vy_mps * vy_mps + vz_mps * vz_mps);
    }
    float slant_range_m() const {
        return std::sqrt(x_m * x_m + y_m * y_m + z_m * z_m);
    }
};

class ThreatWorld : public radar::ISensorFeed {
public:
    ThreatWorld();

    void tick(float dt_sec) override;
    std::vector<radar::AircraftState> truth_snapshot() const override;

    const std::vector<Threat>& threats() const { return threats_; }
    std::vector<Threat>& threats_mut() { return threats_; }
    Threat* threat_by_id(std::int32_t id);
    const Threat* threat_by_id(std::int32_t id) const;

    void spawn_threat();
    void destroy_threat(std::int32_t id);
    int active_threat_count() const;

    float spawn_interval_sec = GameConfig::kThreatSpawnIntervalSec;
    float detection_range_m = 250000.f;  // 250 km — middle between 180 and 450 km
    int max_active_threats = GameConfig::kMaxActiveThreats;
    float threat_speed_min_mps = 600.f;
    float threat_speed_max_mps = 1000.f;
    float threat_altitude_min_m = 12000.f;
    float threat_altitude_max_m = 35000.f;
    float threat_terminal_maneuver_g = 2.5f;  // lateral g in terminal phase below 15 km
    float threat_rcs_min_m2 = 0.5f;
    float threat_rcs_max_m2 = 2.0f;

private:
    std::vector<Threat> threats_;
    std::int32_t next_threat_id_ = 1000;
    float spawn_timer_sec_ = 0.f;

    std::mt19937 rng_;
    std::uniform_real_distribution<float> azimuth_dist_;
    std::uniform_real_distribution<float> speed_dist_;
    std::uniform_real_distribution<float> alt_dist_;
    std::uniform_real_distribution<float> rcs_dist_;

    void update_threat_positions(float dt_sec);
};

} // namespace air_defense
