#pragma once

#include "radar/sensor_feed.hpp"
#include <vector>

namespace radar {

/** Moving aircraft + noise — replaces RF front-end in development. */
class SimulatedWorld : public ISensorFeed {
public:
    SimulatedWorld();

    void tick(float dt_sec) override;
    std::vector<AircraftState> truth_snapshot() const override;

private:
    std::vector<AircraftState> aircraft_;
    float time_s_{0};
};

} // namespace radar
