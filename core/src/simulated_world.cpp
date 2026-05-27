#include "radar/simulated_world.hpp"
#include <cmath>
#include <random>

namespace radar {
namespace {
float rand01(std::mt19937& rng) {
    return std::uniform_real_distribution<float>(0.f, 1.f)(rng);
}
} // namespace

SimulatedWorld::SimulatedWorld() {
    aircraft_ = {
        {1, 45000.f, 28000.f, -180.f, 80.f, 8.f, "UAL412"},
        {2, 62000.f, -15000.f, -140.f, -120.f, 12.f, "THY7K"},
        {3, 18000.f, 52000.f, 200.f, -80.f, 3.f, "GAF01"},
        {4, 85000.f, 40000.f, -250.f, 60.f, 15.f, "MIL99"},
        {5, 12000.f, -42000.f, 120.f, 150.f, 2.f, "C172"},
    };
}

void SimulatedWorld::tick(float dt_sec) {
    time_s_ += dt_sec;
    for (auto& a : aircraft_) {
        // Dead simple: velocity = constant direction, no jitter
        a.x_m += a.vx_mps * dt_sec;
        a.y_m += a.vy_mps * dt_sec;

        // Gentle coordinated turn (0.5 deg/sec max) for realism
        float turn_rate = 0.009f * std::sin(time_s_ * 0.2f + a.id);
        float vx = a.vx_mps * std::cos(turn_rate) - a.vy_mps * std::sin(turn_rate);
        float vy = a.vx_mps * std::sin(turn_rate) + a.vy_mps * std::cos(turn_rate);
        a.vx_mps = vx;
        a.vy_mps = vy;

        // Soft boundary — reverse course if leaving 100 km circle
        float r = std::hypot(a.x_m, a.y_m);
        if (r > 100000.f) {
            a.vx_mps *= -0.5f;
            a.vy_mps *= -0.5f;
        }
    }
}

std::vector<AircraftState> SimulatedWorld::truth_snapshot() const {
    return aircraft_;
}

} // namespace radar
