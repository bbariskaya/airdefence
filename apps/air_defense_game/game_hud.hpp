#pragma once

#include "battery_controller.hpp"
#include "interceptor.hpp"
#include "radar/radar_engine.hpp"
#include "threat_world.hpp"
#include <deque>
#include <vector>

namespace air_defense {

/**
 * Manages all HUD rendering for the Iron Dome game.
 * Handles PPI radar display, status panel, and tactical information.
 */
class GameHUD {
public:
    GameHUD();

    void render(const radar::RadarEngine& engine,
                const ThreatWorld& world,
                const InterceptorManager& missiles,
                const BatteryController& battery,
                const std::vector<float>& blip_ages);

private:
    struct Blip {
        float range_m;
        float az_deg;
        float amp;
        float age;
    };

    std::deque<float> sweep_trail_;
    std::vector<Blip> current_blips_;

    void draw_tactical_map(const ThreatWorld& world, float radar_az, const InterceptorManager& missiles);
    void draw_ppi(const radar::RadarEngine& engine,
                  const std::vector<Blip>& blips,
                  const InterceptorManager& missiles,
                  const ThreatWorld& world);
    void draw_status_panel(const radar::RadarEngine& engine,
                           const BatteryController& battery,
                           const ThreatWorld& world);

    // Helper rendering functions
    static void draw_threat_triangle(float px, float py, float heading_deg);
};

} // namespace air_defense
