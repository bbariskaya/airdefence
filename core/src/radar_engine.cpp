#include "radar/radar_engine.hpp"
#include "radar/radar_physics.hpp"
#include <algorithm>
#include <cmath>
#include <random>

namespace radar {
namespace {
constexpr float kPi = 3.14159265f;
float deg2rad(float d) { return d * kPi / 180.f; }
float rad2deg(float r) { return r * 180.f / kPi; }
} // namespace

RadarEngine::RadarEngine() : cfg_() {}

RadarEngine::RadarEngine(Config cfg) : cfg_(cfg) {}

float RadarEngine::normalize_angle(float deg) {
    while (deg < 0.f) deg += 360.f;
    while (deg >= 360.f) deg -= 360.f;
    return deg;
}

void RadarEngine::tick(float dt_sec) {
    time_ms_ += static_cast<std::uint32_t>(dt_sec * 1000.f);
    if (sensor_) sensor_->tick(dt_sec);

    sweep_az_ += cfg_.sweep_deg_per_sec * dt_sec;
    if (sweep_az_ >= 360.f) sweep_az_ -= 360.f;

    std::vector<AircraftState> world;
    if (sensor_) world = sensor_->truth_snapshot();
    process_sweep(world);
    update_tracks(last_hits_);
}

void RadarEngine::process_sweep(const std::vector<AircraftState>& world) {
    last_hits_.clear();
    static std::mt19937 rng{7};
    std::uniform_real_distribution<float> noise(0.f, 0.12f);

    const float beam = cfg_.beam_width_deg;

    for (const auto& ac : world) {
        if (ignore_ids_.count(ac.id)) continue;
        float range = std::hypot(ac.x_m, ac.y_m);
        if (range < RadarPhysics::kMinRangeM || range > cfg_.max_range_m) continue;

        float az = normalize_angle(rad2deg(std::atan2(ac.x_m, ac.y_m)));
        float delta = std::fabs(az - sweep_az_);
        if (delta > 180.f) delta = 360.f - delta;
        if (delta > beam * 0.5f) continue;

        const float r4 = range * range * range * range;
        float amp = ac.rcs_m2 * RadarPhysics::kRadarCal / (RadarPhysics::kRangeFloor + r4);
        amp = std::min(1.5f, amp) + noise(rng);
        if (amp < cfg_.detection_threshold) continue;
        float range_bin = std::round(range / cfg_.range_resolution_m) * cfg_.range_resolution_m;
        Detection d;
        d.range_m = range_bin;
        d.azimuth_deg = az;
        d.amplitude = amp;
        d.target_id = ac.id;
        last_hits_.push_back(d);
    }
}

void RadarEngine::update_tracks(const std::vector<Detection>& hits) {
    // Association gate: allow 1.5 range bins + 6 deg azimuth uncertainty
    const float gate_range = cfg_.range_resolution_m * 1.5f;
    const float gate_az = 6.0f;  // degrees

    for (const auto& h : hits) {
        Track* best = nullptr;
        float best_score = 1e9f;
        for (auto& t : tracks_) {
            float dr = std::fabs(t.range_m - h.range_m);
            float da = std::fabs(t.azimuth_deg - h.azimuth_deg);
            if (da > 180.f) da = 360.f - da;
            float score = dr + da * 100.f;  // Favor azimuth matching
            if (score < best_score && dr < gate_range && da < gate_az) {
                best_score = score;
                best = &t;
            }
        }
        if (best) {
            // Smooth filter: 60% old track + 40% new detection
            best->range_m = 0.6f * best->range_m + 0.4f * h.range_m;
            best->azimuth_deg = normalize_angle(0.6f * best->azimuth_deg + 0.4f * h.azimuth_deg);
            best->confidence = std::min(1.f, best->confidence + 0.15f);  // Boost confidence faster
            best->last_seen_ms = time_ms_;
        } else {
            // New track
            Track t;
            t.id = static_cast<std::int32_t>(tracks_.size() + 1);
            t.range_m = h.range_m;
            t.azimuth_deg = h.azimuth_deg;
            t.confidence = 0.50f;  // Start higher
            t.last_seen_ms = time_ms_;
            t.callsign = "T" + std::to_string(t.id);
            if (sensor_) {
                for (const auto& ac : sensor_->truth_snapshot()) {
                    if (ac.id == h.target_id) {
                        t.callsign = ac.callsign;
                        float spd = std::hypot(ac.vx_mps, ac.vy_mps);
                        t.speed_mps = spd;
                        t.heading_deg = normalize_angle(rad2deg(std::atan2(ac.vx_mps, ac.vy_mps)));
                        break;
                    }
                }
            }
            tracks_.push_back(t);
        }
    }

    // Drop stale or low-confidence tracks
    tracks_.erase(
        std::remove_if(tracks_.begin(), tracks_.end(),
                       [&](const Track& t) {
                           return (time_ms_ - t.last_seen_ms) > 6000  // 6 sec timeout
                               || t.confidence < 0.05f;  // Drop very weak
                       }),
        tracks_.end());
}

} // namespace radar
