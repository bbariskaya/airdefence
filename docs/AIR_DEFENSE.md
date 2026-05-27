# Iron Dome Battery Game - Technical Documentation

## Overview

The **Iron Dome Battery Game** (or `air_defense_game`) is a real-time simulation of an air defense system protecting a radar battery from randomized incoming threats (ballistic missiles, rockets, etc.). The game is built on top of the existing RadarSim monostatic radar engine and showcases both **radar detection physics** and **projectile interception physics**.

## Game Concept

- **Player Role**: Operate an air defense battery at the origin (0, 0).
- **Threat Model**: Randomized incoming ballistic threats spawn at outer detection range (120 km) and travel toward the battery at variable speeds (300–500 m/s).
- **Detection**: Threats are detected via monostatic radar (σ/R⁴ physics) and displayed on a PPI (Plan Position Indicator) display.
- **Interception**: Player targets threats and fires interceptor missiles. Missiles use lead-intercept guidance (iterative ballistic calculation) to predict the threat's future position.
- **Win/Lose**: Intercept threats before they reach the battery. Battery has health; hits degrade it. Game ends when battery health = 0.

## Core Mechanics

### Threat Generation (ThreatWorld)

**File**: `threat_world.hpp / threat_world.cpp`

- **Spawning**: Every 3.5 seconds, a new threat is created at a random azimuth on the outer detection ring (120 km).
- **Randomization**:
  - Azimuth: 0–360°
  - Velocity: 300–500 m/s (constant toward battery)
  - RCS: 0.5–2.0 m² (smaller than aircraft for realism)
- **Motion**: Ballistic trajectory with constant velocity (no air resistance).
- **Implements**: `radar::ISensorFeed` interface, same as original `SimulatedWorld`, so `RadarEngine` can detect threats seamlessly.

### Radar Detection

- Reuses the entire `RadarEngine` core without modification.
- **Monostatic physics**: `Amplitude = RCS × K / (min_range + R⁴)` + noise threshold.
- **Sweep**: 60°/sec (configurable).
- **Track fusion**: Nearest-neighbor association, exponential smoothing, confidence decay.

### Interceptor Missiles (InterceptorManager)

**File**: `interceptor.hpp / interceptor.cpp`

#### Fire-Control System

- **Lead Intercept**: Iterative ballistic computation.
  - Given: missile launch point, target position, target velocity, missile speed.
  - Solve: `time T` such that missile launched toward `(Px + Vx×T, Py + Vy×T)` arrives exactly when target does.
  - Method: Newton's method for 10 iterations or convergence < 0.01 s.

#### Missile Physics

- **Launch**: From battery origin (0, 0).
- **Propagation**: Constant velocity along computed intercept vector.
- **Lifetime**: Max 120 seconds; destroyed if expired.
- **Blast Radius**: 200 m proximity detonation.

#### Collision Detection

- When missile and threat are within blast radius → both destroyed.
- Interceptor count incremented for scoring.

### Battery Controller (BatteryController)

**File**: `battery_controller.hpp / battery_controller.cpp`

#### State

- **Health**: 0–100 (starts at 100). Lost when threats reach battery.
- **Ammo**: Limited magazine (12 missiles). Reload takes 2.5 seconds.
- **Selected Target**: Track ID of currently targeted threat.

#### Input Handling

- **Click on PPI**: Select a radar track for targeting.
- **Press SPACE**: Fire interceptor at selected target (if ammo, not reloading).
- **Press P**: Pause / resume.
- **Press R**: Reset game.
- **Press ESC**: Quit.

### HUD / Rendering (GameHUD)

**File**: `game_hud.hpp / game_hud.cpp`

Three-panel display (1280×800, 60 FPS):

1. **Tactical Map** (left panel, 600×600):
   - Gridded 2D map.
   - Battery at center (green dot).
   - Threats as red triangles (heading indicated).
   - Radar sweep line rotating at sweep rate.
   - Scale: ~0.0042 px per meter.

2. **PPI Radar** (center panel, 380×380):
   - Concentric range rings (0, 30, 60, 90, 120 km).
   - Sweep trail (fading history).
   - Detection blips (fading).
   - Fused tracks (red circles with callsigns).
   - Missile tracks (light blue dots).
   - Current sweep line (bright green).

3. **Status Panel** (right panel, 220×700):
   - Battery health bar (red if < 30%).
   - Ammo counter (current / max).
   - Reload timer (if active).
   - Active threat count.
   - Fused track list (range, bearing, callsign).
   - Control hints.

