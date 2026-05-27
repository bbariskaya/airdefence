#pragma once

#include <cmath>
#include <cstdint>
#include <vector>

namespace air_defense {

/**
 * Represents an active interceptor missile in flight.
 */
struct Interceptor {
    std::int32_t id;
    float x_m;
    float y_m;
    float vx_mps;
    float vy_mps;
    float lifetime_sec;
    float max_lifetime_sec;
    bool destroyed;
    std::int32_t target_id;  // Which threat is this interceptor aimed at (-1 if no target)

    bool is_alive() const { return !destroyed && lifetime_sec < max_lifetime_sec; }
};

/**
 * Manages interceptor missile physics and collisions.
 */
class InterceptorManager {
public:
    InterceptorManager();

    void tick(float dt_sec);
    void fire_interceptor(float from_x, float from_y, float target_x, float target_y, float target_vx, float target_vy);

    const std::vector<Interceptor>& interceptors() const { return interceptors_; }
    std::vector<Interceptor>& interceptors() { return interceptors_; }

    void destroy_interceptor(std::int32_t id);

    // Configuration
    float missile_speed_mps = 800.f;  // Interception missile speed
    float blast_radius_m = 200.f;    // Proximity detonation radius
    float max_missile_lifetime_sec = 120.f;

    // Statistics
    std::int32_t total_fired = 0;
    std::int32_t total_intercepted = 0;

private:
    std::vector<Interceptor> interceptors_;
    std::int32_t next_interceptor_id_ = 2000;

    void update_interceptor_positions(float dt_sec);

    /**
     * Calculate intercept point for constant-velocity target.
     * Returns (px, py, time_to_intercept) where missile should aim.
     */
    static constexpr int kMaxIterations = 10;
    bool calculate_intercept_point(float mx, float my, float tx, float ty,
                                    float tvx, float tvy, float missile_speed,
                                    float& intercept_x, float& intercept_y, float& time_sec);
};

} // namespace air_defense
