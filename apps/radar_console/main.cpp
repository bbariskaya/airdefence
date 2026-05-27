#include "radar/radar_engine.hpp"
#include "radar/simulated_world.hpp"
#include <chrono>
#include <cstdio>
#include <thread>

int main() {
    radar::SimulatedWorld world;
    radar::RadarEngine engine;
    engine.set_sensor(&world);

    std::printf("RadarSim console — runs until Ctrl+C\n");
    std::printf("Sweep | Tracks (range km, bearing, callsign)\n");

    while (true) {
        engine.tick(0.05f);
        std::printf("\rAZ %5.1f | ", engine.sweep_azimuth_deg());
        for (const auto& t : engine.tracks()) {
            std::printf("[%s %.0fkm %.0f deg] ", t.callsign.c_str(), t.range_m / 1000.f,
                        t.azimuth_deg);
        }
        std::fflush(stdout);
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    return 0;
}
