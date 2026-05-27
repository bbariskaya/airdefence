#pragma once

#include "radar/sensor_feed.hpp"
#include "radar/simulated_world.hpp"
#include <cmath>
#include <vector>

namespace game {

struct PlayerAircraft {
    std::int32_t id = 99;
    float x_m = -25000.f;
    float y_m = -20000.f;
    float vx_mps = 0.f;
    float vy_mps = 0.f;
    float heading_deg = 45.f;
    float max_speed_mps = 220.f;
    float rcs_m2 = 4.f;
};

inline void update_player(PlayerAircraft& p, float dt, float thrust, float turn) {
    p.heading_deg += turn * 90.f * dt;
    while (p.heading_deg < 0.f) p.heading_deg += 360.f;
    while (p.heading_deg >= 360.f) p.heading_deg -= 360.f;

    const float rad = (p.heading_deg - 90.f) * 3.14159265f / 180.f;
    const float accel = 280.f * dt;
    if (thrust > 0.f) {
        p.vx_mps += std::cos(rad) * accel;
        p.vy_mps += std::sin(rad) * accel;
    } else if (thrust < 0.f) {
        p.vx_mps *= 0.98f;
        p.vy_mps *= 0.98f;
    }

    float spd = std::hypot(p.vx_mps, p.vy_mps);
    float cap = p.max_speed_mps * (thrust > 0.f ? 1.f : 0.35f);
    if (spd > cap && spd > 1.f) {
        p.vx_mps = p.vx_mps / spd * cap;
        p.vy_mps = p.vy_mps / spd * cap;
    }

    p.x_m += p.vx_mps * dt;
    p.y_m += p.vy_mps * dt;

    float r = std::hypot(p.x_m, p.y_m);
    if (r > 98000.f) {
        p.x_m *= 0.98f;
        p.y_m *= 0.98f;
    }
}

class GameSensor : public radar::ISensorFeed {
public:
    radar::SimulatedWorld world;
    PlayerAircraft player;

    void tick(float dt_sec) override { world.tick(dt_sec); }

    std::vector<radar::AircraftState> truth_snapshot() const override {
        std::vector<radar::AircraftState> out = world.truth_snapshot();
        radar::AircraftState me;
        me.id = player.id;
        me.x_m = player.x_m;
        me.y_m = player.y_m;
        me.vx_mps = player.vx_mps;
        me.vy_mps = player.vy_mps;
        me.rcs_m2 = player.rcs_m2;
        me.callsign = "YOU";
        out.push_back(me);
        return out;
    }
};

} // namespace game
