#pragma once

namespace radar {

/**
 * Monostatic radar equation (hardware basis):
 *   P_rx = (P_t * G_t * G_r * lambda^2 * sigma) / ((4*pi)^3 * R^4 * L)
 * Detectability uses normalized amplitude proportional to sigma / R^4.
 * kRadarCal absorbs transmit power, antenna gain, wavelength, losses, and noise bandwidth.
 * Recalibrate kRadarCal when you measure your front-end (anechoic chamber or known target).
 */
struct RadarPhysics {
    static constexpr float kRadarCal = 1e21f;  // Very high sensitivity for long-range detection
    static constexpr float kRangeFloor = 1e12f;
    static constexpr float kMinRangeM = 800.f;
    static constexpr float kDefaultMaxRangeM = 100000.f;
};

} // namespace radar
