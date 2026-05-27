#include "game_sensor.hpp"
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
constexpr float kMapScale = 0.0042f;
constexpr float kPpiR = 175.f;
constexpr float kMaxRange = 100000.f;

struct Blip {
    float range_m, az_deg, amp, age;
};

static Color phosphor() { return {80, 255, 120, 255}; }
static Color dim() { return {28, 72, 48, 255}; }
static Color hudBg() { return {10, 16, 12, 255}; }

static float norm_deg(float d) {
    while (d < 0.f) d += 360.f;
    while (d >= 360.f) d -= 360.f;
    return d;
}

static Vector2 polar_px(float cx, float cy, float range_m, float az_deg) {
    float r = (range_m / kMaxRange) * kPpiR;
    float rad = (az_deg - 90.f) * 3.14159265f / 180.f;
    return {cx + r * std::cos(rad), cy + r * std::sin(rad)};
}

static void draw_aircraft_triangle(float px, float py, float heading_deg, Color fill) {
    float rad = (heading_deg - 90.f) * 3.14159265f / 180.f;
    Vector2 nose = {px + std::cos(rad) * 10.f, py + std::sin(rad) * 10.f};
    Vector2 left = {px + std::cos(rad + 2.4f) * 7.f, py + std::sin(rad + 2.4f) * 7.f};
    Vector2 right = {px + std::cos(rad - 2.4f) * 7.f, py + std::sin(rad - 2.4f) * 7.f};
    DrawTriangle(nose, left, right, fill);
}

static void draw_tactical_map(const game::GameSensor& sensor, float radar_az) {
    const int mx = 20, my = 60, mw = 600, mh = 600;
    DrawRectangle(mx, my, mw, mh, {8, 14, 10, 255});
    DrawRectangleLines(mx, my, mw, mh, dim());

    const float cx = mx + mw * 0.5f;
    const float cy = my + mh * 0.5f;

    for (int i = -5; i <= 5; ++i) {
        DrawLine(mx, static_cast<int>(cy + i * 55), mx + mw, static_cast<int>(cy + i * 55), {14, 32, 22, 255});
        DrawLine(static_cast<int>(cx + i * 55), my, static_cast<int>(cx + i * 55), my + mh, {14, 32, 22, 255});
    }

    DrawCircle(static_cast<int>(cx), static_cast<int>(cy), 6, phosphor());
    DrawText("RADAR", static_cast<int>(cx) + 10, static_cast<int>(cy) - 6, 12, dim());

    float sweep_rad = (radar_az - 90.f) * 3.14159265f / 180.f;
    DrawLineEx({cx, cy},
               {cx + std::cos(sweep_rad) * 280.f, cy + std::sin(sweep_rad) * 280.f},
               2.f, {80, 255, 120, 80});

    for (const auto& ac : sensor.world.truth_snapshot()) {
        float px = cx + ac.x_m * kMapScale;
        float py = cy - ac.y_m * kMapScale;
        float hdg = norm_deg(std::atan2(ac.vx_mps, ac.vy_mps) * 57.2958f);
        draw_aircraft_triangle(px, py, hdg, {240, 70, 70, 255});
        DrawText(ac.callsign.c_str(), static_cast<int>(px) + 12, static_cast<int>(py) - 8, 11, {200, 120, 120, 255});
    }

    const auto& pl = sensor.player;
    float ppx = cx + pl.x_m * kMapScale;
    float ppy = cy - pl.y_m * kMapScale;
    draw_aircraft_triangle(ppx, ppy, pl.heading_deg, {80, 160, 255, 255});
    DrawText("YOU", static_cast<int>(ppx) + 12, static_cast<int>(ppy) - 8, 12, {120, 180, 255, 255});

    DrawText("TACTICAL MAP", mx + 8, my + 8, 16, phosphor());
}

static void draw_ppi(const radar::RadarEngine& engine,
                     const std::vector<Blip>& blips,
                     std::deque<float>& trail) {
    const int px = 640, py = 60, pw = 380, ph = 380;
    const float cx = px + pw * 0.5f;
    const float cy = py + ph * 0.5f;

    DrawRectangle(px, py, pw, ph, {5, 10, 7, 255});
    DrawRectangleLines(px, py, pw, ph, dim());

    for (int ring = 1; ring <= 4; ++ring) {
        float rr = kPpiR * ring / 4.f;
        DrawCircleLines(static_cast<int>(cx), static_cast<int>(cy), rr, dim());
        DrawText(TextFormat("%.0f", 25.f * ring), static_cast<int>(cx) + 4,
                 static_cast<int>(cy - rr - 10), 11, dim());
    }
    DrawCircleLines(static_cast<int>(cx), static_cast<int>(cy), kPpiR, phosphor());

    trail.push_back(engine.sweep_azimuth_deg());
    if (trail.size() > 50) trail.pop_front();
    int ti = 0;
    for (float az : trail) {
        float a = static_cast<float>(ti) / static_cast<float>(trail.size());
        float rad = (az - 90.f) * 3.14159265f / 180.f;
        DrawLineEx({cx, cy},
                   {cx + kPpiR * std::cos(rad), cy + kPpiR * std::sin(rad)},
                   1.5f, {80, 255, 120, static_cast<unsigned char>(30 + a * 120)});
        ++ti;
    }

    float srad = (engine.sweep_azimuth_deg() - 90.f) * 3.14159265f / 180.f;
    DrawLineEx({cx, cy},
               {cx + kPpiR * std::cos(srad), cy + kPpiR * std::sin(srad)},
               3.f, phosphor());

    for (const auto& b : blips) {
        Vector2 p = polar_px(cx, cy, b.range_m, b.az_deg);
        unsigned char a = static_cast<unsigned char>((1.f - b.age / 2.5f) * 220.f * b.amp);
        DrawCircleV(p, 4.f + b.amp * 5.f, {100, 255, 150, a});
    }

    for (const auto& t : engine.tracks()) {
        if (t.confidence < 0.2f) continue;
        Vector2 p = polar_px(cx, cy, t.range_m, t.azimuth_deg);
        DrawCircleLinesV(p, 11.f, {255, 100, 100, 255});
        DrawText(t.callsign.c_str(), static_cast<int>(p.x) + 12, static_cast<int>(p.y) - 6, 12, phosphor());
    }

    DrawText("PPI RADAR", px + 8, py + 8, 16, phosphor());
    DrawText(TextFormat("AZ %.0f", engine.sweep_azimuth_deg()), px + 8, py + ph - 22, 14, dim());
}

