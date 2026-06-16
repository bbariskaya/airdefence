#pragma once

#include "radar/sensor_feed.hpp"
#include <cmath>
#include <random>
#include <vector>

namespace air_defense {

/**
 * Represents an incoming threat (ballistic missile, rocket, etc.)
 */
struct Threat {
    std::int32_t id;
    float x_m;
    float y_m;
    float vx_mps;
    float vy_mps;
    float rcs_m2;
    std::string callsign;
    bool destroyed;
    float explosion_timer = 0.f;  // Time since destruction for explosion effect
};

/**
 * ThreatWorld generates randomized incoming threats in all directions
 * and simulates their ballistic motion toward the battery origin.
 * Implements ISensorFeed for use with RadarEngine.
 */
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

    // Configuration
    float spawn_interval_sec = 5.0f;  // Slower spawns - fewer threats at once
    float detection_range_m = 200000.f;  // 200 km - realistic air defense range
    float threat_speed_min_mps = 300.f;
    float threat_speed_max_mps = 500.f;
    float threat_rcs_min_m2 = 0.5f;
    float threat_rcs_max_m2 = 2.0f;

private:
    std::vector<Threat> threats_;
    std::int32_t next_threat_id_ = 1000;
    float spawn_timer_sec_ = 0.f;

    std::mt19937 rng_;
    std::uniform_real_distribution<float> azimuth_dist_;  // 0 to 360
    std::uniform_real_distribution<float> speed_dist_;    // min to max speed
    std::uniform_real_distribution<float> rcs_dist_;      // min to max RCS

    void update_threat_positions(float dt_sec);
};

} // namespace air_defense
