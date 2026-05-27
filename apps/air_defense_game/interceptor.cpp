#include "interceptor.hpp"
#include <cmath>
#include <algorithm>

namespace air_defense {

InterceptorManager::InterceptorManager()
    : next_interceptor_id_(2000) {}

void InterceptorManager::tick(float dt_sec) {
    // Update interceptor positions
    update_interceptor_positions(dt_sec);

    // Remove destroyed/expired interceptors
    interceptors_.erase(std::remove_if(interceptors_.begin(), interceptors_.end(),
                                       [](const Interceptor& i) { return !i.is_alive(); }),
                        interceptors_.end());
}

void InterceptorManager::fire_interceptor(float from_x, float from_y,
                                          float target_x, float target_y,
                                          float target_vx, float target_vy) {
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
    missile.target_id = -1;

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
    // Iterative solution: guess time_to_impact, solve for intercept, refine
    // This is a simplified proportional navigation approach

    float time = 1.0f;  // Initial guess

    for (int iter = 0; iter < kMaxIterations; ++iter) {
        // Where will target be at this time?
        float est_tx = tx + tvx * time;
        float est_ty = ty + tvy * time;

        // Distance missile must travel
        float dx = est_tx - mx;
        float dy = est_ty - my;
        float dist = std::hypot(dx, dy);

        // What time does missile actually need?
        float needed_time = (missile_speed > 0.001f) ? dist / missile_speed : time;

        // Check convergence
        if (std::abs(needed_time - time) < 0.01f) {
            intercept_x = est_tx;
            intercept_y = est_ty;
            time_sec = needed_time;
            return true;
        }

        time = needed_time;
    }

    // Fallback to last estimate
    intercept_x = tx + tvx * time;
    intercept_y = ty + tvy * time;
    time_sec = time;
    return true;
}

} // namespace air_defense
