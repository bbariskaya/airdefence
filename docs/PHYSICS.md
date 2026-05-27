# Radar Physics & Real-world accuracy

This simulator uses physics **correct for basic surveillance radar**. Designed as a foundation for actual hardware later.

## What's modeled (real)

| Aspect | Formula / Model | Accuracy |
|--------|-----------------|----------|
| **Detection range** | `P_rx ∝ RCS / R⁴` | ✅ Monostatic radar equation (simplified) |
| **Azimuth** | `atan2(target_x, target_y)` | ✅ Correct bearing |
| **Range** | `√(x² + y²)` | ✅ Direct distance |
| **Beam gate** | ±1.5° around sweep | ✅ 3° beam width typical |
| **Track fusion** | Multi-sweep association | ✅ Similar to real MTI/track processors |
| **Clutter rejection** | Simple threshold | ⚠️ Simplified; real uses Doppler filtering |

## What's simplified (for now)

| Feature | Real radar | This sim | Use case |
|---------|-----------|---------|----------|
| **Doppler shift** | ±f₀·v_r/c for radial velocity | Not modeled | Needed for MTI (reject clutter, find moving targets) |
| **Antenna pattern** | Side lobes, gain rolloff | Perfect cosine | Real pattern has <-20 dB side lobes |
| **Propagation** | Refraction, multipath | Line-of-sight | Affects range accuracy ±10% in troposphere |
| **Clutter** | Sea, rain, terrain | None | Reduces S/N by 20-40 dB |
| **Coherent integration** | CPI ≥ 64 pulses | Single sweep | Real radar coherently sums pulses for SNR gain |

## For hardware implementation

### Before you have an antenna:

1. **Use this sim** to validate track fusion logic
2. **Add Doppler** when you have I/Q (in-phase/quadrature) from a coherent receiver
3. **Characterize your RCS** — measure physical targets in an anechoic chamber or use published data (F-16 ~10 m², 747 ~100 m²)

### When you have an RF front-end:

Replace `SimulatedWorld.snapshot()` with actual detections from your DSP:

```javascript
snapshot() {
  return this.dspBuffer.map(raw_detection => ({
    id: raw_detection.track_id,
    x: raw_detection.range * Math.sin(raw_detection.azimuth_rad),
    y: raw_detection.range * Math.cos(raw_detection.azimuth_rad),
    vx: raw_detection.doppler_velocity_x,  // new!
    vy: raw_detection.doppler_velocity_y,  // new!
    rcs: raw_detection.amplitude,
    callsign: "REAL"
  }));
}
```

Track processing stays the same.

### Typical radar specs (for context)

| Radar | Type | Range (km) | Resolution (m) | RCS sensitivity |
|-------|------|-----------|----------------|-----------------|
| AN/SPY-1 (Navy) | Phased array | 700 | 10–50 | ~1 m² |
| S-band weather | Parabolic | 480 | 100–500 | ~10 dBZ (rain rate) |
| Air traffic control | Rotating beam | 80–200 | 50–150 | ~2–8 m² (aircraft) |
| This sim | Simplified | **100** | **500** | ~2–15 m² |

## Radar equation (detailed)

**Power received:**
```
P_rx = (P_tx × G_tx × G_rx × λ² × σ) / ((4π)³ × R⁴)
```

Where:
- `P_tx` = transmitted power  
- `G_tx`, `G_rx` = antenna gains (typically same for monostatic)
- `λ` = wavelength (c / frequency)
- `σ` = radar cross-section (RCS) of target
- `R` = range

**Signal-to-noise ratio:**
```
SNR = P_rx / P_noise
```

**Detectability (approximation):**
```
P_detection ≈ (K × RCS) / R⁴  (for fixed transmit/noise/gain)
```

This sim uses:
```javascript
amplitude = (RCS * constant) / (1e-12 + R⁴)
```

## Validation

When you build hardware, measure:

1. **Received power vs. range** — plot log(amplitude) vs log(range). Should be **-12 dB per octave** (−4 slope in dB/octave for R⁴).
2. **Detection threshold** — compare threshold to noise floor. Real radar operates at **SNR > 13 dB** for ~50% P_fa.
3. **Angular resolution** — confirm with a test beacon (RCS sphere) swept across azimuth.

---

**Next steps:** Add Doppler when you have a coherent receiver. Then integrate with your actual RF front-end.
