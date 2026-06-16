#include "game_hud.hpp"
#include "game_config.hpp"
#include "raylib.h"
#include <cmath>
#include <string>
#include <algorithm>

namespace air_defense {

namespace {
constexpr float kMaxRange = GameConfig::kMaxRadarRangeM;
constexpr float kExplosionDuration = GameConfig::kExplosionDurationSec;
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

static void draw_rocket(float px, float py, float heading_deg, float scale, Color body, bool exhaust) {
    float rad = (heading_deg - 90.f) * 3.14159265f / 180.f;
    float c = std::cos(rad);
    float s = std::sin(rad);
    auto rot = [&](float lx, float ly) -> Vector2 {
        return {px + lx * c - ly * s, py + lx * s + ly * c};
    };

    Vector2 nose = rot(14.f * scale, 0.f);
    Vector2 tail = rot(-12.f * scale, 0.f);
    Vector2 top = rot(0.f, -3.5f * scale);
    Vector2 bot = rot(0.f, 3.5f * scale);
    DrawTriangle(nose, top, bot, body);

    Vector2 f1 = rot(-10.f * scale, -5.f * scale);
    Vector2 f2 = rot(-10.f * scale, 5.f * scale);
    Vector2 f3 = rot(-14.f * scale, 0.f);
    DrawTriangle(f1, f2, f3, {body.r, body.g, body.b, 200});

    DrawLineEx(top, bot, 2.f * scale, {30, 30, 30, 220});
    DrawCircleV(rot(2.f * scale, 0.f), 2.f * scale, {180, 180, 190, 255});

    if (exhaust) {
        Vector2 e1 = rot(-16.f * scale, -4.f * scale);
        Vector2 e2 = rot(-16.f * scale, 4.f * scale);
        Vector2 e3 = rot(-24.f * scale, 0.f);
        DrawTriangle(e1, e2, e3, {255, 180, 60, 200});
        DrawTriangle(e1, e2, rot(-20.f * scale, 0.f), {255, 240, 120, 140});
    }
    (void)tail;
}

static void draw_exhaust_trail(float x0, float y0, float x1, float y1) {
    DrawLineEx({x0, y0}, {x1, y1}, 5.f, {255, 140, 40, 80});
    DrawLineEx({x0, y0}, {x1, y1}, 2.f, {255, 220, 100, 160});
}

static void draw_explosion_effect(float px, float py, float progress) {
    unsigned char alpha = static_cast<unsigned char>(255 * (1.f - progress));
    float scale = 1.f + progress * 3.5f;

    // Outer shockwave ring
    DrawCircleLines(static_cast<int>(px), static_cast<int>(py),
                    static_cast<int>(18.f * scale), {255, 200, 100, alpha});
    DrawCircleV({px, py}, 14.f * scale, {255, 220, 140, static_cast<unsigned char>(alpha * 0.5f)});
    DrawCircleV({px, py}, 8.f * scale, {255, 120, 0, alpha});
    DrawCircleV({px, py}, 3.f * scale, {255, 255, 220, alpha});

    for (int i = 0; i < 12; ++i) {
        float ang = i * (2.f * 3.14159265f / 12.f) + progress * 0.8f;
        float start_r = 12.f * scale;
        float end_r = start_r + 22.f * (1.f - progress);
        Vector2 start = {px + std::cos(ang) * start_r, py + std::sin(ang) * start_r};
        Vector2 end = {px + std::cos(ang) * end_r, py + std::sin(ang) * end_r};
        DrawLineEx(start, end, 2.5f, {255, 180, 60, alpha});
    }

    for (int i = 0; i < 5; ++i) {
        float ang = i * (2.f * 3.14159265f / 5.f) + progress;
        float r = 16.f * scale + i * 5.f;
        DrawCircle(static_cast<int>(px + std::cos(ang) * r),
                   static_cast<int>(py + std::sin(ang) * r),
                   static_cast<int>(5 + progress * 6.f),
                   {255, 90, 40, static_cast<unsigned char>(alpha * 0.6f)});
    }
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

    draw_tactical_map(world, engine.sweep_azimuth_deg(), missiles);
    draw_ballistic_profile(world, missiles);
    draw_ppi(engine, current_blips_, missiles, world);
    draw_status_panel(engine, battery, world);
}

void GameHUD::draw_tactical_map(const ThreatWorld& world, float radar_az, const InterceptorManager& missiles) {
    const int mx = 20, my = 60, mw = 600, mh = 420;
    DrawRectangle(mx, my, mw, mh, {8, 14, 10, 255});
    DrawRectangleLines(mx, my, mw, mh, dim());

    const float cx = mx + mw * 0.5f;
    const float cy = my + mh * 0.5f;

    // Grid
    for (int i = -5; i <= 5; ++i) {
        DrawLine(mx, static_cast<int>(cy + i * 55), mx + mw, static_cast<int>(cy + i * 55), {14, 32, 22, 255});
        DrawLine(static_cast<int>(cx + i * 55), my, static_cast<int>(cx + i * 55), my + mh, {14, 32, 22, 255});
    }

    // Battery launcher
    DrawRectangle(static_cast<int>(cx) - 10, static_cast<int>(cy) - 6, 20, 12, {60, 80, 60, 255});
    DrawCircle(static_cast<int>(cx), static_cast<int>(cy), 6, {0, 255, 0, 255});
    DrawText("BATTERY", static_cast<int>(cx) + 14, static_cast<int>(cy) - 8, 12, {0, 255, 0, 255});

    // Radar sweep line
    float sweep_rad = (radar_az - 90.f) * 3.14159265f / 180.f;
    DrawLineEx({cx, cy},
               {cx + std::cos(sweep_rad) * 280.f, cy + std::sin(sweep_rad) * 280.f},
               2.f, {80, 255, 120, 80});

    // Draw missiles on tactical map (in-air interceptors with smoke trail)
    constexpr float kMapScale = 0.0011f;
    for (const auto& missile : missiles.interceptors()) {
        if (!missile.is_alive()) continue;
        float px = cx + missile.x_m * kMapScale;
        float py = cy - missile.y_m * kMapScale;
        float trail_x = cx + missile.trail_x_m * kMapScale;
        float trail_y = cy - missile.trail_y_m * kMapScale;
        draw_exhaust_trail(trail_x, trail_y, px, py);
        float heading = norm_deg(std::atan2(missile.vx_mps, missile.vy_mps) * 57.2958f);
        draw_rocket(px, py, heading, 0.9f, {180, 220, 255, 255}, true);
        DrawText(TextFormat("INT %d %.0fm/s", missile.id, missile.speed_mps()),
                 static_cast<int>(px) + 10, static_cast<int>(py) - 18, 9, {140, 220, 255, 220});
    }

    // Draw threats (inbound missiles on road to battery)
    for (const auto& threat : world.threats()) {
        if (threat.destroyed) continue;
        float px = cx + threat.x_m * kMapScale;
        float py = cy - threat.y_m * kMapScale;
        float hdg = norm_deg(std::atan2(threat.vx_mps, threat.vy_mps) * 57.2958f);
        draw_rocket(px, py, hdg, 1.f, {220, 70, 60, 255}, true);
        DrawText(TextFormat("%s %.0fkm %.0fm", threat.callsign.c_str(),
                            std::hypot(threat.x_m, threat.y_m) / 1000.f, threat.z_m),
                 static_cast<int>(px) + 12, static_cast<int>(py) - 10, 9, threat_color());
        DrawText(TextFormat("%.0f m/s", threat.speed_mps()),
                 static_cast<int>(px) + 12, static_cast<int>(py) + 2, 9, {255, 160, 120, 255});
    }

    // Draw destroyed threats (explosion effects)
    for (const auto& threat : world.threats()) {
        if (threat.destroyed && threat.explosion_timer < kExplosionDuration) {
            float px = cx + threat.x_m * kMapScale;
            float py = cy - threat.y_m * kMapScale;
            float progress = threat.explosion_timer / kExplosionDuration;
            draw_explosion_effect(px, py, progress);
            DrawText("KILL", static_cast<int>(px) - 12, static_cast<int>(py) - 24, 12, {255, 200, 80, 255});
        }
    }

    DrawText("TOP-DOWN RADAR MAP", mx + 8, my + 8, 16, phosphor());
}

void GameHUD::draw_ballistic_profile(const ThreatWorld& world, const InterceptorManager& missiles) {
    const int bx = 20, by = 490, bw = 600, bh = 280;
    DrawRectangle(bx, by, bw, bh, {12, 18, 28, 255});
    DrawRectangleLines(bx, by, bw, bh, dim());

    const float ground_y = by + bh - 36.f;
    const float left_x = bx + 48.f;
    const float plot_w = bw - 64.f;
    const float plot_h = bh - 56.f;
    constexpr float kMaxGroundKm = 200.f;
    constexpr float kMaxAltKm = 40.f;

    // Sky gradient
    for (int i = 0; i < 8; ++i) {
        unsigned char b = static_cast<unsigned char>(40 + i * 8);
        DrawRectangle(bx + 1, by + 1 + i * static_cast<int>(plot_h / 8.f),
                      bw - 2, static_cast<int>(plot_h / 8.f) + 1,
                      {10, 20, b, 255});
    }

    // Ground
    DrawRectangle(bx, static_cast<int>(ground_y), bw, bh - static_cast<int>(ground_y - by),
                  {45, 38, 28, 255});
    DrawLineEx({static_cast<float>(bx), ground_y}, {static_cast<float>(bx + bw), ground_y}, 2.f,
               {90, 75, 50, 255});
    DrawText("GROUND", bx + 8, static_cast<int>(ground_y) + 4, 11, {140, 120, 90, 255});

    auto to_screen = [&](float ground_m, float alt_m) -> Vector2 {
        float gx = std::clamp(ground_m / (kMaxGroundKm * 1000.f), 0.f, 1.f);
        float az = std::clamp(alt_m / (kMaxAltKm * 1000.f), 0.f, 1.f);
        return {left_x + gx * plot_w, ground_y - az * plot_h};
    };

    for (int km = 50; km <= 200; km += 50) {
        Vector2 p = to_screen(km * 1000.f, 0.f);
        DrawLine(static_cast<int>(p.x), static_cast<int>(by + 24), static_cast<int>(p.x),
                 static_cast<int>(ground_y), {30, 45, 60, 120});
        DrawText(TextFormat("%dkm", km), static_cast<int>(p.x) - 12, static_cast<int>(ground_y) + 16, 10, dim());
    }
    for (int km = 10; km <= 40; km += 10) {
        Vector2 p = to_screen(0.f, km * 1000.f);
        DrawText(TextFormat("%dkm", km), bx + 6, static_cast<int>(p.y) - 6, 9, dim());
        DrawLine(static_cast<int>(left_x), static_cast<int>(p.y), bx + bw - 8, static_cast<int>(p.y),
                 {30, 45, 60, 80});
    }

    // Battery on ground
    DrawRectangle(static_cast<int>(left_x) - 6, static_cast<int>(ground_y) - 14, 12, 14, {0, 180, 0, 255});

    for (const auto& threat : world.threats()) {
        if (threat.destroyed) continue;
        float ground = std::hypot(threat.x_m, threat.y_m);
        Vector2 p = to_screen(ground, threat.z_m);
        float hdg = norm_deg(std::atan2(threat.vx_mps, -threat.vz_mps) * 57.2958f);
        draw_rocket(p.x, p.y, hdg, 1.1f, {230, 80, 70, 255}, true);
        DrawText(TextFormat("%.0f m/s", threat.speed_mps()), static_cast<int>(p.x) + 8,
                 static_cast<int>(p.y) - 14, 9, {255, 180, 120, 255});
    }

    for (const auto& m : missiles.interceptors()) {
        if (!m.is_alive()) continue;
        float ground = std::hypot(m.x_m, m.y_m);
        Vector2 p = to_screen(ground, m.z_m);
        float hdg = norm_deg(std::atan2(m.vx_mps, -m.vz_mps) * 57.2958f);
        Vector2 pt = to_screen(std::hypot(m.trail_x_m, m.trail_y_m), m.trail_z_m);
        draw_exhaust_trail(pt.x, pt.y, p.x, p.y);
        draw_rocket(p.x, p.y, hdg, 0.95f, {120, 200, 255, 255}, true);
    }

    for (const auto& threat : world.threats()) {
        if (threat.destroyed && threat.explosion_timer < kExplosionDuration) {
            float ground = std::hypot(threat.x_m, threat.y_m);
            Vector2 p = to_screen(ground, threat.z_m);
            draw_explosion_effect(p.x, p.y, threat.explosion_timer / kExplosionDuration);
        }
    }

    DrawText("BALLISTIC PROFILE (altitude vs ground range)", bx + 8, by + 8, 15, phosphor());
    DrawText("Threats dive from 12-35 km | Intercept uses secant TOF + gravity", bx + 8, by + bh - 18, 10, dim());
}

void GameHUD::draw_ppi(const radar::RadarEngine& engine,
                       const std::vector<Blip>& blips,
                       const InterceptorManager& missiles,
                       const ThreatWorld& world) {
    const int px = 640, py = 60, pw = 380, ph = 380;
    const float cx = px + pw * 0.5f;
    const float cy = py + ph * 0.5f;

    DrawRectangle(px, py, pw, ph, {5, 10, 7, 255});
    DrawRectangleLines(px, py, pw, ph, dim());

    // Range rings (100 km steps up to 500 km)
    for (int ring = 1; ring <= 5; ++ring) {
        float rr = kPpiR * ring / 5.f;
        DrawCircleLines(static_cast<int>(cx), static_cast<int>(cy), rr, dim());
        DrawText(TextFormat("%.0f km", 100.f * ring), static_cast<int>(cx) + 4,
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

    // Draw missile tracks with trails
    for (const auto& missile : missiles.interceptors()) {
        float dist = std::hypot(missile.x_m, missile.y_m);
        if (dist < kMaxRange) {
            float az = norm_deg(std::atan2(missile.x_m, missile.y_m) * 57.2958f);
            Vector2 p = polar_px(cx, cy, dist, az);

            float trail_dist = std::hypot(missile.trail_x_m, missile.trail_y_m);
            if (trail_dist < kMaxRange && trail_dist > 0.f) {
                float trail_az = norm_deg(std::atan2(missile.trail_x_m, missile.trail_y_m) * 57.2958f);
                Vector2 trail_p = polar_px(cx, cy, trail_dist, trail_az);
                draw_exhaust_trail(trail_p.x, trail_p.y, p.x, p.y);
            }
            float heading = norm_deg(std::atan2(missile.vx_mps, missile.vy_mps) * 57.2958f);
            draw_rocket(p.x, p.y, heading, 0.7f, {160, 220, 255, 255}, true);
        }
    }

    // Draw explosion effects on PPI
    for (const auto& threat : world.threats()) {
        if (threat.destroyed && threat.explosion_timer < kExplosionDuration) {
            float dist = std::hypot(threat.x_m, threat.y_m);
            if (dist < kMaxRange) {
                float az = norm_deg(std::atan2(threat.x_m, threat.y_m) * 57.2958f);
                Vector2 p = polar_px(cx, cy, dist, az);
                float progress = threat.explosion_timer / kExplosionDuration;
                draw_explosion_effect(p.x, p.y, progress);
            }
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

    // Status
    DrawText("STATUS", x + 10, ty, 14, missile_color());
    ty += 16;
    if (battery.can_fire()) {
        DrawText("FIRING ACTIVE", x + 10, ty, 12, {0, 255, 0, 255});
    } else {
        DrawText("BATTERY DOWN", x + 10, ty, 12, {255, 0, 0, 255});
    }
    ty += 24;

    // Threat info
    DrawText("THREATS", x + 10, ty, 14, threat_color());
    ty += 16;
    DrawText(TextFormat("%d active (max %d)", world.active_threat_count(), world.max_active_threats),
             x + 10, ty, 12, dim());
    ty += 20;

    DrawText("BALLISTIC DATA", x + 10, ty, 14, {255, 180, 100, 255});
    ty += 16;
    for (const auto& th : world.threats()) {
        if (th.destroyed) continue;
        DrawText(th.callsign.c_str(), x + 10, ty, 10, threat_color());
        ty += 12;
        DrawText(TextFormat("Alt %.1f km  Spd %.0f m/s", th.z_m / 1000.f, th.speed_mps()),
                 x + 10, ty, 9, dim());
        ty += 14;
        if (ty > y + h - 120) break;
    }
    ty += 6;

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

    ty = y + h - 100;
    DrawText("AUTO MODE", x + 10, ty, 14, phosphor());
    ty += 18;
    DrawText("Radar picks closest", x + 10, ty, 10, dim());
    ty += 14;
    DrawText("unengaged track", x + 10, ty, 10, dim());
    ty += 14;
    DrawText("P pause | R reset", x + 10, ty, 10, dim());
}

} // namespace air_defense
