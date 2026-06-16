#include "battery_controller.hpp"
#include "game_config.hpp"
#include "game_simulator.hpp"
#include "interceptor.hpp"
#include "threat_world.hpp"
#include "radar/radar_engine.hpp"
#include <cmath>
#include <iostream>
#include <string>

namespace {
int passed = 0;
int failed = 0;

void test(const std::string& name, bool condition) {
    if (condition) {
        std::cout << "  ✓ " << name << "\n";
        ++passed;
    } else {
        std::cout << "  ✗ FAILED: " << name << "\n";
        ++failed;
    }
}

void print_header(const std::string& section) {
    std::cout << "\n" << std::string(80, '=') << "\n";
    std::cout << "SYSTEM TEST: " << section << "\n";
    std::cout << std::string(80, '=') << "\n";
}

radar::RadarEngine::Config make_cfg() {
    radar::RadarEngine::Config cfg;
    cfg.max_range_m = air_defense::GameConfig::kMaxRadarRangeM;
    cfg.omnidirectional_search = true;
    cfg.detection_threshold = 0.12f;
    return cfg;
}

} // namespace

int main() {
    std::cout << "AIR DEFENSE SYSTEM TESTS (end-to-end, no GUI)\n\n";

    // Full pipeline: spawn -> detect -> fire closest -> intercept
    print_header("Full Auto-Defense Pipeline");
    {
        air_defense::ThreatWorld world;
        world.spawn_interval_sec = 1.f;
        world.max_active_threats = 2;
        world.detection_range_m = 80000.f;

        radar::RadarEngine engine(make_cfg());
        engine.set_sensor(&world);

        air_defense::InterceptorManager mgr;
        air_defense::BatteryController battery;
        float cooldown = 0.f;

        int fired = 0;
        int kills = 0;

        for (int frame = 0; frame < 8000; ++frame) {
            auto step = air_defense::step_simulation(0.016f, world, engine, mgr, battery, cooldown, true);
            for (const auto& ev : step.events) {
                if (ev.type == air_defense::GameEvent::Fired) ++fired;
                if (ev.type == air_defense::GameEvent::Intercept) ++kills;
            }
            if (kills >= 1) break;
        }

        std::cout << "  Missiles fired: " << fired << " | Kills: " << kills << "\n";
        test("Radar should detect and system should fire", fired >= 1);
        test("System should achieve at least one intercept", kills >= 1);
        test("Battery should survive first intercept scenario", battery.battery_health() > 0.f);
    }

    // Closest-target policy: nearer threat gets missile first
    print_header("Closest Target Priority");
    {
        air_defense::ThreatWorld world;
        world.spawn_interval_sec = 999.f;
        world.max_active_threats = 5;

        auto& threats = world.threats_mut();
        air_defense::Threat far_t{};
        far_t.id = 1001;
        far_t.x_m = 200000.f;
        far_t.y_m = 0.f;
        far_t.vx_mps = -400.f;
        far_t.vy_mps = 0.f;
        far_t.rcs_m2 = 2.f;
        far_t.callsign = "FAR";
        threats.push_back(far_t);

        air_defense::Threat near_t{};
        near_t.id = 1002;
        near_t.x_m = 50000.f;
        near_t.y_m = 0.f;
        near_t.vx_mps = -400.f;
        near_t.vy_mps = 0.f;
        near_t.rcs_m2 = 2.f;
        near_t.callsign = "NEAR";
        threats.push_back(near_t);

        radar::RadarEngine engine(make_cfg());
        engine.set_sensor(&world);
        air_defense::InterceptorManager mgr;
        air_defense::BatteryController battery;
        float cooldown = 0.f;

        std::int32_t first_target = -1;
        for (int frame = 0; frame < 300; ++frame) {
            auto step = air_defense::step_simulation(0.016f, world, engine, mgr, battery, cooldown, true);
            for (const auto& ev : step.events) {
                if (ev.type == air_defense::GameEvent::Fired && first_target < 0) {
                    first_target = ev.threat_id;
                }
            }
            if (first_target >= 0) break;
        }

        std::cout << "  First fired at threat id: " << first_target << " (expected near=1002)\n";
        test("First engagement should target closest threat", first_target == 1002);
    }

    // One missile per threat (no spam along same line)
    print_header("One Missile Per Threat");
    {
        air_defense::ThreatWorld world;
        world.spawn_interval_sec = 999.f;

        auto& threats = world.threats_mut();
        air_defense::Threat t{};
        t.id = 1000;
        t.x_m = 60000.f;
        t.y_m = 0.f;
        t.vx_mps = -300.f;
        t.vy_mps = 0.f;
        t.rcs_m2 = 2.f;
        t.callsign = "SOLO";
        threats.push_back(t);

        radar::RadarEngine engine(make_cfg());
        engine.set_sensor(&world);
        air_defense::InterceptorManager mgr;
        air_defense::BatteryController battery;
        float cooldown = 0.f;

        int fire_events = 0;
        for (int frame = 0; frame < 200; ++frame) {
            auto step = air_defense::step_simulation(0.016f, world, engine, mgr, battery, cooldown, true);
            for (const auto& ev : step.events) {
                if (ev.type == air_defense::GameEvent::Fired) ++fire_events;
            }
        }

        std::cout << "  Fire events for single threat: " << fire_events << "\n";
        test("Should fire only one missile per threat until engaged", fire_events == 1);
    }

    // Continuous spawn (no low cap in production config)
    print_header("Threat Spawn Rate");
    {
        air_defense::ThreatWorld world;
        world.spawn_interval_sec = 1.f;
        world.max_active_threats = 20;
        for (int i = 0; i < 15; ++i) {
            world.tick(1.f);
        }
        std::cout << "  Active after 15s: " << world.active_threat_count() << "\n";
        test("Multiple threats spawn over time", world.active_threat_count() >= 5);
        test("Active threats respect max cap", world.active_threat_count() <= world.max_active_threats);
    }

    // Explosion state after kill
    print_header("Explosion Visual State");
    {
        air_defense::ThreatWorld world;
        auto& threats = world.threats_mut();
        air_defense::Threat t{};
        t.id = 1000;
        t.x_m = 5000.f;
        t.y_m = 0.f;
        t.vx_mps = -200.f;
        t.vy_mps = 0.f;
        t.rcs_m2 = 2.f;
        threats.push_back(t);

        radar::RadarEngine engine(make_cfg());
        engine.set_sensor(&world);
        air_defense::InterceptorManager mgr;
        air_defense::BatteryController battery;
        float cooldown = 0.f;

        bool had_explosion = false;
        for (int frame = 0; frame < 6000; ++frame) {
            auto step = air_defense::step_simulation(0.016f, world, engine, mgr, battery, cooldown, true);
            for (const auto& th : world.threats()) {
                if (th.destroyed && th.explosion_timer > 0.f && th.explosion_timer < 1.2f) {
                    had_explosion = true;
                }
            }
            if (had_explosion) break;
        }
        test("Destroyed threat should enter explosion animation state", had_explosion);
    }

    std::cout << "\n" << std::string(80, '=') << "\n";
    std::cout << "SYSTEM TEST RESULTS\n";
    std::cout << std::string(80, '=') << "\n";
    std::cout << "Passed: " << passed << "\n";
    std::cout << "Failed: " << failed << "\n";
    std::cout << "Total:  " << (passed + failed) << "\n";

    return failed == 0 ? 0 : 1;
}
