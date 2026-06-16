#include "interceptor.hpp"
#include "threat_world.hpp"
#include <cmath>
#include <algorithm>

namespace air_defense {

InterceptorManager::InterceptorManager()
    : next_interceptor_id_(2000) {}

void InterceptorManager::tick(float dt_sec) {
    // No-op placeholder for legacy tick; new overload uses world data
    update_interceptor_positions(dt_sec);

    interceptors_.erase(std::remove_if(interceptors_.begin(), interceptors_.end(),
                                       [](const Interceptor& i) { return !i.is_alive(); }),
                        interceptors_.end());
}

void InterceptorManager::tick(float dt_sec, const ThreatWorld& world) {
    // Update interceptor guidance and motion
    for (auto& missile : interceptors_) {
        if (!missile.is_alive()) continue;

        // Adjust heading toward current assigned target
        if (missile.target_id >= 0) {
            if (auto target = world.threat_by_id(missile.target_id); target && !target->destroyed) {
                float dx = target->x_m - missile.x_m;
                float dy = target->y_m - missile.y_m;
                float dist = std::hypot(dx, dy);
                if (dist > 1.f) {
                    float desired_vx = (dx / dist) * missile_speed_mps;
                    float desired_vy = (dy / dist) * missile_speed_mps;
                    float steer_strength = 0.25f; // smooth turning
                    missile.vx_mps += (desired_vx - missile.vx_mps) * steer_strength * dt_sec;
                    missile.vy_mps += (desired_vy - missile.vy_mps) * steer_strength * dt_sec;
                    float speed = std::hypot(missile.vx_mps, missile.vy_mps);
                    if (speed > 0.1f) {
                        missile.vx_mps = (missile.vx_mps / speed) * missile_speed_mps;
                        missile.vy_mps = (missile.vy_mps / speed) * missile_speed_mps;
                    }
                }
            }
        }

        // Store previous position for trail
        missile.trail_x_m = missile.x_m;
        missile.trail_y_m = missile.y_m;
        missile.x_m += missile.vx_mps * dt_sec;
        missile.y_m += missile.vy_mps * dt_sec;
        missile.lifetime_sec += dt_sec;
    }

    interceptors_.erase(std::remove_if(interceptors_.begin(), interceptors_.end(),
                                       [](const Interceptor& i) { return !i.is_alive(); }),
                        interceptors_.end());
}

void InterceptorManager::fire_interceptor(float from_x, float from_y,
                                          float target_x, float target_y,
                                          float target_vx, float target_vy) {
    fire_interceptor(from_x, from_y, target_x, target_y, target_vx, target_vy, -1);
}

void InterceptorManager::fire_interceptor(float from_x, float from_y,
                                          float target_x, float target_y,
                                          float target_vx, float target_vy,
                                          std::int32_t target_id) {
    // Calculate intercept point using iterative method
    float intercept_x, intercept_y, time_to_intercept;

    if (!calculate_intercept_point(from_x, from_y, target_x, target_y,
                                   target_vx, target_vy, missile_speed_mps,
                                   intercept_x, intercept_y, time_to_intercept)) {
        // Failed to calculate intercept, fire directly at target
        intercept_x = target_x;
        intercept_y = target_y;
        time_to_intercept = 10.0f;  // Fallback estimate
    }

    // Calculate velocity vector to reach intercept point
    float dx = intercept_x - from_x;
    float dy = intercept_y - from_y;
    float dist = std::hypot(dx, dy);

    float vx = (dist > 0.001f) ? (dx / dist) * missile_speed_mps : 0.f;
    float vy = (dist > 0.001f) ? (dy / dist) * missile_speed_mps : 0.f;

    Interceptor missile;
    missile.id = next_interceptor_id_++;
    missile.x_m = from_x;
    missile.y_m = from_y;
    missile.vx_mps = vx;
    missile.vy_mps = vy;
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
    for (auto& missile : interceptors_) {
        if (missile.is_alive()) {
            // Store previous position for trail
            missile.trail_x_m = missile.x_m;
            missile.trail_y_m = missile.y_m;
            
            // Update position
            missile.x_m += missile.vx_mps * dt_sec;
            missile.y_m += missile.vy_mps * dt_sec;
            missile.lifetime_sec += dt_sec;
        }
    }
}

bool InterceptorManager::calculate_intercept_point(float mx, float my, float tx, float ty,
                                                    float tvx, float tvy, float missile_speed,
                                                    float& intercept_x, float& intercept_y,
                                                    float& time_sec) {
    // Direct solution using quadratic equation
    // Target position at time t: (tx + tvx*t, ty + tvy*t)
    // Missile distance at time t: missile_speed * t
    // Solve: distance = missile_speed * t
    
    float dx = tx - mx;
    float dy = ty - my;
    
    // Quadratic coefficients: (tvx^2 + tvy^2 - v^2) * t^2 + 2*(dx*tvx + dy*tvy) * t + (dx^2 + dy^2) = 0
    float a = tvx * tvx + tvy * tvy - missile_speed * missile_speed;
    float b = 2.f * (dx * tvx + dy * tvy);
    float c = dx * dx + dy * dy;
    
    // Handle degenerate case (target moving slower than missile speed or at missile)
    if (std::abs(a) < 0.001f) {
        // Linear equation: b*t + c = 0
        if (std::abs(b) < 0.001f) {
            time_sec = 0.1f;  // Fallback
        } else {
            time_sec = -c / b;
        }
    } else {
        // Solve quadratic
        float discriminant = b * b - 4.f * a * c;
        if (discriminant < 0.f) {
            // No solution - missile too slow, aim at current position
            time_sec = std::hypot(dx, dy) / std::max(0.1f, missile_speed);
        } else {
            float sqrt_disc = std::sqrt(discriminant);
            float t1 = (-b - sqrt_disc) / (2.f * a);
            float t2 = (-b + sqrt_disc) / (2.f * a);
            
            // Pick smallest positive time
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
