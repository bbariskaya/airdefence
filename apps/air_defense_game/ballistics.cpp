#include "ballistics.hpp"
#include <algorithm>
#include <cmath>

namespace air_defense {

namespace {

using State3D = Ballistics::State3D;

float distance3(const State3D& a, const State3D& b) {
    float dx = b.x_m - a.x_m;
    float dy = b.y_m - a.y_m;
    float dz = b.z_m - a.z_m;
    return std::sqrt(dx * dx + dy * dy + dz * dz);
}

float air_density_factor(float altitude_m) {
    if (altitude_m > 15000.f) return 0.f;
    return Ballistics::kAirDensityScale * (1.f - altitude_m / 15000.f);
}

State3D apply_drag(State3D s, float dt) {
    float spd = Ballistics::speed(s);
    if (spd < 1.f) return s;
    float drag = air_density_factor(s.z_m) * spd * spd * dt;
    drag = std::min(drag, spd * 0.5f);
    float scale = (spd - drag) / spd;
    s.vx_mps *= scale;
    s.vy_mps *= scale;
    s.vz_mps *= scale;
    return s;
}

float miss_at_time(const State3D& launcher, const State3D& target, float t,
                   float missile_speed, float g) {
    State3D tgt = Ballistics::predict(target, t, g);
    State3D v0 = Ballistics::velocity_to_reach(launcher, tgt, t, missile_speed, g);
    State3D m = Ballistics::integrate_missile(launcher, v0, t, g);
    return distance3(m, tgt);
}

} // namespace

State3D Ballistics::predict(const State3D& s, float t_sec, float g) {
    State3D p = s;
    constexpr float kStep = 0.25f;
    float remaining = t_sec;
    while (remaining > 0.f) {
        float dt = std::min(kStep, remaining);
        p.x_m += p.vx_mps * dt;
        p.y_m += p.vy_mps * dt;
        p.z_m += p.vz_mps * dt;
        p.vz_mps -= g * dt;
        p = apply_drag(p, dt);
        remaining -= dt;
    }
    return p;
}

State3D Ballistics::integrate_missile(const State3D& launcher, const State3D& v0,
                                      float t_sec, float g) {
    State3D m = launcher;
    m.vx_mps = v0.vx_mps;
    m.vy_mps = v0.vy_mps;
    m.vz_mps = v0.vz_mps;
    constexpr float kStep = 0.1f;
    float remaining = t_sec;
    while (remaining > 0.f) {
        float dt = std::min(kStep, remaining);
        m.x_m += m.vx_mps * dt;
        m.y_m += m.vy_mps * dt;
        m.z_m += m.vz_mps * dt;
        m.vz_mps -= g * dt;
        if (m.z_m < 0.f) m.z_m = 0.f;
        remaining -= dt;
    }
    return m;
}

State3D Ballistics::velocity_to_reach(const State3D& from, const State3D& to, float t_sec,
                                      float speed_mps, float g) {
    State3D v{};
    if (t_sec < 0.01f) return v;
    v.vx_mps = (to.x_m - from.x_m) / t_sec;
    v.vy_mps = (to.y_m - from.y_m) / t_sec;
    v.vz_mps = (to.z_m - from.z_m + 0.5f * g * t_sec * t_sec) / t_sec;
    float mag = speed(v);
    if (mag > 0.001f) {
        v.vx_mps = (v.vx_mps / mag) * speed_mps;
        v.vy_mps = (v.vy_mps / mag) * speed_mps;
        v.vz_mps = (v.vz_mps / mag) * speed_mps;
    }
    return v;
}

Ballistics::InterceptSolution Ballistics::solve_intercept_newton(const State3D& launcher,
                                                                 const State3D& target,
                                                                 float missile_speed_mps,
                                                                 int max_iterations,
                                                                 float tolerance_m) {
    InterceptSolution out{};
    const float g = kGravityMps2;

    float t = std::max(1.f, slant_range(target) / std::max(200.f, missile_speed_mps));

    for (int i = 0; i < max_iterations; ++i) {
        float f = miss_at_time(launcher, target, t, missile_speed_mps, g);
        out.iterations = i + 1;
        out.miss_distance_m = f;

        if (f < tolerance_m) {
            out.converged = true;
            out.time_sec = t;
            out.aim_point = predict(target, t, g);
            out.launch_velocity = velocity_to_reach(launcher, out.aim_point, t, missile_speed_mps, g);
            return out;
        }

        constexpr float h = 0.05f;
        float f_plus = miss_at_time(launcher, target, t + h, missile_speed_mps, g);
        float f_minus = miss_at_time(launcher, target, std::max(0.1f, t - h), missile_speed_mps, g);
        float df = (f_plus - f_minus) / (2.f * h);
        if (std::abs(df) < 1e-8f) {
            t += 0.5f;
            continue;
        }

        float t_next = t - f / df;
        t_next = std::clamp(t_next, 0.2f, 600.f);
        if (std::abs(t_next - t) < 1e-4f) break;
        t = t_next;
    }

    out.time_sec = t;
    out.aim_point = predict(target, t, g);
    out.launch_velocity = velocity_to_reach(launcher, out.aim_point, t, missile_speed_mps, g);
    out.miss_distance_m = miss_at_time(launcher, target, t, missile_speed_mps, g);
    out.converged = out.miss_distance_m < tolerance_m * 4.f;
    return out;
}

bool Ballistics::solve_intercept_time(const State3D& launcher, const State3D& target,
                                      float missile_speed_mps, float& time_sec,
                                      int max_iterations) {
    auto sol = solve_intercept_newton(launcher, target, missile_speed_mps, max_iterations);
    time_sec = sol.time_sec;
    return sol.converged;
}

State3D Ballistics::launch_velocity(const State3D& launcher, const State3D& predicted,
                                    float missile_speed_mps) {
    float dx = predicted.x_m - launcher.x_m;
    float dy = predicted.y_m - launcher.y_m;
    float dz = predicted.z_m - launcher.z_m;
    float dist = std::sqrt(dx * dx + dy * dy + dz * dz);
    State3D v{};
    if (dist > 0.001f) {
        v.vx_mps = (dx / dist) * missile_speed_mps;
        v.vy_mps = (dy / dist) * missile_speed_mps;
        v.vz_mps = (dz / dist) * missile_speed_mps;
    }
    return v;
}

} // namespace air_defense
