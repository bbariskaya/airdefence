#include "battery_controller.hpp"
#include "game_config.hpp"
#include "game_hud.hpp"
#include "game_simulator.hpp"
#include "interceptor.hpp"
#include "threat_world.hpp"
#include "radar/radar_engine.hpp"
#include "raylib.h"
#include <deque>
#include <string>
#include <vector>

namespace {
constexpr int kW = 1280;
constexpr int kH = 800;

struct UIState {
    bool paused = false;
    bool game_over = false;
    float game_over_timer = 0.f;
    float fire_cooldown = 0.f;
};

radar::RadarEngine::Config make_radar_config() {
    radar::RadarEngine::Config cfg;
    cfg.max_range_m = air_defense::GameConfig::kMaxRadarRangeM;
    cfg.sweep_deg_per_sec = 90.f;
    cfg.omnidirectional_search = true;
    cfg.detection_threshold = 0.12f;
    return cfg;
}

} // namespace

int main() {
    SetConfigFlags(FLAG_MSAA_4X_HINT | FLAG_WINDOW_HIGHDPI);
    InitWindow(kW, kH, "Iron Dome Battery - Air Defense Game");
    SetTargetFPS(60);

    air_defense::ThreatWorld threat_world;
    radar::RadarEngine::Config cfg = make_radar_config();
    radar::RadarEngine engine(cfg);
    engine.set_sensor(&threat_world);

    air_defense::InterceptorManager interceptor_mgr;
    air_defense::BatteryController battery;
    air_defense::GameHUD hud;

    UIState ui_state;
    std::vector<float> blip_ages;

    while (!WindowShouldClose()) {
        if (IsKeyPressed(KEY_ESCAPE)) break;
        if (IsKeyPressed(KEY_R)) {
            threat_world = air_defense::ThreatWorld();
            engine = radar::RadarEngine(cfg);
            engine.set_sensor(&threat_world);
            interceptor_mgr = air_defense::InterceptorManager();
            battery.reset();
            ui_state = {};
            blip_ages.clear();
        }
        if (IsKeyPressed(KEY_P)) {
            ui_state.paused = !ui_state.paused;
        }

        float dt = GetFrameTime() * air_defense::GameConfig::kSimTimeScale;
        if (!ui_state.paused && !ui_state.game_over) {
            auto step = air_defense::step_simulation(dt, threat_world, engine, interceptor_mgr,
                                                     battery, ui_state.fire_cooldown, true);
            if (step.game_over) {
                ui_state.game_over = true;
                ui_state.game_over_timer = 0.f;
            }

            blip_ages.clear();
            for (const auto& d : engine.last_detections()) {
                (void)d;
                blip_ages.push_back(0.f);
            }
            for (auto& age : blip_ages) {
                age += dt;
            }
        } else if (ui_state.game_over) {
            ui_state.game_over_timer += dt;
        }

        BeginDrawing();
        ClearBackground({4, 8, 6, 255});

        DrawText("Iron Dome | 3D Ballistic Intercept | 5x sim | fire 0.15s | spawn every 3s max 20",
                 16, 16, 18, {80, 200, 120, 255});

        hud.render(engine, threat_world, interceptor_mgr, battery, blip_ages);

        if (ui_state.game_over) {
            DrawRectangle(0, 0, kW, kH, {0, 0, 0, 200});
            DrawText("BATTERY DESTROYED", kW / 2 - 180, kH / 2 - 40, 48, {255, 80, 80, 255});
            DrawText(TextFormat("Intercepted: %d threats", battery.total_intercepted),
                     kW / 2 - 120, kH / 2 + 40, 24, {200, 200, 200, 255});
            DrawText(TextFormat("Reached battery: %d threats", battery.total_threats_reached),
                     kW / 2 - 150, kH / 2 + 80, 24, {200, 200, 200, 255});
            DrawText("Press R to restart or ESC to quit", kW / 2 - 200, kH / 2 + 140, 20, {150, 150, 150, 255});
        }

        if (ui_state.paused && !ui_state.game_over) {
            DrawText("PAUSED", kW / 2 - 50, 80, 32, ORANGE);
            DrawText("Press P to resume", kW / 2 - 100, 120, 20, ORANGE);
        }

        EndDrawing();
    }

    CloseWindow();
    return 0;
}
