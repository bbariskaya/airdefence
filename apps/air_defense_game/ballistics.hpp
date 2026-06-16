#pragma once

#include <cmath>

namespace air_defense {

/**
 * Fire-control ballistics for intercept problems.
 *
 * Launch solution: Newton-Raphson on time-of-flight T (typically 8–20 iterations)
 *   f(T) = miss distance between missile and predicted target at time T,
 *   with both bodies integrated under gravity (+ optional drag on threats).
 *
 * Mid-course: intercept is re-solved every guidance frame as the target moves
 *   (iterative track-while-intercept — not a single fixed aim point).
 */
struct Ballistics {
    static constexpr float kGravityMps2 = 9.81f;
    /** Thin-atmosphere drag scale (threats slow slightly in dense air below 15 km). */
    static constexpr float kAirDensityScale = 1.2e-4f;

    struct State3D {
        float x_m = 0.f;
        float y_m = 0.f;
        float z_m = 0.f;
        float vx_mps = 0.f;
        float vy_mps = 0.f;
        float vz_mps = 0.f;
    };

    struct InterceptSolution {
        float time_sec = 0.f;
        State3D aim_point{};
        State3D launch_velocity{};
        float miss_distance_m = 0.f;
        int iterations = 0;
        bool converged = false;
    };

    static float speed(const State3D& s) {
        return std::sqrt(s.vx_mps * s.vx_mps + s.vy_mps * s.vy_mps + s.vz_mps * s.vz_mps);
    }

    static float slant_range(const State3D& s) {
        return std::sqrt(s.x_m * s.x_m + s.y_m * s.y_m + s.z_m * s.z_m);
    }

    /** Target prediction: constant velocity + gravity on Z (+ drag below 15 km). */
    static State3D predict(const State3D& s, float t_sec, float g = kGravityMps2);

    /**
     * Newton-Raphson intercept: find T where missile meets predicted target.
     * Uses numerical derivative of miss-distance f(T).
     */
    static InterceptSolution solve_intercept_newton(const State3D& launcher, const State3D& target,
                                                    float missile_speed_mps,
                                                    int max_iterations = 20,
                                                    float tolerance_m = 25.f);

    static bool solve_intercept_time(const State3D& launcher, const State3D& target,
                                     float missile_speed_mps, float& time_sec,
                                     int max_iterations = 20);

    static State3D launch_velocity(const State3D& launcher, const State3D& predicted,
                                   float missile_speed_mps);

    /** Integrate missile from launcher with initial velocity over t seconds (gravity). */
    static State3D integrate_missile(const State3D& launcher, const State3D& v0,
                                     float t_sec, float g = kGravityMps2);

    /** Required constant-speed launch velocity to reach point in time T under gravity. */
    static State3D velocity_to_reach(const State3D& from, const State3D& to, float t_sec,
                                     float speed_mps, float g = kGravityMps2);
};

} // namespace air_defense
