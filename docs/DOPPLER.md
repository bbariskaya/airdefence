# Doppler velocity (future feature)

When you have a coherent radar (I/Q receiver), add radial velocity detection.

## Current sim

Right now, detection depends **only on RCS and range** — velocity doesn't matter. This works for finding stationary targets (buildings, ships).

## Real radars use Doppler for

1. **Moving target indication (MTI)** — reject clutter, keep moving targets  
2. **Velocity estimation** — used in track fusion (better prediction)
3. **Ambiguity resolution** — range/Doppler 2D filtering  

## How to add it

### Physics (Doppler shift)

For a target moving toward/away from the radar:

```
Δf / f₀ = v_radial / c = (v⃗_target · r̂) / c
```

Where:
- `Δf` = frequency shift
- `f₀` = transmitted frequency
- `v_radial` = component of velocity toward radar  
- `c` = speed of light (3×10⁸ m/s)
- `r̂` = unit vector from radar to target

### Velocity from Doppler

Rearrange to get radial velocity:

```
v_radial = (Δf / f₀) × c
```

Example: S-band radar (3 GHz), target moving at 200 m/s toward you:
```
Δf = 200 × 3e9 / 3e8 = 2 kHz
```

### Implementation sketch

1. In `processSweep()`, compute radial velocity:

```javascript
const vx_body = ac.vx;  // world frame
const vy_body = ac.vy;
const tx = ac.x;
const ty = ac.y;
const r = Math.hypot(tx, ty);
const v_radial = (vx_body * tx + vy_body * ty) / r;  // dot product
```

2. Convert to Doppler shift for a C-band radar (~5 GHz):

```javascript
const f0 = 5e9;  // Hz
const c = 3e8;   // m/s
const doppler_shift = (v_radial / c) * f0;  // Hz
```

3. Store in detection:

```javascript
this.lastHits.push({
  rangeM: range_bin,
  azimuthDeg: az,
  amplitude: amp,
  dopplerHz: doppler_shift,
  targetId: ac.id
});
```

4. Use in track update for **velocity refinement**:

```javascript
const v_from_doppler = detection.dopplerHz / f0 * c;
track.velocityEstimate = 0.8 * track.velocityEstimate + 0.2 * v_from_doppler;
```

## Ambiguity

Real radars have **max unambiguous velocity** (Nyquist for Doppler):

```
v_max = (c × PRF) / (4 × f₀)
```

Example: PRF=10 kHz, X-band (10 GHz):
```
v_max = 3e8 × 10e3 / (4 × 10e9) = 750 m/s
```

Targets faster than that "fold" and appear slower (aliasing).

---

Add Doppler once you have coherent receivers; track fusion (this sim) will immediately improve.
