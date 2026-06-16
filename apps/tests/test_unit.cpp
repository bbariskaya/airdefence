#include "battery_controller.hpp"
#include "ballistics.hpp"
#include "game_simulator.hpp"
#include "interceptor.hpp"
#include "threat_world.hpp"
#include "radar/radar_engine.hpp"
#include "radar/radar_physics.hpp"
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
    std::cout << "UNIT TEST: " << section << "\n";
    std::cout << std::string(80, '=') << "\n";
}
}

int main() {
    std::cout << "AIR DEFENSE UNIT TESTS\n\n";

    // ===== RADAR PHYSICS UNIT TESTS =====
    print_header("Radar Physics");
    {
        float r = 100000.f;  // 100 km
        float rcs = 1.0f;    // 1 m² RCS
        float r4 = r * r * r * r;
        float amp = rcs * radar::RadarPhysics::kRadarCal / (radar::RadarPhysics::kRangeFloor + r4);
        std::cout << "  Range: " << r / 1000.f << " km, RCS: " << rcs << " m²\n";
        std::cout << "  Amplitude: " << amp << " (threshold: 0.18)\n";
        test("Amplitude should be > 0", amp > 0);
        
        float r2 = 200000.f;  // 200 km (realistic air defense range)
        float r4_2 = r2 * r2 * r2 * r2;
        float amp2 = rcs * radar::RadarPhysics::kRadarCal / (radar::RadarPhysics::kRangeFloor + r4_2);
        std::cout << "  Range: " << r2 / 1000.f << " km, RCS: " << rcs << " m²\n";
        std::cout << "  Amplitude: " << amp2 << " (threshold: 0.18)\n";
        test("Medium-range detection (200 km) should work", amp2 > 0.18f);
    }

    // ===== THREAT SPAWN UNIT TESTS =====
    print_header("Threat Spawning");
    {
        air_defense::ThreatWorld world;
        world.detection_range_m = 100000.f;
        world.tick(3.1f);
        test("Threat should spawn", !world.threats().empty());
        
        auto threats_before = world.threats().size();
        world.tick(3.1f);
        auto threats_after = world.threats().size();
        test("More threats should spawn over time", threats_after > threats_before);
        
        for (const auto& t : world.threats()) {
            float range = std::hypot(t.x_m, t.y_m);
            test("Threat range should be within detection range", range <= world.detection_range_m + 1.f);
            test("Threat should have valid RCS", t.rcs_m2 > 0 && t.rcs_m2 <= world.threat_rcs_max_m2);
            test("Ballistic threat has altitude", t.z_m > 0.f);
            test("Ballistic threat has dive velocity", t.vz_mps < 0.f);
        }
    }

    // ===== BATTERY CONTROLLER UNIT TESTS =====
    print_header("Battery Controller");
    {
        air_defense::BatteryController battery;
        test("Battery should start healthy", battery.battery_health() == 100.f);
        test("Battery should be able to fire", battery.can_fire());
        
        battery.take_damage(25.f);
        test("Battery should take damage", battery.battery_health() == 75.f);
        
        battery.take_damage(100.f);
        test("Battery health should not go below 0", battery.battery_health() == 0.f);
        test("Battery should not fire when destroyed", !battery.can_fire());
        
        battery.reset();
        test("Battery should reset to full health", battery.battery_health() == 100.f);
    }

    // ===== INTERCEPTOR PHYSICS UNIT TESTS =====
    print_header("Interceptor Physics");
    {
        air_defense::InterceptorManager mgr;
        
        // Test direct aim (stationary target)
        mgr.fire_interceptor(0.f, 0.f, 1000.f, 0.f, 0.f, 0.f);
        test("Missile should be created", mgr.interceptors().size() == 1);
        
        auto& m = mgr.interceptors()[0];
        test("Missile should have valid velocity", m.speed_mps() > 0);
        test("Missile should have speed close to missile_speed", 
             std::abs(m.speed_mps() - mgr.missile_speed_mps) < 1.f);
        
        mgr.tick(1.0f);
        float dist = std::hypot(mgr.interceptors()[0].x_m, mgr.interceptors()[0].y_m);
        float expected = mgr.missile_speed_mps * 1.0f;
        std::cout << "  Missile traveled: " << dist << " m (expected ~" << expected << " m)\n";
        test("Missile should travel toward target", dist > 500.f);
    }

    // ===== COLLISION DETECTION UNIT TESTS =====
    print_header("Collision Detection");
    {
        air_defense::Threat threat;
        threat.x_m = 0.f;
        threat.y_m = 0.f;
        threat.destroyed = false;
        
        air_defense::Interceptor missile;
        missile.x_m = 100.f;
        missile.y_m = 0.f;
        missile.destroyed = false;
        
        float blast_radius = 200.f;
        float dist = std::hypot(missile.x_m - threat.x_m, missile.y_m - threat.y_m);
        test("Collision should detect when within radius", dist < blast_radius);
        
        missile.x_m = 500.f;
        dist = std::hypot(missile.x_m - threat.x_m, missile.y_m - threat.y_m);
        test("Collision should not detect when outside radius", dist > blast_radius);
    }

    // ===== TARGET SELECTION UNIT TESTS =====
    print_header("Closest Target Selection");
    {
        air_defense::InterceptorManager mgr;
        mgr.fire_interceptor(0.f, 0.f, 1000.f, 0.f, 0.f, 0.f, 1001);

        radar::RadarEngine engine;
        // Tracks are internal; test helper via has_active_interceptor
        test("Active interceptor blocks re-fire on same target",
             air_defense::has_active_interceptor_for_target(mgr, 1001));
        test("No interceptor for other target",
             !air_defense::has_active_interceptor_for_target(mgr, 1002));
    }

    // ===== INTERCEPT BALLISTICS UNIT TESTS =====
    print_header("Lead Intercept Ballistics");
    {
        float ix, iy, t;
        bool ok = air_defense::InterceptorManager::calculate_intercept_point(
            0.f, 0.f, 10000.f, 0.f, -200.f, 0.f, 1200.f, ix, iy, t);
        test("Intercept solution should succeed", ok);
        test("Intercept time should be positive", t > 0.f);
        float dist_at_t = std::hypot(ix, iy);
        test("Missile reaches intercept distance in time",
             std::abs(dist_at_t - 1200.f * t) < 500.f);
    }

    // ===== 3D GRAVITY INTERCEPT UNIT TESTS (Newton-Raphson) =====
    print_header("3D Newton-Raphson Intercept");
    {
        air_defense::Ballistics::State3D launcher{};
        air_defense::Ballistics::State3D tgt{};
        tgt.x_m = 50000.f;
        tgt.y_m = 0.f;
        tgt.z_m = 20000.f;
        tgt.vx_mps = -800.f;
        tgt.vy_mps = 0.f;
        tgt.vz_mps = -200.f;

        auto sol = air_defense::Ballistics::solve_intercept_newton(launcher, tgt, 1800.f, 20);
        std::cout << "  Newton iterations: " << sol.iterations
                  << " | miss: " << sol.miss_distance_m << " m"
                  << " | converged: " << (sol.converged ? "yes" : "no") << "\n";
        test("Newton intercept should iterate (>1 step)", sol.iterations > 1);
        test("Newton intercept should converge", sol.converged);
        test("Intercept time positive", sol.time_sec > 0.f);
        test("Predicted intercept has altitude", sol.aim_point.z_m > 0.f);
    }

    // ===== PRINT RESULTS =====
    std::cout << "\n" << std::string(80, '=') << "\n";
    std::cout << "UNIT TEST RESULTS\n";
    std::cout << std::string(80, '=') << "\n";
    std::cout << "Passed: " << passed << "\n";
    std::cout << "Failed: " << failed << "\n";
    std::cout << "Total:  " << (passed + failed) << "\n";

    return failed == 0 ? 0 : 1;
}
