#include "game_simulator.hpp"
#include "ballistics.hpp"
#include <algorithm>
#include <cmath>

namespace air_defense {

bool check_collision(const Interceptor& missile, const Threat& threat, float blast_radius) {
    if (missile.destroyed || threat.destroyed) return false;
    float dx = missile.x_m - threat.x_m;
    float dy = missile.y_m - threat.y_m;
    float dz = missile.z_m - threat.z_m;
    return std::sqrt(dx * dx + dy * dy + dz * dz) < blast_radius;
}

bool threat_reached_battery(const Threat& threat, float kill_radius_m) {
    return std::hypot(threat.x_m, threat.y_m) < kill_radius_m;
}

bool has_active_interceptor_for_target(const InterceptorManager& missiles, std::int32_t target_id) {
    if (target_id < 0) return false;
    for (const auto& m : missiles.interceptors()) {
        if (m.is_alive() && m.target_id == target_id) return true;
    }
    return false;
}

std::int32_t select_closest_untargeted_track(const radar::RadarEngine& engine,
                                             const InterceptorManager& missiles,
                                             float min_confidence) {
    float best_range = 1e9f;
    std::int32_t best_id = -1;

    for (const auto& t : engine.tracks()) {
        if (t.confidence < min_confidence) continue;
        if (has_active_interceptor_for_target(missiles, t.id)) continue;
        if (t.range_m < best_range) {
            best_range = t.range_m;
            best_id = t.id;
        }
    }
    return best_id;
}

GameStepResult step_simulation(float dt_sec,
                               ThreatWorld& threat_world,
                               radar::RadarEngine& engine,
                               InterceptorManager& interceptor_mgr,
                               BatteryController& battery,
                               float& fire_cooldown_sec,
                               bool use_missile_guidance) {
    GameStepResult result;

    threat_world.tick(dt_sec);
    engine.tick(dt_sec);

    if (use_missile_guidance) {
        interceptor_mgr.tick(dt_sec, threat_world);
    } else {
        interceptor_mgr.tick(dt_sec);
    }
    battery.tick(dt_sec);

    if (fire_cooldown_sec > 0.f) {
        fire_cooldown_sec = std::max(0.f, fire_cooldown_sec - dt_sec);
    }

    const std::int32_t target_id = select_closest_untargeted_track(engine, interceptor_mgr);
    if (target_id >= 0 && fire_cooldown_sec <= 0.f && battery.can_fire()) {
        battery.handle_target_selection(target_id);
        if (Threat* target = threat_world.threat_by_id(target_id)) {
            interceptor_mgr.fire_interceptor(0.f, 0.f, 0.f,
                                             target->x_m, target->y_m, target->z_m,
                                             target->vx_mps, target->vy_mps, target->vz_mps,
                                             target_id);
            fire_cooldown_sec = GameConfig::kFireCooldownSec;

            GameEvent ev;
            ev.type = GameEvent::Fired;
            ev.threat_id = target_id;
            ev.missile_id = interceptor_mgr.interceptors().empty()
                                ? -1
                                : interceptor_mgr.interceptors().back().id;
            ev.range_m = std::hypot(target->x_m, target->y_m);
            result.events.push_back(ev);
        }
    }

    for (auto& missile : interceptor_mgr.interceptors()) {
        if (missile.destroyed) continue;
        for (auto& threat : threat_world.threats_mut()) {
            if (threat.destroyed) continue;
            if (check_collision(missile, threat, interceptor_mgr.blast_radius_m)) {
                missile.destroyed = true;
                threat.destroyed = true;
                threat.explosion_timer = 0.f;
                battery.total_intercepted++;
                interceptor_mgr.total_intercepted++;

                GameEvent ev;
                ev.type = GameEvent::Intercept;
                ev.threat_id = threat.id;
                ev.missile_id = missile.id;
                ev.range_m = std::hypot(threat.x_m, threat.y_m);
                result.events.push_back(ev);
                break;
            }
        }
    }

    for (auto& threat : threat_world.threats_mut()) {
        if (threat.destroyed) continue;
        if (threat_reached_battery(threat)) {
            threat.destroyed = true;
            battery.take_damage(battery.damage_per_hit);
            battery.total_threats_reached++;

            GameEvent ev;
            ev.type = GameEvent::ThreatReached;
            ev.threat_id = threat.id;
            ev.range_m = std::hypot(threat.x_m, threat.y_m);
            result.events.push_back(ev);

            if (battery.battery_health() <= 0.f) {
                result.game_over = true;
            }
        }
    }

    return result;
}

} // namespace air_defense
