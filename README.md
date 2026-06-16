# RadarSim & Iron Dome Air Defense

C++17 radar and air-defense simulation. The shared **`radar_core`** library implements monostatic radar physics (σ/R⁴), rotating-beam or omnidirectional search, and track fusion. On top of that sit two applications:

| Application | Binary | Description |
|-------------|--------|-------------|
| **Radar flight sim** | `radar_sim` | Fly an aircraft; PPI + tactical map |
| **Iron Dome game** | `air_defense_game` | Automated battery vs inbound ballistic threats |

The core is built to swap simulated targets for real sensor feeds (`ISensorFeed`) on embedded hardware or over the network.

![C++17](https://img.shields.io/badge/C%2B%2B-17-blue)
![Raylib](https://img.shields.io/badge/GUI-Raylib-green)

---

## Iron Dome — what the game does

You operate a fixed air-defense battery at the origin. Inbound threats spawn at long range, dive from altitude, and head toward the battery. A search radar detects them, the fire-control computer picks the **closest unengaged track**, computes an intercept, and launches missiles automatically.

**Your job:** watch the tactical map, PPI, and ballistic profile; survive as long as possible. The battery loses health when threats reach the kill zone (~2 km).

### Displays (Raylib GUI)

- **Top-down radar map** — threats, interceptors, trails, explosions  
- **Ballistic profile** — altitude vs ground range (side view)  
- **PPI** — range rings, sweep, fused tracks  
- **Status panel** — health, threat alt/speed, track list  

### Controls

| Key | Action |
|-----|--------|
| P | Pause / resume |
| R | Reset |
| ESC | Quit |

Firing is **fully automatic** (no click-to-fire).

### Game balance (defaults)

| Parameter | Value |
|-----------|--------|
| Sim time scale | 5× wall clock |
| Fire cooldown | 0.15 s (sim time) |
| Threat spawn | every 3 s |
| Max concurrent threats | 20 |
| Threat spawn range | 250 km |
| Threat speed | 600–1000 m/s |
| Threat altitude | 12–35 km (ballistic dive) |
| Interceptor speed | 1800 m/s |
| Kill radius | 600 m (3D proximity) |

Tunable in `apps/air_defense_game/game_config.hpp` and `threat_world.hpp`.

---

## Fire-control & physics methods

### Radar (`core/`)

- Monostatic equation: detectability ∝ **σ / R⁴**  
- **Search mode:** omnidirectional detection each frame (all in-range targets)  
- **Track fusion:** nearest-neighbor association, smoothed range/azimuth, confidence  
- Max range: 500 km (configurable)

See [docs/PHYSICS.md](docs/PHYSICS.md) and [docs/SENSORS.md](docs/SENSORS.md).

### Threat motion

- 3D ballistic reentry: horizontal velocity + downward dive + **gravity on Z**  
- Below 15 km: **air drag** (speed-dependent) and **terminal maneuver** (~2.5 g lateral jink)  
- Speed and heading change in dense air — intercept is not guaranteed  

### Intercept guidance (`ballistics.cpp`, `interceptor.cpp`)

**At launch — Newton–Raphson on time-of-flight**

Find intercept time `T` (up to 20 iterations) where miss distance `f(T) → 0`:

1. Predict threat at `T` (velocity + gravity + drag)  
2. Integrate missile under gravity toward predicted point  
3. Update `T` with `T ← T − f(T)/f′(T)` (numerical derivative)  
4. Converge when miss &lt; ~25 m  

**In flight — iterative track-while-intercept**

Every frame, each missile:

1. Re-runs Newton (12 iterations) from **current** missile state to **current** threat state  
2. Steers toward the new aim point with **limited turn rate** (cannot snap instantly)  

Misses happen when targets jink, drag changes their path, or the missile cannot turn fast enough. This is intentional — not every shot hits.

**Collision:** 3D distance &lt; blast radius → kill + explosion animation.

### Headless debug

`air_defense_debug` runs the same simulation without Raylib and prints `[FIRE]`, `[KILL]`, `[HIT]` events to stdout.

---

## Build and run

### Requirements

- CMake 3.16+  
- C++17 compiler (g++, clang++)  
- Internet on first configure (downloads Raylib via FetchContent)  

Disable graphics: `cmake -DRADAR_BUILD_GRAPHICS=OFF ..`

### Linux

```bash
cd airdefence
mkdir -p build && cd build
cmake ..
cmake --build . -j$(nproc)

# Iron Dome GUI
./air_defense_game

# Console debug (no window)
./air_defense_debug

# Original radar flight sim
./radar_sim

# Tests
./test_unit
./test_integration
./test_system
```

### Windows

```cmd
build-gui.bat          # radar_sim
build-air-defense.bat  # air_defense_game (if present)
run-game.bat
```

If CMake fails under a path with non-ASCII characters, use an ASCII path such as `C:\Dev\RadarSim`.

---

## Tests

Three automated suites (no GUI):

| Target | Scope |
|--------|--------|
| `test_unit` | Radar physics, spawning, ballistics, Newton intercept, collision |
| `test_integration` | Radar + threats, track IDs, firing, collision loop |
| `test_system` | Full auto-defense pipeline, closest-target policy, one-missile-per-threat |

Run all after changes:

```bash
cd build && ./test_unit && ./test_integration && ./test_system
```

---

## Project layout

```
airdefence/
├── core/                          # radar_core library
│   ├── include/radar/             # RadarEngine, physics, ISensorFeed
│   └── src/
├── apps/
│   ├── air_defense_game/          # Iron Dome (game + ballistics + HUD)
│   │   ├── game_simulator.cpp     # Shared sim tick (GUI + console)
│   │   ├── ballistics.cpp         # Newton-Raphson intercept
│   │   ├── threat_world.cpp       # Threat spawn & motion
│   │   ├── interceptor.cpp        # Missile guidance
│   │   └── game_hud.cpp           # Raylib rendering
│   ├── radar_sim/                 # Flyable radar sim
│   ├── radar_console/             # Text-only tracker
│   └── tests/                     # unit / integration / system
├── docs/                          # Physics, sensors, Unreal notes
├── web/                           # Browser PPI demo
└── embed/                         # MCU port notes
```

---

## Architecture

```
ThreatWorld (ISensorFeed)  →  RadarEngine  →  tracks + detections
                                    ↓
                         game_simulator (auto-target, fire, collide)
                                    ↓
              Raylib HUD  /  console debug  /  (future) UDP bridge
```

`radar_core` stays independent of Raylib. Game logic lives in `apps/air_defense_game/` and links `radar_core` as a static library.

---

## Hardware integration

1. Implement `radar::ISensorFeed` for your ADC, FPGA, or UDP truth feed.  
2. Call `RadarEngine::tick(dt)` on a fixed schedule.  
3. Recalibrate `RadarPhysics::kRadarCal` with a known target ([docs/PHYSICS.md](docs/PHYSICS.md)).  
4. Optionally stream tracks to an external visualizer.

---

## What’s next — better visuals and engines

Raylib is used for a lightweight 2D/2.5D HUD. The **simulation core does not depend on it**. A typical upgrade path:

### Unreal Engine 5

- Keep `radar_core` + `game_simulator` as a **C++ sidecar** or plugin  
- Publish tracks, missile states, and explosions over **UDP/JSON** or gRPC  
- UE handles: 3D rocket meshes, Niagara explosions, cinematic camera, audio  
- See [docs/UNREAL.md](docs/UNREAL.md) for a planned bridge layout  

### Unity / Godot

- Same pattern: sim in native plugin or separate process, engine reads state each frame  
- Godot: GDExtension wrapping `radar_core`  
- Unity: native plug-in + C# UI for PPI  

### Other improvements

| Area | Idea |
|------|------|
| Radar | Doppler / MTI ([docs/DOPPLER.md](docs/DOPPLER.md)), phased-array sectors |
| Fire control | Kalman track filter, seeker model in terminal phase, salvo firing after miss |
| Ballistics | ISA atmosphere, wind, boost/coast motor phases |
| Networking | `radar_bridge` app: JSON tracks @ 20 Hz to any engine |
| Embedded | Run `RadarEngine` + fire control on Linux SBC; UE only as display |

The repo is structured so you can replace Raylib with Unreal (or any engine) without rewriting radar or intercept math.

---

## Original radar sim (radar_sim)

Fly a blue aircraft, watch AI traffic on PPI.

| Control | Action |
|---------|--------|
| W / ↑ | Thrust |
| S / ↓ | Brake |
| A / D | Turn |
| Space | Pause |
| R | Reset tracks |
| ESC | Quit |

---

## Web demo

No compile required:

```cmd
run-web.bat
```

Or serve `web/` locally and open the PPI page in a browser.

---

## License

MIT — see [LICENSE](LICENSE).
