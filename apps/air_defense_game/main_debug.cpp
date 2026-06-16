#include "battery_controller.hpp"
#include "game_config.hpp"
#include "game_simulator.hpp"
#include "interceptor.hpp"
#include "threat_world.hpp"
#include "radar/radar_engine.hpp"
#include <cmath>
#include <iomanip>
#include <iostream>
#include <string>

namespace {

radar::RadarEngine::Config make_radar_config() {
    radar::RadarEngine::Config cfg;
    cfg.max_range_m = air_defense::GameConfig::kMaxRadarRangeM;
    cfg.sweep_deg_per_sec = 90.f;
    cfg.omnidirectional_search = true;
    cfg.detection_threshold = 0.12f;
    return cfg;
}

void print_event(const air_defense::GameEvent& ev) {
    switch (ev.type) {
    case air_defense::GameEvent::Fired:
        std::cout << "[FIRE] Missile " << ev.missile_id << " launched at threat " << ev.threat_id
                  << " | range " << std::fixed << std::setprecision(0) << ev.range_m / 1000.f
                  << " km\n";
        break;
    case air_defense::GameEvent::Intercept:
        std::cout << "[KILL] Threat " << ev.threat_id << " destroyed by missile " << ev.missile_id
                  << " at " << ev.range_m / 1000.f << " km\n";
        break;
    case air_defense::GameEvent::ThreatReached:
        std::cout << "[HIT]  Threat " << ev.threat_id << " reached battery kill zone!\n";
        break;
    default:
        break;
    }
}

void print_status(int frame, const air_defense::ThreatWorld& world,
                  const radar::RadarEngine& engine,
                  const air_defense::InterceptorManager& mgr,
                  const air_defense::BatteryController& battery) {
    std::cout << "\n" << std::string(100, '-') << "\n";
    std::cout << "FRAME " << frame << " | health " << battery.battery_health()
              << " | active threats " << world.active_threat_count()
              << " | tracks " << engine.tracks().size()
              << " | missiles in flight " << mgr.interceptors().size()
              << " | kills " << battery.total_intercepted
              << " | leaks " << battery.total_threats_reached << "\n";

    if (engine.last_detections().empty() && world.active_threat_count() > 0) {
        std::cout << "  WARNING: threats present but radar returned zero detections this frame\n";
    }

    for (const auto& t : engine.tracks()) {
        if (t.confidence < 0.2f) continue;
        const bool engaged = air_defense::has_active_interceptor_for_target(mgr, t.id);
        std::cout << "  TRACK " << t.callsign << " id=" << t.id
                  << " range=" << t.range_m / 1000.f << "km"
                  << " conf=" << std::setprecision(0) << t.confidence * 100.f << "%"
                  << (engaged ? " [ENGAGED]" : " [UNENGAGED]") << "\n";
    }

    for (const auto& m : mgr.interceptors()) {
        if (!m.is_alive()) continue;
        std::cout << "  MISSILE id=" << m.id << " -> target " << m.target_id
                  << " pos=(" << m.x_m / 1000.f << "km," << m.y_m / 1000.f << "km)"
                  << " t=" << std::setprecision(1) << m.lifetime_sec << "s\n";
    }
}

} // namespace

int main() {
    std::cout << "AIR DEFENSE — CONSOLE DEBUG (no GUI)\n";
    std::cout << "Long-range search radar + quadratic lead intercept + one missile per threat\n\n";

    air_defense::ThreatWorld threat_world;
    radar::RadarEngine::Config cfg = make_radar_config();
    radar::RadarEngine engine(cfg);
    engine.set_sensor(&threat_world);

    std::cout << "[CONFIG] radar range=" << cfg.max_range_m / 1000.f << " km"
              << " | spawn every " << threat_world.spawn_interval_sec << " s"
              << " | max active=" << threat_world.max_active_threats << "\n\n";

    air_defense::InterceptorManager interceptor_mgr;
    air_defense::BatteryController battery;
    float fire_cooldown = 0.f;

    constexpr float dt = 0.016f * air_defense::GameConfig::kSimTimeScale;
    int frame = 0;

    while (battery.battery_health() > 0.f && frame < 15000) {
        ++frame;
        auto step = air_defense::step_simulation(dt, threat_world, engine, interceptor_mgr,
                                                 battery, fire_cooldown, true);

        for (const auto& ev : step.events) {
            print_event(ev);
        }

        if (frame % 120 == 0) {
            print_status(frame, threat_world, engine, interceptor_mgr, battery);
        }

        if (step.game_over) break;
    }

    std::cout << "\n" << std::string(100, '=') << "\nFINAL REPORT\n";
    std::cout << "Frames: " << frame << " (" << frame * dt << " s simulated)\n";
    std::cout << "Missiles fired: " << interceptor_mgr.total_fired << "\n";
    std::cout << "Intercepted: " << battery.total_intercepted << "\n";
    std::cout << "Reached battery: " << battery.total_threats_reached << "\n";
    std::cout << "Final health: " << battery.battery_health() << "\n";
    std::cout << std::string(100, '=') << "\n";

    return battery.battery_health() > 0.f ? 0 : 1;
}
