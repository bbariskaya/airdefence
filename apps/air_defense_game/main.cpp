#include "battery_controller.hpp"
#include "game_hud.hpp"
#include "interceptor.hpp"
#include "threat_world.hpp"
#include "radar/radar_engine.hpp"
#include "raylib.h"
#include <algorithm>
#include <cmath>
#include <deque>
#include <string>
#include <vector>

namespace {
constexpr int kW = 1280;
constexpr int kH = 800;
constexpr float kMaxRange = 120000.f;

struct UIState {
    bool paused = false;
    bool game_over = false;
    float game_over_timer = 0.f;
};

/**
 * Check collision between interceptor and threat.
 * Returns true if they're within blast radius.
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

/**
 * Get track position for mouse click target selection.
 */
std::int32_t get_track_at_position(const radar::RadarEngine& engine,
                                    const air_defense::ThreatWorld& world,
                                    float mouse_x, float mouse_y) {
    constexpr float kPpiR = 175.f;
    constexpr int px = 640, py = 60, pw = 380, ph = 380;
    const float cx = px + pw * 0.5f;
    const float cy = py + ph * 0.5f;

    // Check if click is in PPI area
    if (mouse_x < px || mouse_x > px + pw || mouse_y < py || mouse_y > py + ph) {
        return -1;
    }

    // Check tracks
    for (const auto& t : engine.tracks()) {
        if (t.confidence < 0.2f) continue;

        float r = (t.range_m / kMaxRange) * kPpiR;
        float rad = (t.azimuth_deg - 90.f) * 3.14159265f / 180.f;
        float track_px = cx + r * std::cos(rad);
        float track_py = cy + r * std::sin(rad);

        float dx = mouse_x - track_px;
        float dy = mouse_y - track_py;
        if (std::hypot(dx, dy) < 15.f) {
            return t.id;
        }
    }

    return -1;
}

} // namespace

int main() {
    SetConfigFlags(FLAG_MSAA_4X_HINT | FLAG_WINDOW_HIGHDPI);
    InitWindow(kW, kH, "Iron Dome Battery - Air Defense Game");
    SetTargetFPS(60);

    // Initialize game systems
    air_defense::ThreatWorld threat_world;
    radar::RadarEngine::Config cfg;
    cfg.max_range_m = kMaxRange;
    cfg.sweep_deg_per_sec = 60.f;
    radar::RadarEngine engine(cfg);
    engine.set_sensor(&threat_world);

    air_defense::InterceptorManager interceptor_mgr;
    air_defense::BatteryController battery;
    air_defense::GameHUD hud;

    UIState ui_state;
    std::vector<float> blip_ages;

    while (!WindowShouldClose()) {
        // Input handling
        if (IsKeyPressed(KEY_ESCAPE)) break;
        if (IsKeyPressed(KEY_SPACE) && !ui_state.game_over) {
            // Fire interceptor at selected target
            if (battery.selected_target_id() >= 0 && battery.can_fire()) {
                // Find target threat
                air_defense::Threat* target = threat_world.threat_by_id(battery.selected_target_id());
                if (target) {
                    // Fire from battery (origin) at threat position
                    interceptor_mgr.fire_interceptor(0.f, 0.f,
                                                     target->x_m, target->y_m,
                                                     target->vx_mps, target->vy_mps);
                    battery.handle_fire_command();
                }
            }
        }
        if (IsKeyPressed(KEY_R)) {
            // Reset game
            threat_world = air_defense::ThreatWorld();
            engine = radar::RadarEngine(cfg);
            engine.set_sensor(&threat_world);
            interceptor_mgr = air_defense::InterceptorManager();
            battery.reset();
            ui_state.paused = false;
            ui_state.game_over = false;
            ui_state.game_over_timer = 0.f;
            blip_ages.clear();
        }
        if (IsKeyPressed(KEY_P)) {
            ui_state.paused = !ui_state.paused;
        }

        // Mouse click for targeting
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && !ui_state.game_over) {
            std::int32_t track_id = get_track_at_position(engine, threat_world,
                                                          GetMouseX(), GetMouseY());
            if (track_id >= 0) {
                battery.handle_target_selection(track_id);
            }
        }

        // Game update
        float dt = GetFrameTime();
        if (!ui_state.paused && !ui_state.game_over) {
            // Update world
            threat_world.tick(dt);
            engine.tick(dt);
            interceptor_mgr.tick(dt);
            battery.tick(dt);

            // Collect blip ages
            blip_ages.clear();
            for (const auto& d : engine.last_detections()) {
                blip_ages.push_back(0.f);
            }
            for (auto& age : blip_ages) {
                age += dt;
            }

            // Check collisions: interceptors vs threats
            for (auto& missile : interceptor_mgr.interceptors()) {
                if (missile.destroyed) continue;

                for (auto& threat : threat_world.threats_mut()) {
                    if (threat.destroyed) continue;

                    if (check_collision(missile, threat, interceptor_mgr.blast_radius_m)) {
                        missile.destroyed = true;
                        threat.destroyed = true;
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
                        ui_state.game_over = true;
                        ui_state.game_over_timer = 0.f;
                    }
                }
            }
        } else if (ui_state.game_over) {
            ui_state.game_over_timer += dt;
        }

        // Rendering
        BeginDrawing();
        ClearBackground({4, 8, 6, 255});

        DrawText("Iron Dome Battery Simulation | Radar + Physics Engine", 16, 16, 18, {80, 200, 120, 255});

        // Draw game UI
        hud.render(engine, threat_world, interceptor_mgr, battery, blip_ages);

        // Draw game over screen
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
