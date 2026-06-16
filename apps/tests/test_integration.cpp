#include "battery_controller.hpp"
#include "interceptor.hpp"
#include "threat_world.hpp"
#include "radar/radar_engine.hpp"
#include <iostream>
#include <iomanip>
#include <cassert>
#include <cmath>

namespace {
int passed = 0;
int failed = 0;

void test(const std::string& name, bool condition) {
    if (condition) {
        std::cout << "  ✓ " << name << "\n";
        passed++;
    } else {
        std::cout << "  ✗ FAILED: " << name << "\n";
        failed++;
    }
}

void print_header(const std::string& section) {
    std::cout << "\n" << std::string(80, '=') << "\n";
    std::cout << "INTEGRATION TEST: " << section << "\n";
    std::cout << std::string(80, '=') << "\n";
}
}

int main() {
    std::cout << "AIR DEFENSE INTEGRATION TESTS\n\n";

    // ===== RADAR + THREAT INTEGRATION =====
    print_header("Radar Detection Integration");
    {
        air_defense::ThreatWorld threat_world;
        radar::RadarEngine::Config cfg;
        cfg.max_range_m = 200000.f;  // 200 km (realistic)
        cfg.beam_width_deg = 30.f;  // Wide beam
        radar::RadarEngine engine(cfg);
        engine.set_sensor(&threat_world);

        std::cout << "  Spawning threats and running radar...\n";
        
        int detections = 0;
        int tracks = 0;
        
        for (int frame = 0; frame < 600; frame++) {
            threat_world.tick(0.016f);
            engine.tick(0.016f);
            
            if (!engine.last_detections().empty()) {
                detections++;
            }
            if (!engine.tracks().empty()) {
                tracks++;
            }
        }

        std::cout << "  Frames with detections: " << detections << " / 600\n";
        std::cout << "  Frames with tracks: " << tracks << " / 600\n";
        test("Radar should detect threats", detections > 0);
        test("Radar should create tracks", tracks > 0);
        
        if (!engine.tracks().empty()) {
            const auto& t = engine.tracks()[0];
            std::cout << "  Sample track - ID: " << t.id << " | Range: " << t.range_m / 1000.f 
                      << " km | Confidence: " << t.confidence * 100.f << "%\n";
            test("Track should have confidence > 0", t.confidence > 0);
        }
    }

    // ===== RADAR TRACK ID MATCHING =====
    print_header("Track ID Matching");
    {
        air_defense::ThreatWorld threat_world;
        radar::RadarEngine::Config cfg;
        cfg.max_range_m = 500000.f;
        cfg.beam_width_deg = 30.f;
        radar::RadarEngine engine(cfg);
        engine.set_sensor(&threat_world);

        // Simulate until we have a track
        bool found_match = false;
        for (int frame = 0; frame < 1000 && !found_match; frame++) {
            threat_world.tick(0.016f);
            engine.tick(0.016f);
            
            for (const auto& track : engine.tracks()) {
                if (track.confidence > 0.5f) {
                    // Check if track ID matches any threat ID
                    if (threat_world.threat_by_id(track.id)) {
                        std::cout << "  Matched: Track ID " << track.id << " → Threat ID " << track.id << "\n";
                        found_match = true;
                        break;
                    }
                }
            }
        }
        test("Track IDs should match threat IDs", found_match);
    }

    // ===== BATTERY + INTERCEPTOR INTEGRATION =====
    print_header("Battery Firing System");
    {
        air_defense::BatteryController battery;
        air_defense::InterceptorManager mgr;
        
        battery.handle_target_selection(1000);
        test("Battery should select target", battery.selected_target_id() == 1000);
        test("Battery should be ready to fire", battery.can_fire());
        
        // Simulate firing
        int initial_fired = mgr.total_fired;
        for (int i = 0; i < 10; i++) {
            if (battery.can_fire()) {
                mgr.fire_interceptor(0.f, 0.f, 10000.f + i * 1000.f, 0.f, 0.f, 0.f);
            }
        }
        test("Missiles should be fired", mgr.total_fired > initial_fired);
        
        std::cout << "  Total missiles fired: " << mgr.total_fired << "\n";
        test("Multiple missiles should be created", mgr.interceptors().size() >= 10);
    }

    // ===== COLLISION INTEGRATION =====
    print_header("Collision System");
    {
        air_defense::ThreatWorld threat_world;
        air_defense::InterceptorManager mgr;
        
        // Create a threat
        auto& threats = threat_world.threats_mut();
        air_defense::Threat t;
        t.id = 1000;
        t.x_m = 1000.f;
        t.y_m = 0.f;
        t.vx_mps = -100.f;
        t.vy_mps = 0.f;
        t.rcs_m2 = 1.0f;
        t.destroyed = false;
        t.explosion_timer = 0.f;
        threats.push_back(t);

        // Fire missile at it
        mgr.fire_interceptor(0.f, 0.f, t.x_m, t.y_m, t.vx_mps, t.vy_mps);
        test("Missile should be created", mgr.interceptors().size() == 1);

        // Simulate collision
        bool hit = false;
        for (int frame = 0; frame < 5000; frame++) {
            threat_world.tick(0.016f);
            mgr.tick(0.016f);

            auto& missile = mgr.interceptors()[0];
            auto& threat = threats[0];
            
            float dx = missile.x_m - threat.x_m;
            float dy = missile.y_m - threat.y_m;
            float dist = std::hypot(dx, dy);
            
            if (dist < mgr.blast_radius_m && !missile.destroyed && !threat.destroyed) {
                missile.destroyed = true;
                threat.destroyed = true;
                hit = true;
                std::cout << "  Collision at frame " << frame << " | Distance: " << dist << " m\n";
                break;
            }
        }
        test("Collision should occur", hit);
    }

    // ===== THREAT REACHING BATTERY =====
    print_header("Threat Reaching Battery");
    {
        air_defense::ThreatWorld threat_world;
        air_defense::BatteryController battery;
        
        auto& threats = threat_world.threats_mut();
        air_defense::Threat t;
        t.id = 1000;
        t.x_m = 5000.f;  // 5 km away
        t.y_m = 0.f;
        t.vx_mps = -1000.f;  // Moving toward origin
        t.vy_mps = 0.f;
        t.rcs_m2 = 1.0f;
        t.destroyed = false;
        t.explosion_timer = 0.f;
        threats.push_back(t);

        int health_before = battery.battery_health();
        
        // Simulate threat reaching battery
        bool reached = false;
        for (int frame = 0; frame < 10000; frame++) {
            threat_world.tick(0.016f);
            
            float range = std::hypot(t.x_m, t.y_m);
            if (range < 2000.f && !t.destroyed) {
                t.destroyed = true;
                battery.take_damage(battery.damage_per_hit);
                reached = true;
                std::cout << "  Threat reached at frame " << frame << " | Range: " << range << " m\n";
                break;
            }
        }
        
        test("Threat should reach battery", reached);
        test("Battery should take damage", battery.battery_health() < health_before);
        std::cout << "  Battery health: " << health_before << " → " << battery.battery_health() << "\n";
    }

    // ===== PRINT RESULTS =====
    std::cout << "\n" << std::string(80, '=') << "\n";
    std::cout << "INTEGRATION TEST RESULTS\n";
    std::cout << std::string(80, '=') << "\n";
    std::cout << "Passed: " << passed << "\n";
    std::cout << "Failed: " << failed << "\n";
    std::cout << "Total:  " << (passed + failed) << "\n";

    return failed == 0 ? 0 : 1;
}
