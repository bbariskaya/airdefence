#include "interceptor.hpp"
#include "threat_world.hpp"
#include <cmath>
#include <algorithm>

namespace air_defense {

InterceptorManager::InterceptorManager() : next_interceptor_id_(2000) {}

void InterceptorManager::tick(float dt_sec) {
    update_interceptor_positions(dt_sec);
    interceptors_.erase(std::remove_if(interceptors_.begin(), interceptors_.end(),
                                       [](const Interceptor& i) { return !i.is_alive(); }),
                        interceptors_.end());
}

void InterceptorManager::tick(float dt_sec, const ThreatWorld& world) {
    const float g = Ballistics::kGravityMps2;

    for (auto& missile : interceptors_) {
        if (!missile.is_alive()) continue;

        if (missile.target_id >= 0) {
            if (auto target = world.threat_by_id(missile.target_id); target && !target->destroyed) {
                // Iterative mid-course: re-solve intercept every frame as target moves / maneuvers
                Ballistics::State3D launcher{};
                launcher.x_m = missile.x_m;
                launcher.y_m = missile.y_m;
                launcher.z_m = missile.z_m;
                launcher.vx_mps = missile.vx_mps;
                launcher.vy_mps = missile.vy_mps;
                launcher.vz_mps = missile.vz_mps;

                Ballistics::State3D tgt{};
                tgt.x_m = target->x_m;
                tgt.y_m = target->y_m;
                tgt.z_m = target->z_m;
                tgt.vx_mps = target->vx_mps;
                tgt.vy_mps = target->vy_mps;
                tgt.vz_mps = target->vz_mps;

                auto sol = Ballistics::solve_intercept_newton(launcher, tgt, missile_speed_mps, 12);

                float dx = sol.aim_point.x_m - missile.x_m;
                float dy = sol.aim_point.y_m - missile.y_m;
                float dz = sol.aim_point.z_m - missile.z_m;
                float dist = std::sqrt(dx * dx + dy * dy + dz * dz);
                if (dist > 1.f) {
                    float desired_vx = (dx / dist) * missile_speed_mps;
                    float desired_vy = (dy / dist) * missile_speed_mps;
                    float desired_vz = (dz / dist) * missile_speed_mps;
                    float steer = 0.35f;  // limited turn rate — miss if target jinks hard
                    missile.vx_mps += (desired_vx - missile.vx_mps) * steer * dt_sec;
                    missile.vy_mps += (desired_vy - missile.vy_mps) * steer * dt_sec;
                    missile.vz_mps += (desired_vz - missile.vz_mps) * steer * dt_sec;
                    float spd = missile.speed_mps();
                    if (spd > 0.1f) {
                        missile.vx_mps = (missile.vx_mps / spd) * missile_speed_mps;
                        missile.vy_mps = (missile.vy_mps / spd) * missile_speed_mps;
                        missile.vz_mps = (missile.vz_mps / spd) * missile_speed_mps;
                    }
                }
            }
        }

        missile.trail_x_m = missile.x_m;
        missile.trail_y_m = missile.y_m;
        missile.trail_z_m = missile.z_m;
        missile.x_m += missile.vx_mps * dt_sec;
        missile.y_m += missile.vy_mps * dt_sec;
        missile.z_m += missile.vz_mps * dt_sec;
        missile.vz_mps -= g * dt_sec;
        if (missile.z_m < 0.f) missile.z_m = 0.f;
        missile.lifetime_sec += dt_sec;
    }

    interceptors_.erase(std::remove_if(interceptors_.begin(), interceptors_.end(),
                                       [](const Interceptor& i) { return !i.is_alive(); }),
                        interceptors_.end());
}

void InterceptorManager::fire_interceptor(float from_x, float from_y,
                                          float target_x, float target_y,
                                          float target_vx, float target_vy) {
    fire_interceptor(from_x, from_y, 0.f, target_x, target_y, 0.f,
                     target_vx, target_vy, 0.f, -1);
}

void InterceptorManager::fire_interceptor(float from_x, float from_y,
                                          float target_x, float target_y,
                                          float target_vx, float target_vy,
                                          std::int32_t target_id) {
    fire_interceptor(from_x, from_y, 0.f, target_x, target_y, 0.f,
                     target_vx, target_vy, 0.f, target_id);
}

void InterceptorManager::fire_interceptor(float from_x, float from_y, float from_z,
                                          float target_x, float target_y, float target_z,
                                          float target_vx, float target_vy, float target_vz,
                                          std::int32_t target_id) {
    Ballistics::State3D launcher{};
    launcher.x_m = from_x;
    launcher.y_m = from_y;
    launcher.z_m = from_z;

    Ballistics::State3D tgt{};
    tgt.x_m = target_x;
    tgt.y_m = target_y;
    tgt.z_m = target_z;
    tgt.vx_mps = target_vx;
    tgt.vy_mps = target_vy;
    tgt.vz_mps = target_vz;

    auto sol = Ballistics::solve_intercept_newton(launcher, tgt, missile_speed_mps, 20);
    Ballistics::State3D vel = sol.launch_velocity;

    Interceptor missile;
    missile.id = next_interceptor_id_++;
    missile.x_m = from_x;
    missile.y_m = from_y;
    missile.z_m = from_z;
    missile.vx_mps = vel.vx_mps;
    missile.vy_mps = vel.vy_mps;
    missile.vz_mps = vel.vz_mps;
    missile.lifetime_sec = 0.f;
    missile.max_lifetime_sec = max_missile_lifetime_sec;
    missile.destroyed = false;
    missile.target_id = target_id;
    interceptors_.push_back(missile);
    ++total_fired;
}

void InterceptorManager::destroy_interceptor(std::int32_t id) {
    for (auto& i : interceptors_) {
        if (i.id == id) {
            i.destroyed = true;
            break;
        }
    }
}

void InterceptorManager::update_interceptor_positions(float dt_sec) {
    const float g = Ballistics::kGravityMps2;
    for (auto& missile : interceptors_) {
        if (!missile.is_alive()) continue;
        missile.trail_x_m = missile.x_m;
        missile.trail_y_m = missile.y_m;
        missile.trail_z_m = missile.z_m;
        missile.x_m += missile.vx_mps * dt_sec;
        missile.y_m += missile.vy_mps * dt_sec;
        missile.z_m += missile.vz_mps * dt_sec;
        missile.vz_mps -= g * dt_sec;
        if (missile.z_m < 0.f) missile.z_m = 0.f;
        missile.lifetime_sec += dt_sec;
    }
}

bool InterceptorManager::calculate_intercept_point(float mx, float my, float tx, float ty,
                                                    float tvx, float tvy, float missile_speed,
                                                    float& intercept_x, float& intercept_y,
                                                    float& time_sec) {
    float dx = tx - mx;
    float dy = ty - my;
    float a = tvx * tvx + tvy * tvy - missile_speed * missile_speed;
    float b = 2.f * (dx * tvx + dy * tvy);
    float c = dx * dx + dy * dy;

    if (std::abs(a) < 0.001f) {
        time_sec = (std::abs(b) < 0.001f) ? 0.1f : -c / b;
    } else {
        float discriminant = b * b - 4.f * a * c;
        if (discriminant < 0.f) {
            time_sec = std::hypot(dx, dy) / std::max(0.1f, missile_speed);
        } else {
            float sqrt_disc = std::sqrt(discriminant);
            float t1 = (-b - sqrt_disc) / (2.f * a);
            float t2 = (-b + sqrt_disc) / (2.f * a);
            time_sec = (t1 > 0.001f) ? t1 : t2;
            if (time_sec < 0.001f) {
                time_sec = std::hypot(dx, dy) / std::max(0.1f, missile_speed);
            }
        }
    }
    intercept_x = tx + tvx * time_sec;
    intercept_y = ty + tvy * time_sec;
    return true;
}

} // namespace air_defense
