#include "game_hud.hpp"
#include "raylib.h"
#include <cmath>
#include <string>
#include <algorithm>

namespace air_defense {

namespace {
constexpr float kMaxRange = 120000.f;
constexpr float kPpiR = 175.f;

static Color phosphor() { return {80, 255, 120, 255}; }
static Color dim() { return {28, 72, 48, 255}; }
static Color hudBg() { return {10, 16, 12, 255}; }
static Color threat_color() { return {255, 80, 80, 255}; }
static Color missile_color() { return {100, 200, 255, 255}; }

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

static void draw_triangle(float px, float py, float heading_deg, Color fill) {
    float rad = (heading_deg - 90.f) * 3.14159265f / 180.f;
    Vector2 nose = {px + std::cos(rad) * 8.f, py + std::sin(rad) * 8.f};
    Vector2 left = {px + std::cos(rad + 2.4f) * 5.f, py + std::sin(rad + 2.4f) * 5.f};
    Vector2 right = {px + std::cos(rad - 2.4f) * 5.f, py + std::sin(rad - 2.4f) * 5.f};
    DrawTriangle(nose, left, right, fill);
}
} // namespace

GameHUD::GameHUD() {}

void GameHUD::render(const radar::RadarEngine& engine,
                     const ThreatWorld& world,
                     const InterceptorManager& missiles,
                     const BatteryController& battery,
                     const std::vector<float>& blip_ages) {
    // Update blips with age
    for (size_t i = 0; i < current_blips_.size() && i < blip_ages.size(); ++i) {
        current_blips_[i].age = blip_ages[i];
    }

    // Remove expired blips
    current_blips_.erase(
        std::remove_if(current_blips_.begin(), current_blips_.end(),
                       [](const Blip& b) { return b.age > 2.5f; }),
        current_blips_.end());

    draw_tactical_map(world, engine.sweep_azimuth_deg());
    draw_ppi(engine, current_blips_, missiles);
    draw_status_panel(engine, battery, world);
}

void GameHUD::draw_tactical_map(const ThreatWorld& world, float radar_az) {
    const int mx = 20, my = 60, mw = 600, mh = 600;
    DrawRectangle(mx, my, mw, mh, {8, 14, 10, 255});
    DrawRectangleLines(mx, my, mw, mh, dim());

    const float cx = mx + mw * 0.5f;
    const float cy = my + mh * 0.5f;

    // Grid
    for (int i = -5; i <= 5; ++i) {
        DrawLine(mx, static_cast<int>(cy + i * 55), mx + mw, static_cast<int>(cy + i * 55), {14, 32, 22, 255});
        DrawLine(static_cast<int>(cx + i * 55), my, static_cast<int>(cx + i * 55), my + mh, {14, 32, 22, 255});
    }

    // Battery at origin
    DrawCircle(static_cast<int>(cx), static_cast<int>(cy), 8, {0, 255, 0, 255});
    DrawText("BATTERY", static_cast<int>(cx) + 12, static_cast<int>(cy) - 8, 12, {0, 255, 0, 255});

    // Radar sweep line
    float sweep_rad = (radar_az - 90.f) * 3.14159265f / 180.f;
    DrawLineEx({cx, cy},
               {cx + std::cos(sweep_rad) * 280.f, cy + std::sin(sweep_rad) * 280.f},
               2.f, {80, 255, 120, 80});

    // Draw threats
    constexpr float kMapScale = 0.0042f;
    for (const auto& threat : world.threats()) {
        float px = cx + threat.x_m * kMapScale;
        float py = cy - threat.y_m * kMapScale;
        float hdg = norm_deg(std::atan2(threat.vx_mps, threat.vy_mps) * 57.2958f);
        draw_triangle(px, py, hdg, threat_color());
        DrawText(threat.callsign.c_str(), static_cast<int>(px) + 10, static_cast<int>(py) - 8, 10, threat_color());
    }

    DrawText("TACTICAL MAP", mx + 8, my + 8, 16, phosphor());
}

void GameHUD::draw_ppi(const radar::RadarEngine& engine,
                       const std::vector<Blip>& blips,
                       const InterceptorManager& missiles) {
    const int px = 640, py = 60, pw = 380, ph = 380;
    const float cx = px + pw * 0.5f;
    const float cy = py + ph * 0.5f;

    DrawRectangle(px, py, pw, ph, {5, 10, 7, 255});
    DrawRectangleLines(px, py, pw, ph, dim());

    // Range rings
    for (int ring = 1; ring <= 4; ++ring) {
        float rr = kPpiR * ring / 4.f;
        DrawCircleLines(static_cast<int>(cx), static_cast<int>(cy), rr, dim());
        DrawText(TextFormat("%.0f km", 30.f * ring), static_cast<int>(cx) + 4,
                 static_cast<int>(cy - rr - 10), 11, dim());
    }
    DrawCircleLines(static_cast<int>(cx), static_cast<int>(cy), kPpiR, phosphor());

    // Sweep trail
    sweep_trail_.push_back(engine.sweep_azimuth_deg());
    if (sweep_trail_.size() > 50) sweep_trail_.pop_front();

    int ti = 0;
    for (float az : sweep_trail_) {
        float a = static_cast<float>(ti) / static_cast<float>(sweep_trail_.size());
        float rad = (az - 90.f) * 3.14159265f / 180.f;
        DrawLineEx({cx, cy},
                   {cx + kPpiR * std::cos(rad), cy + kPpiR * std::sin(rad)},
                   1.5f, {80, 255, 120, static_cast<unsigned char>(30 + a * 120)});
        ++ti;
    }

    // Current sweep line
    float srad = (engine.sweep_azimuth_deg() - 90.f) * 3.14159265f / 180.f;
    DrawLineEx({cx, cy},
               {cx + kPpiR * std::cos(srad), cy + kPpiR * std::sin(srad)},
               3.f, phosphor());

    // Draw detection blips
    for (const auto& b : blips) {
        Vector2 p = polar_px(cx, cy, b.range_m, b.az_deg);
        unsigned char a = static_cast<unsigned char>((1.f - b.age / 2.5f) * 220.f * b.amp);
        DrawCircleV(p, 3.f + b.amp * 4.f, {255, 150, 100, a});
    }

    // Draw fused tracks
    for (const auto& t : engine.tracks()) {
        if (t.confidence < 0.2f) continue;
        Vector2 p = polar_px(cx, cy, t.range_m, t.azimuth_deg);
        DrawCircleLinesV(p, 11.f, threat_color());
        DrawText(t.callsign.c_str(), static_cast<int>(p.x) + 12, static_cast<int>(p.y) - 6, 12, phosphor());
    }

    // Draw missile tracks
    for (const auto& missile : missiles.interceptors()) {
        float dist = std::hypot(missile.x_m, missile.y_m);
        if (dist < kMaxRange) {
            float az = norm_deg(std::atan2(missile.x_m, missile.y_m) * 57.2958f);
            Vector2 p = polar_px(cx, cy, dist, az);
            DrawCircleV(p, 2.5f, missile_color());
        }
    }

    DrawText("PPI RADAR", px + 8, py + 8, 16, phosphor());
    DrawText(TextFormat("AZ %.0f", engine.sweep_azimuth_deg()), px + 8, py + ph - 22, 14, dim());
}

void GameHUD::draw_status_panel(const radar::RadarEngine& engine,
                                const BatteryController& battery,
                                const ThreatWorld& world) {
    const int x = 1040, y = 60, w = 220, h = 700;
    DrawRectangle(x, y, w, h, hudBg());
    DrawRectangleLines(x, y, w, h, dim());

    int ty = y + 12;
    
    // Title
    DrawText("BATTERY", x + 10, ty, 18, phosphor());
    ty += 28;

    // Health bar
    DrawText("HEALTH", x + 10, ty, 14, {255, 100, 100, 255});
    ty += 16;
    float health_pct = battery.battery_health() / battery.battery_max_health;
    DrawRectangle(x + 10, ty, 200, 16, {40, 40, 40, 255});
    DrawRectangle(x + 10, ty, static_cast<int>(200.f * health_pct), 16,
                  health_pct > 0.3f ? Color{0, 200, 0, 255} : Color{200, 0, 0, 255});
    DrawRectangleLines(x + 10, ty, 200, 16, dim());
    DrawText(TextFormat("%.0f%%", health_pct * 100.f), x + 120, ty + 2, 12, phosphor());
    ty += 24;

    // Ammo
    DrawText("AMMO", x + 10, ty, 14, missile_color());
    ty += 16;
    DrawText(TextFormat("%d / %d", battery.ammo_count(), battery.max_ammo),
             x + 10, ty, 12, dim());
    ty += 16;
    if (battery.reload_timer_sec() > 0.f) {
        DrawText(TextFormat("RELOAD %.1f s", battery.reload_timer_sec()),
                 x + 10, ty, 12, {255, 165, 0, 255});
    } else if (battery.can_fire()) {
        DrawText("READY", x + 10, ty, 12, {0, 255, 0, 255});
    }
    ty += 24;

    // Threat info
    DrawText("THREATS", x + 10, ty, 14, threat_color());
    ty += 16;
    DrawText(TextFormat("%zu active", world.threats().size()), x + 10, ty, 12, dim());
    ty += 20;

    // Tracks
    DrawText("TRACKS", x + 10, ty, 14, phosphor());
    ty += 16;
    for (const auto& t : engine.tracks()) {
        if (t.confidence > 0.2f) {
            DrawText(t.callsign.c_str(), x + 10, ty, 11, {255, 140, 140, 255});
            ty += 14;
            DrawText(TextFormat("%.0f km  BRG %03.0f", t.range_m / 1000.f, t.azimuth_deg),
                     x + 10, ty, 10, dim());
            ty += 14;
        }
    }

    ty = y + h - 120;
    DrawText("CONTROLS", x + 10, ty, 14, phosphor());
    ty += 18;
    DrawText("CLICK threat to target", x + 10, ty, 10, dim());
    ty += 14;
    DrawText("SPACE to fire", x + 10, ty, 10, dim());
    ty += 14;
    DrawText("R to reset", x + 10, ty, 10, dim());
    ty += 14;
    DrawText("ESC to quit", x + 10, ty, 10, dim());
}

} // namespace air_defense
