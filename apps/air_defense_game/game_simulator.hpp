#pragma once

#include "battery_controller.hpp"
#include "game_config.hpp"
#include "interceptor.hpp"
#include "threat_world.hpp"
#include "radar/radar_engine.hpp"
#include <cstdint>
#include <string>
#include <vector>

namespace air_defense {

struct GameEvent {
    enum Type { Intercept, ThreatReached, Fired, TrackAcquired };
    Type type;
    std::int32_t threat_id = -1;
    std::int32_t missile_id = -1;
    float range_m = 0.f;
};

struct GameStepResult {
    bool game_over = false;
    std::vector<GameEvent> events;
};

/** Check proximity detonation between interceptor and threat. */
bool check_collision(const Interceptor& missile, const Threat& threat, float blast_radius);

/** True when threat has entered the battery kill zone. */
bool threat_reached_battery(const Threat& threat, float kill_radius_m = GameConfig::kBatteryKillRadiusM);

/** Pick closest live track that has no interceptor already assigned. */
std::int32_t select_closest_untargeted_track(const radar::RadarEngine& engine,
                                             const InterceptorManager& missiles,
                                             float min_confidence = GameConfig::kMinTrackConfidence);

bool has_active_interceptor_for_target(const InterceptorManager& missiles, std::int32_t target_id);

/**
 * One simulation tick: world, radar, missiles, auto-fire at closest new track,
 * collisions, battery damage. Shared by GUI and console builds.
 */
GameStepResult step_simulation(float dt_sec,
                               ThreatWorld& threat_world,
                               radar::RadarEngine& engine,
                               InterceptorManager& interceptor_mgr,
                               BatteryController& battery,
                               float& fire_cooldown_sec,
                               bool use_missile_guidance = true);

} // namespace air_defense
