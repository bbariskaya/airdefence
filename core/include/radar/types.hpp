#pragma once

#include <cstdint>
#include <string>

namespace radar {

/** Polar detection from one sweep (what real DSP would emit). */
struct Detection {
    float range_m{0};
    float azimuth_deg{0};
    float amplitude{0}; // 0..1 after AGC/normalization
    std::int32_t target_id{-1};
};

/** Fused track — same structure you'd publish over CAN/Ethernet to a display or UE. */
struct Track {
    std::int32_t id{0};
    float range_m{0};
    float azimuth_deg{0};
    float speed_mps{0};
    float heading_deg{0};
    float confidence{0};
    std::string callsign;
    std::uint32_t last_seen_ms{0};
};

/** Ground-truth aircraft (simulation only; replace with sensor feed on hardware). */
struct AircraftState {
    std::int32_t id{0};
    float x_m{0};
    float y_m{0};
    float vx_mps{0};
    float vy_mps{0};
    float rcs_m2{1}; // radar cross section scales echo strength
    std::string callsign;
};

} // namespace radar
