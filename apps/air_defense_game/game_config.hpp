#pragma once

namespace air_defense {

/** Shared simulation constants for GUI and console modes. */
struct GameConfig {
    static constexpr float kMaxRadarRangeM = 500000.f;
    static constexpr float kBatteryKillRadiusM = 2000.f;
    static constexpr float kFireCooldownSec = 0.15f;
    static constexpr float kMinTrackConfidence = 0.35f;
    static constexpr float kExplosionDurationSec = 1.2f;

    /** Balance: between 1x (real-time) and 12x — paced but not sluggish. */
    static constexpr float kSimTimeScale = 5.f;

    static constexpr float kThreatSpawnIntervalSec = 3.f;
    static constexpr int kMaxActiveThreats = 20;
};

} // namespace air_defense
