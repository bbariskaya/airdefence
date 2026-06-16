#include "battery_controller.hpp"
#include "interceptor.hpp"
#include "threat_world.hpp"
#include "radar/radar_engine.hpp"
#include <iostream>
#include <iomanip>
#include <cmath>
#include <chrono>

namespace {
constexpr float kMaxRange = 500000.f;

/**
 * Check collision between interceptor and threat.
 */
bool check_collision(const air_defense::Interceptor& missile,
                    const air_defense::Threat& threat,
                    float blast_radius) {
    float dx = missile.x_m - threat.x_m;
    float dy = missile.y_m - threat.y_m;
    float dist = std::hypot(dx, dy);
    return dist < blast_radius;
}

/**
 * Check if threat has reached the battery (within 2km).
 */
bool threat_reached_battery(const air_defense::Threat& threat) {
    float dist = std::hypot(threat.x_m, threat.y_m);
    return dist < 2000.f;
}

void print_header(int frame) {
    std::cout << "\n" << std::string(120, '=') << "\n";
    std::cout << "FRAME " << frame << " - AUTO-TARGETING AIR DEFENSE SYSTEM DEBUG\n";
    std::cout << std::string(120, '=') << "\n";
}

void print_threats(const air_defense::ThreatWorld& world) {
    std::cout << "\n[THREATS] (" << world.threats().size() << " active)\n";
    for (const auto& t : world.threats()) {
        if (!t.destroyed) {
            float range = std::hypot(t.x_m, t.y_m);
            float bearing = std::atan2(t.x_m, t.y_m) * 57.2958f;
            float speed = std::hypot(t.vx_mps, t.vy_mps);
            std::cout << "  ID " << t.id << " | Range: " << std::fixed << std::setprecision(0)
                      << range / 1000.f << " km | Bearing: " << std::setprecision(1) << bearing
                      << "° | Speed: " << speed << " m/s | RCS: " << t.rcs_m2 << " m²\n";
        }
    }
}

void print_tracks(const radar::RadarEngine& engine) {
    std::cout << "\n[RADAR TRACKS] (" << engine.tracks().size() << " total)\n";
    for (const auto& t : engine.tracks()) {
        std::cout << "  Track ID " << t.id << " | Range: " << std::fixed << std::setprecision(0)
                  << t.range_m / 1000.f << " km | Az: " << std::setprecision(1) << t.azimuth_deg
                  << "° | Confidence: " << std::setprecision(2) << t.confidence * 100.f << "% | Speed: "
                  << std::setprecision(0) << t.speed_mps << " m/s\n";
    }
    
    const auto& detections = engine.last_detections();
    std::cout << "\n[RAW DETECTIONS] (" << detections.size() << " this frame)\n";
    if (detections.empty()) {
        std::cout << "  WARNING: No detections! Check radar config and threshold.\n";
    }
    for (const auto& d : detections) {
        std::cout << "  Range: " << std::fixed << std::setprecision(0) << d.range_m / 1000.f 
                  << " km | Az: " << std::setprecision(1) << d.azimuth_deg << "° | Amp: " 
                  << std::setprecision(3) << d.amplitude << " | Target ID: " << d.target_id << "\n";
    }
}

void print_interceptors(const air_defense::InterceptorManager& mgr) {
    std::cout << "\n[INTERCEPTORS] (" << mgr.interceptors().size() << " in flight)\n";
    for (const auto& m : mgr.interceptors()) {
        if (m.is_alive()) {
            float range = std::hypot(m.x_m, m.y_m);
            std::cout << "  Missile ID " << m.id << " | Range from origin: " << std::fixed
                      << std::setprecision(0) << range / 1000.f << " km | Lifetime: "
                      << std::setprecision(2) << m.lifetime_sec << " / " << m.max_lifetime_sec << " sec\n";
        }
    }
    std::cout << "  Total fired: " << mgr.total_fired << " | Total intercepted: " << mgr.total_intercepted << "\n";
}

void print_battery(const air_defense::BatteryController& battery) {
    std::cout << "\n[BATTERY STATUS]\n";
    std::cout << "  Health: " << std::fixed << std::setprecision(1) << battery.battery_health() << " / "
              << battery.battery_max_health << " HP\n";
    std::cout << "  Selected Target ID: " << battery.selected_target_id() << "\n";
    std::cout << "  Can Fire: " << (battery.can_fire() ? "YES" : "NO") << "\n";
    std::cout << "  Score - Intercepted: " << battery.total_intercepted
              << " | Reached Battery: " << battery.total_threats_reached << "\n";
}

void print_collisions(int intercepted, int reached) {
    if (intercepted > 0) {
        std::cout << "\n[!] INTERCEPTOR HIT! Threat destroyed\n";
    }
    if (reached > 0) {
        std::cout << "\n[!] THREAT REACHED BATTERY! Damage taken\n";
    }
}

} // namespace

