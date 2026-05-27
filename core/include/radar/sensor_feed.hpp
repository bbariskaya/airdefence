#pragma once

#include "radar/types.hpp"
#include <vector>

namespace radar {

/**
 * Hardware boundary: implement this for ADC/SPI radar front-end, SDR, or UDP from FPGA.
 * SimulatedWorld is the stand-in until sensors exist.
 */
class ISensorFeed {
public:
    virtual ~ISensorFeed() = default;
    virtual void tick(float dt_sec) = 0;
    virtual std::vector<AircraftState> truth_snapshot() const = 0;
};

} // namespace radar
