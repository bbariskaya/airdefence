# Connecting real sensors

## Interface

Implement `radar::ISensorFeed`:

```cpp
class MyRadarFrontEnd : public radar::ISensorFeed {
    void tick(float dt_sec) override;
    std::vector<radar::AircraftState> truth_snapshot() const override;
};
```

For production you typically do **not** expose ground truth. Instead extend the engine with:

```cpp
void ingest_detection(float range_m, float azimuth_deg, float amplitude);
```

and leave `AircraftState` only in simulation.

## Expected hardware paths

| Source | Integration |
|--------|-------------|
| FMCW / pulsed radar module | SPI or LVDS → MCU → detection list |
| SDR (Pluto, USRP) | Host DSP → UDP packets |
| ADS-B / IFF (secondary) | Merge as `Track` with different `source` flag |
| Simulator (X-Plane, UE) | UDP position feed → `SimulatedWorld`-like adapter |

## Packet sketch (UDP port 9100)

```json
{"type":"detection","range_m":42000,"az_deg":127.5,"amp":0.62}
```

`radar_sim` can be extended with a background thread parsing these into `RadarEngine`.

## Embedded

`embed/README.md` — compile `radar_core` without Raylib; drive a 240×240 circular display or send CAN frames to a glass cockpit.