static void draw_hud(const radar::RadarEngine& engine, const game::PlayerAircraft& pl, bool paused) {
    const int x = 1040, y = 60, w = 220, h = 700;
    DrawRectangle(x, y, w, h, hudBg());
    DrawRectangleLines(x, y, w, h, dim());

    int ty = y + 12;
    DrawText("TRACKS", x + 10, ty, 18, phosphor());
    ty += 28;
    for (const auto& t : engine.tracks()) {
        DrawText(TextFormat("%s", t.callsign.c_str()), x + 10, ty, 14, {255, 140, 140, 255});
        ty += 16;
        DrawText(TextFormat("%.0f km  BRG %03.0f", t.range_m / 1000.f, t.azimuth_deg),
                 x + 10, ty, 12, dim());
        ty += 20;
    }
    ty += 10;
    DrawText("YOUR SHIP", x + 10, ty, 16, {120, 180, 255, 255});
    ty += 22;
    DrawText(TextFormat("SPD %.0f m/s", std::hypot(pl.vx_mps, pl.vy_mps)), x + 10, ty, 12, dim());
    ty += 18;
    DrawText(TextFormat("HDG %.0f", pl.heading_deg), x + 10, ty, 12, dim());
    ty += 30;
    DrawText(paused ? "PAUSED" : "LIVE", x + 10, ty, 14, paused ? ORANGE : GREEN);
    ty += 28;
    DrawText("W/S thrust  A/D turn", x + 10, ty, 11, dim());
    ty += 16;
    DrawText("SPACE pause  R reset", x + 10, ty, 11, dim());
    ty += 16;
    DrawText("ESC quit", x + 10, ty, 11, dim());
}
} // namespace

int main() {
    SetConfigFlags(FLAG_MSAA_4X_HINT | FLAG_WINDOW_HIGHDPI);
    InitWindow(kW, kH, "RadarSim - Fly + PPI Radar (C++)");
    SetTargetFPS(60);

    game::GameSensor sensor;
    radar::RadarEngine::Config cfg;
    cfg.max_range_m = kMaxRange;
    cfg.sweep_deg_per_sec = 60.f;
    radar::RadarEngine engine(cfg);
    engine.set_sensor(&sensor);
    engine.ignore_target(sensor.player.id);

    std::deque<float> sweep_trail;
    std::vector<Blip> blips;
    bool paused = false;

    while (!WindowShouldClose()) {
        if (IsKeyPressed(KEY_ESCAPE)) break;

        float thrust = 0.f, turn = 0.f;
        if (IsKeyDown(KEY_W) || IsKeyDown(KEY_UP)) thrust = 1.f;
        if (IsKeyDown(KEY_S) || IsKeyDown(KEY_DOWN)) thrust = -0.5f;
        if (IsKeyDown(KEY_A) || IsKeyDown(KEY_LEFT)) turn = -1.f;
        if (IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT)) turn = 1.f;
        if (IsKeyPressed(KEY_SPACE)) paused = !paused;
        if (IsKeyPressed(KEY_R)) {
            engine = radar::RadarEngine(cfg);
            engine.set_sensor(&sensor);
            engine.ignore_target(sensor.player.id);
            blips.clear();
            sweep_trail.clear();
        }

        float dt = GetFrameTime();
        if (!paused) {
            game::update_player(sensor.player, dt, thrust, turn);
            engine.tick(dt);
            for (const auto& d : engine.last_detections()) {
                blips.push_back({d.range_m, d.azimuth_deg, d.amplitude, 0.f});
            }
        }

        for (auto& b : blips) b.age += dt;
        blips.erase(std::remove_if(blips.begin(), blips.end(),
                                   [](const Blip& b) { return b.age > 2.5f; }),
                    blips.end());

        BeginDrawing();
        ClearBackground({4, 8, 6, 255});

        DrawText("RadarSim | monostatic sigma/R^4 | sensor-ready core", 16, 16, 18, dim());

        draw_tactical_map(sensor, engine.sweep_azimuth_deg());
        draw_ppi(engine, blips, sweep_trail);
        draw_hud(engine, sensor.player, paused);

        EndDrawing();
    }

    CloseWindow();
    return 0;
}
