#pragma once

#include "ballistics.hpp"
#include <cmath>
#include <cstdint>
#include <vector>

namespace air_defense {
class ThreatWorld;

struct Interceptor {
    std::int32_t id;
    float x_m;
    float y_m;
    float z_m = 0.f;
    float vx_mps;
    float vy_mps;
    float vz_mps = 0.f;
    float lifetime_sec;
    float max_lifetime_sec;
    bool destroyed;
    float trail_x_m = 0.f;
    float trail_y_m = 0.f;
    float trail_z_m = 0.f;
    std::int32_t target_id = -1;

    bool is_alive() const { return !destroyed && lifetime_sec < max_lifetime_sec; }
    float speed_mps() const {
        return std::sqrt(vx_mps * vx_mps + vy_mps * vy_mps + vz_mps * vz_mps);
    }
};

class InterceptorManager {
public:
    InterceptorManager();

    void tick(float dt_sec);
    void tick(float dt_sec, const ThreatWorld& world);
    void fire_interceptor(float from_x, float from_y, float target_x, float target_y,
                          float target_vx, float target_vy);
    void fire_interceptor(float from_x, float from_y, float target_x, float target_y,
                          float target_vx, float target_vy, std::int32_t target_id);
    void fire_interceptor(float from_x, float from_y, float from_z,
                          float target_x, float target_y, float target_z,
                          float target_vx, float target_vy, float target_vz,
                          std::int32_t target_id);

    const std::vector<Interceptor>& interceptors() const { return interceptors_; }
    std::vector<Interceptor>& interceptors() { return interceptors_; }
    void destroy_interceptor(std::int32_t id);

    static bool calculate_intercept_point(float mx, float my, float tx, float ty,
                                          float tvx, float tvy, float missile_speed,
                                          float& intercept_x, float& intercept_y,
                                          float& time_sec);

    float missile_speed_mps = 1800.f;
    float blast_radius_m = 600.f;
    float max_missile_lifetime_sec = 120.f;

    std::int32_t total_fired = 0;
    std::int32_t total_intercepted = 0;

private:
    std::vector<Interceptor> interceptors_;
    std::int32_t next_interceptor_id_ = 2000;
    void update_interceptor_positions(float dt_sec);
};

} // namespace air_defense
