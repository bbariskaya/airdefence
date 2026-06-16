#pragma once

#include "radar/sensor_feed.hpp"
#include "radar/types.hpp"
#include <cstdint>
#include <unordered_set>
#include <vector>

namespace radar {

/**
 * Rotating-beam PPI radar model: sweep azimuth, listen for echoes, threshold, track.
 * Same control loop you'd run on embedded (fixed dt, no heap in hot path if desired).
 */
class RadarEngine {
public:
    struct Config {
        float max_range_m;
        float beam_width_deg;
        float sweep_deg_per_sec;
        float detection_threshold;
        float range_resolution_m;
        /** When true, every target in range is detected each frame (search radar). */
        bool omnidirectional_search;
        Config()
            : max_range_m(500000.f),
              beam_width_deg(30.f),
              sweep_deg_per_sec(90.f),
              detection_threshold(0.12f),
              range_resolution_m(500.f),
              omnidirectional_search(true) {}
    };

    RadarEngine();
    explicit RadarEngine(Config cfg);

    void set_sensor(ISensorFeed* feed) { sensor_ = feed; }

    /** IFF: skip detections for these target ids (e.g. own aircraft). */
    void ignore_target(std::int32_t id) { ignore_ids_.insert(id); }
    void clear_ignore() { ignore_ids_.clear(); }

    /** Advance physics + one radar processing frame. */
    void tick(float dt_sec);

    float sweep_azimuth_deg() const { return sweep_az_; }
    const std::vector<Detection>& last_detections() const { return last_hits_; }
    const std::vector<Track>& tracks() const { return tracks_; }

private:
    Config cfg_;
    ISensorFeed* sensor_{nullptr};
    float sweep_az_{0};
    std::vector<Detection> last_hits_;
    std::vector<Track> tracks_;
    std::uint32_t time_ms_{0};
    std::unordered_set<std::int32_t> ignore_ids_;

    void process_sweep(const std::vector<AircraftState>& world);
    void update_tracks(const std::vector<Detection>& hits);
    static float normalize_angle(float deg);
};

} // namespace radar