int main() {
    std::cout << "AIR DEFENSE GAME - DEBUG/CONSOLE MODE\n";
    std::cout << "No GUI - Real-time console output\n\n";

    // Initialize game systems
    air_defense::ThreatWorld threat_world;
    radar::RadarEngine::Config cfg;
    cfg.max_range_m = kMaxRange;
    cfg.sweep_deg_per_sec = 60.f;
    radar::RadarEngine engine(cfg);
    engine.set_sensor(&threat_world);

    std::cout << "[RADAR CONFIG]\n";
    std::cout << "  Max Range: " << cfg.max_range_m / 1000.f << " km\n";
    std::cout << "  Beam Width: " << cfg.beam_width_deg << "°\n";
    std::cout << "  Sweep Speed: " << cfg.sweep_deg_per_sec << "°/sec\n";
    std::cout << "  Detection Threshold: " << cfg.detection_threshold << "\n";
    std::cout << "  Range Resolution: " << cfg.range_resolution_m << " m\n\n";

    air_defense::InterceptorManager interceptor_mgr;
    air_defense::BatteryController battery;

    float fire_cooldown = 0.f;
    float dt = 0.016f;  // 60 FPS
    int frame = 0;
    int prev_intercepted = 0;
    int prev_reached = 0;

    std::cout << "Starting simulation. Press Ctrl+C to stop.\n\n";

    // Simulation loop
    while (battery.battery_health() > 0.f && frame < 10000) {
        frame++;

        // Update world
        threat_world.tick(dt);
        engine.tick(dt);
        interceptor_mgr.tick(dt);
        battery.tick(dt);

        // Update fire cooldown
        if (fire_cooldown > 0.f) {
            fire_cooldown -= dt;
        }

        // AUTO-TARGETING: Find nearest high-confidence track
        float best_range = 1e9f;
        std::int32_t best_track_id = -1;
        for (const auto& t : engine.tracks()) {
            if (t.confidence > 0.3f && t.range_m < best_range) {
                best_range = t.range_m;
                best_track_id = t.id;
            }
        }

        // Set target and auto-fire if we have a target and cooldown is up
        if (best_track_id >= 0 && fire_cooldown <= 0.f) {
            battery.handle_target_selection(best_track_id);
            if (battery.can_fire()) {
                air_defense::Threat* target = threat_world.threat_by_id(best_track_id);
                if (target) {
                    interceptor_mgr.fire_interceptor(0.f, 0.f,
                                                     target->x_m, target->y_m,
                                                     target->vx_mps, target->vy_mps);
                    fire_cooldown = 0.15f;
                }
            }
        }

        // Check collisions: interceptors vs threats
        for (auto& missile : interceptor_mgr.interceptors()) {
            if (missile.destroyed) continue;

            for (auto& threat : threat_world.threats_mut()) {
                if (threat.destroyed) continue;

                if (check_collision(missile, threat, interceptor_mgr.blast_radius_m)) {
                    missile.destroyed = true;
                    threat.destroyed = true;
                    threat.explosion_timer = 0.f;
                    battery.total_intercepted++;
                    break;
                }
            }
        }

        // Check if threats reached battery
        for (auto& threat : threat_world.threats_mut()) {
            if (!threat.destroyed && threat_reached_battery(threat)) {
                threat.destroyed = true;
                battery.take_damage(battery.damage_per_hit);
                battery.total_threats_reached++;

                if (battery.battery_health() <= 0.f) {
                    std::cout << "\n" << std::string(120, '*') << "\n";
                    std::cout << "GAME OVER - BATTERY DESTROYED\n";
                    std::cout << std::string(120, '*') << "\n";
                    break;
                }
            }
        }

        // Print debug info every 60 frames (1 second at 60 FPS)
        if (frame % 60 == 0) {
            print_header(frame);
            print_threats(threat_world);
            print_tracks(engine);
            print_interceptors(interceptor_mgr);
            print_battery(battery);
            
            if (battery.total_intercepted != prev_intercepted || battery.total_threats_reached != prev_reached) {
                print_collisions(battery.total_intercepted - prev_intercepted,
                               battery.total_threats_reached - prev_reached);
                prev_intercepted = battery.total_intercepted;
                prev_reached = battery.total_threats_reached;
            }
        }

        // Also print immediately when important events happen
        if (battery.total_intercepted != prev_intercepted) {
            std::cout << "\n[EVENT] Threat intercepted! Total: " << battery.total_intercepted << "\n";
            prev_intercepted = battery.total_intercepted;
        }
        if (battery.total_threats_reached != prev_reached) {
            std::cout << "\n[EVENT] Threat reached battery! Damage taken. Health: "
                      << battery.battery_health() << "/" << battery.battery_max_health << "\n";
            prev_reached = battery.total_threats_reached;
        }
    }

    // Final report
    std::cout << "\n" << std::string(120, '=') << "\n";
    std::cout << "FINAL REPORT\n";
    std::cout << std::string(120, '=') << "\n";
    std::cout << "Simulation ran for " << frame << " frames (" << std::fixed << std::setprecision(1)
              << frame * dt << " seconds)\n";
    std::cout << "Total threats spawned: " << threat_world.threats().size() + battery.total_intercepted + battery.total_threats_reached << "\n";
    std::cout << "Threats intercepted: " << battery.total_intercepted << "\n";
    std::cout << "Threats reached battery: " << battery.total_threats_reached << "\n";
    std::cout << "Final battery health: " << battery.battery_health() << " / " << battery.battery_max_health << "\n";
    std::cout << "Missiles fired: " << interceptor_mgr.total_fired << "\n";
    std::cout << std::string(120, '=') << "\n";

    return 0;
}