#### Game Over Screen

- Semi-transparent overlay.
- Displays final score (intercepted threats, threats reached).
- Prompt to restart (R) or quit (ESC).

## Scoring & Progression

- **+100 points** per successful interception.
- **–25 health** per threat reaching battery.
- Wave difficulty: Threat spawn rate remains constant; as battery takes damage, urgency increases.

## Physics Integration

### Radar Physics (Existing)

The game reuses `radar_core` without modification:
- σ/R⁴ monostatic equation: echoes weaker at far range, scaled by RCS.
- Beam sweep and range gating: only threats within ±3° of sweep azimuth and within range gates detected.
- Track smoothing: multi-scan exponential averaging reduces noise, fuses detections into stable tracks.

### Projectile Physics (New)

1. **Threat Motion**: Constant velocity ballistic trajectory toward battery.
   - `x += vx × dt`, `y += vy × dt`.

2. **Missile Guidance**: Lead intercept using iterative ballistic solution.
   - Predict threat future position for given time T.
   - Compute distance missile must travel.
   - Refine T (Newton's method).
   - Launch missile toward predicted intercept point.

3. **Collision**: Euclidean distance check (blast radius).

## Configuration

### ThreatWorld

```cpp
float spawn_interval_sec = 3.5f;           // Time between threat spawns
float detection_range_m = 120000.f;        // Spawn distance
float threat_speed_min_mps = 300.f;        // Min threat speed
float threat_speed_max_mps = 500.f;        // Max threat speed
float threat_rcs_min_m2 = 0.5f;            // Min RCS
float threat_rcs_max_m2 = 2.0f;            // Max RCS
```

### InterceptorManager

```cpp
float missile_speed_mps = 800.f;           // Interceptor velocity
float blast_radius_m = 200.f;              // Proximity detonation
float max_missile_lifetime_sec = 120.f;    // Fuse timer
```

### BatteryController

```cpp
float battery_max_health = 100.f;          // Max health
int max_ammo = 12;                         // Magazine size
float reload_time_sec = 2.5f;              // Reload duration
float damage_per_hit = 25.f;               // Damage per threat hit
```

## Build & Run

### Windows (MinGW + CMake)

```bash
# Build
build-air-defense.bat

# Run
run-air-defense.bat
```

### From CMake

```bash
cmake -B build -G "MinGW Makefiles" .
cd build
mingw32-make air_defense_game
./air_defense_game.exe
```

## File Structure

```
apps/air_defense_game/
├── main.cpp                    # Game loop, collision detection, UI event handling
├── threat_world.hpp/.cpp       # Randomized threat spawning & motion
├── interceptor.hpp/.cpp        # Missile physics & lead intercept guidance
├── battery_controller.hpp/.cpp # Player input, ammo, health, targeting
└── game_hud.hpp/.cpp           # PPI display, tactical map, status panel
```

## Future Enhancements

1. **Doppler Channel**: Velocity estimation for better track discrimination.
2. **Advanced Guidance**: Proportional navigation (PNG) for higher intercept rates.
3. **Multi-Target Prioritization**: Algorithm to decide which threat to target first (e.g., closest, fastest).
4. **Decoys/Clutter**: Radar clutter and false targets to increase difficulty.
5. **UDP Export**: Stream fused tracks to Unreal Engine for 3D visualization.
6. **Network Multiplayer**: Multiple batteries defend a perimeter.
7. **Different Threat Types**: Cruise missiles (slower, lower altitude), hypersonics (faster, higher RCS).

## Performance

- **Target**: 60 FPS on typical gaming hardware.
- **Memory**: ~50 MB (Raylib + thread stacks).
- **CPU**: ~5–10% on mid-range GPU (depends on particles, draw calls).

## Known Limitations

1. **2D Simulation**: All motion is in the X-Y plane; no altitude.
2. **Ballistic Only**: No maneuvering threats or evasive tactics.
3. **Single Battery**: One fixed air defense system; no mobility.
4. **Simplified Physics**: No air drag, no Coriolis, no multipath radar clutter.

## Debugging

- **Pause (P)**: Freeze physics to inspect radar state.
- **Reset (R)**: Clear all threats and reset battery to full health / ammo.
- **Console Logging**: All Raylib initialization logged to stderr; useful for checking OpenGL capabilities.

---

**Author**: Generated by RadarSim project  
**License**: MIT  
**Disclaimer**: This is a simplified educational simulation. Real air defense systems involve far more complex physics, networking, and military protocols.
