# Unreal Engine integration (planned)

## Why not Unreal in this repo yet?

Unreal needs a separate UE project (~GB), licensing, and Blueprint/C++ widget work. This repo keeps the **sensor-grade C++ core** portable; Unreal becomes a **visualization client**.

## Recommended pipeline

```
RadarEngine (C++)  --UDP JSON-->  UE5 Actor (ARadarDisplay)
                      tracks[]       UMG PPI widget
```

1. Add `apps/radar_bridge` sending `tracks()` at 20 Hz:

   ```json
   {"tracks":[{"id":1,"range_m":45000,"az_deg":32,"callsign":"UAL412"}]}
   ```

2. In UE5:
   - `FActor` with `FUdpSocketReceiver`
   - Material: circular mask + sweep line rotated each tick
   - Niagara or UMG for blip sprites
   - Optional: 3D world map with aircraft meshes at geo positions

3. Game-style mission: player aircraft + AI targets; PPI on HUD locates threats (data from same `SimulatedWorld` or UE physics).

## Assets

- PPI material: additive green, radial gradient falloff
- Audio: sweep “tick” optional
- Post-process: slight bloom on blips

## Portfolio demo

Record a short clip: Raylib PPI (this repo) + caption “core feeds Unreal via UDP” — shows systems thinking without shipping a full UE build in git.
