# RadarSim

C++ air-surveillance radar simulator with **real monostatic radar physics** (σ/R⁴), rotating-beam PPI display, multi-target tracking, and a flyable aircraft. Built as a **sensor-ready** foundation for hardware integration (MCU, FPGA, or UDP → game engine).

![C++](https://img.shields.io/badge/C%2B%2B-17-blue)
![Raylib](https://img.shields.io/badge/GUI-Raylib-green)
![License](https://img.shields.io/badge/license-MIT-lightgrey)

## Demo

```cmd
build-gui.bat
run-game.bat
```

- **Tactical map** — fly your aircraft (blue), AI traffic (red), radar at center  
- **PPI radar** — sweep beam, echo blips, fused tracks  
- **Runs until ESC** — no time limit  

| Control | Action |
|---------|--------|
| W / ↑ | Thrust |
| S / ↓ | Brake |
| A / D | Turn |
| Space | Pause |
| R | Reset tracks |
| ESC | Quit |

## Why this project

Real surveillance radars use:

1. Rotating azimuth sweep (or phased-array steps)  
2. Echo detection when received power exceeds noise  
3. **Range** from time-of-flight, **bearing** from antenna angle  
4. Track fusion across sweeps  

RadarSim implements that pipeline in `radar_core`, with a swappable `ISensorFeed` so simulated aircraft are replaced by real detections when hardware exists.

## Physics

Detection uses the **monostatic radar equation**:

```
P_rx ∝ σ / R⁴
```

- **σ** — radar cross-section (m²)  
- **R** — range (m)  
- Calibration constant `kRadarCal` in `core/include/radar/radar_physics.hpp` (tune on your hardware with a known target)

See [docs/PHYSICS.md](docs/PHYSICS.md) for validation steps and [docs/SENSORS.md](docs/SENSORS.md) for wiring a real front-end.

## Architecture

```
ISensorFeed  →  RadarEngine  →  PPI / HUD / UDP
 (sim / HW)      sweep + track
```

| Component | Description |
|-----------|-------------|
| `core/` | `RadarEngine`, `SimulatedWorld`, physics constants |
| `apps/radar_sim/` | Raylib game — map + PPI + flight |
| `apps/radar_console/` | Text-only tracker (CI / headless) |
| `web/` | Browser demo (same math, optional) |
| `embed/` | Notes for STM32 / bare-metal port |

## Windows path note

If the project lives under a folder with **non-ASCII characters** (e.g. `Masaüstü`), CMake/MinGW may break. Clone or copy to an ASCII path such as `C:\Dev\RadarSim` for native builds; the web demo works from any path.

## Requirements

**GUI (recommended)**

- Windows 10/11  
- [CMake](https://cmake.org/download/) 3.16+  
- MinGW-w64 `g++` on PATH ([winlibs](https://winlibs.com/) or MSYS2)  
- Internet on **first** `build-gui.bat` (downloads Raylib once)

**Console only (no CMake / no Raylib)**

- MinGW `g++` only → `build-console.bat`

## Build and run

### GUI game

```cmd
git clone <your-repo-url>
cd RadarSim
build-gui.bat
run-game.bat
```

### Console tracker

```cmd
build-console.bat
run-console.bat
```

### Web (no compile)

```cmd
run-web.bat
```

Or `run-web-server.bat` and open http://localhost:8765

## Project layout

```
RadarSim/
├── core/                 # Radar engine (hardware-ready)
├── apps/
│   ├── radar_sim/        # Raylib GUI + flight
│   └── radar_console/    # Text output
├── web/                  # Browser PPI demo
├── docs/                 # Physics, sensors, Unreal roadmap
├── build-gui.bat
├── build-console.bat
├── run-game.bat
└── run-console.bat
```

## Hardware path

1. Implement `radar::ISensorFeed` for your ADC / FPGA / UDP detections  
2. Keep `RadarEngine::tick()` unchanged on embedded Linux or MCU  
3. Recalibrate `RadarPhysics::kRadarCal` using a sphere or known aircraft  
4. Optional: stream tracks as JSON to Unreal ([docs/UNREAL.md](docs/UNREAL.md))

## Roadmap

- [ ] UDP track export for Unreal Engine HUD  
- [ ] Doppler / MTI channel ([docs/DOPPLER.md](docs/DOPPLER.md))  
- [ ] `HardwareSensorFeed` reference implementation  

## License

MIT — see [LICENSE](LICENSE).
