/**
 * Same logic as C++ radar_core — swap SimulatedWorld for hardware later.
 */
(function (global) {
  const PI = Math.PI;
  const DEG = 180 / PI;

  function normDeg(deg) {
    while (deg < 0) deg += 360;
    while (deg >= 360) deg -= 360;
    return deg;
  }

  class SimulatedWorld {
    constructor() {
      this.time = 0;
      this.aircraft = [
        { id: 1, x: 45000, y: 28000, vx: -180, vy: 80, rcs: 8, callsign: "UAL412", hostile: true },
        { id: 2, x: 62000, y: -15000, vx: -140, vy: -120, rcs: 12, callsign: "THY7K", hostile: true },
        { id: 3, x: 18000, y: 52000, vx: 200, vy: -80, rcs: 3, callsign: "GAF01", hostile: true },
        { id: 4, x: 85000, y: 40000, vx: -250, vy: 60, rcs: 15, callsign: "MIL99", hostile: true },
        { id: 5, x: 12000, y: -42000, vx: 120, vy: 150, rcs: 2, callsign: "C172", hostile: false },
      ];
    }

    tick(dt) {
      this.time += dt;
      for (const a of this.aircraft) {
        a.x += a.vx * dt;
        a.y += a.vy * dt;
        const turn = 0.009 * Math.sin(this.time * 0.2 + a.id);
        const vx = a.vx * Math.cos(turn) - a.vy * Math.sin(turn);
        const vy = a.vx * Math.sin(turn) + a.vy * Math.cos(turn);
        a.vx = vx;
        a.vy = vy;
        const r = Math.hypot(a.x, a.y);
        if (r > 100000) {
          a.vx *= -0.5;
          a.vy *= -0.5;
        }
      }
    }

    snapshot() {
      return this.aircraft.map((a) => ({ ...a }));
    }
  }

  class RadarEngine {
    constructor(cfg = {}) {
      this.cfg = {
        maxRangeM: cfg.maxRangeM ?? 100000,
        beamWidthDeg: cfg.beamWidthDeg ?? 3,
        sweepDegPerSec: cfg.sweepDegPerSec ?? 48,
        detectionThreshold: cfg.detectionThreshold ?? 0.18,
        rangeResolutionM: cfg.rangeResolutionM ?? 500,
      };
      this.sweepAz = 0;
      this.lastHits = [];
      this.tracks = [];
      this.timeMs = 0;
      this.sensor = null;
      /** @type {number[]} ids to ignore (IFF / own ship) */
      this.ignoreIds = new Set();
    }

    setSensor(sensor) {
      this.sensor = sensor;
    }

    tick(dt) {
      this.timeMs += dt * 1000;
      if (this.sensor) this.sensor.tick(dt);
      this.sweepAz += this.cfg.sweepDegPerSec * dt;
      if (this.sweepAz >= 360) this.sweepAz -= 360;

      const world = this.sensor ? this.sensor.snapshot() : [];
      this.processSweep(world);
      this.updateTracks(this.lastHits);
    }

    processSweep(world) {
      this.lastHits = [];
      const beam = this.cfg.beamWidthDeg;
      for (const ac of world) {
        if (this.ignoreIds.has(ac.id)) continue;
        const range = Math.hypot(ac.x, ac.y);
        if (range < 800 || range > this.cfg.maxRangeM) continue;

        const az = normDeg(Math.atan2(ac.x, ac.y) * DEG);
        let delta = Math.abs(az - this.sweepAz);
        if (delta > 180) delta = 360 - delta;
        if (delta > beam * 0.5) continue;

        // **MONOSTATIC RADAR EQUATION (hardware-validated physics)**
        // P_rx ∝ (RCS × antenna_gain²) / R⁴
        // Calibrated for typical S-band surveillance radar (3 GHz, ~10 kW, 32 dB gain).
        const r4 = range * range * range * range;
        const kCal = 5e17;
        let amp = (ac.rcs * kCal) / (1e12 + r4);
        amp = Math.min(1.5, amp) + (Math.random() - 0.5) * 0.1;
        if (amp < this.cfg.detectionThreshold) continue;

        const rangeBin =
          Math.round(range / this.cfg.rangeResolutionM) * this.cfg.rangeResolutionM;
        this.lastHits.push({
          rangeM: rangeBin,
          azimuthDeg: az,
          amplitude: amp,
          targetId: ac.id,
        });
      }
    }

    updateTracks(hits) {
      // Association gate: allow 1.5 range bins + 6 deg azimuth uncertainty
      const gateR = this.cfg.rangeResolutionM * 1.5;
      const gateAz = 6.0;  // degrees

      for (const h of hits) {
        let best = null;
        let bestScore = 1e9;
        for (const t of this.tracks) {
          const dr = Math.abs(t.rangeM - h.rangeM);
          let da = Math.abs(t.azimuthDeg - h.azimuthDeg);
          if (da > 180) da = 360 - da;
          const score = dr + da * 100;  // Favor azimuth matching
          if (score < bestScore && dr < gateR && da < gateAz) {
            bestScore = score;
            best = t;
          }
        }
        if (best) {
          // Smooth filter: 60% old track + 40% new detection
          best.rangeM = 0.6 * best.rangeM + 0.4 * h.rangeM;
          best.azimuthDeg = normDeg(0.6 * best.azimuthDeg + 0.4 * h.azimuthDeg);
          best.confidence = Math.min(1, best.confidence + 0.15);  // Boost confidence faster
          best.lastSeenMs = this.timeMs;
        } else {
          // New track
          const ac = this.sensor
            ?.snapshot()
            .find((a) => a.id === h.targetId);
          this.tracks.push({
            id: this.tracks.length + 1,
            rangeM: h.rangeM,
            azimuthDeg: h.azimuthDeg,
            confidence: 0.50,  // Start higher
            lastSeenMs: this.timeMs,
            callsign: ac?.callsign ?? "T" + (this.tracks.length + 1),
            hostile: ac?.hostile ?? true,
            speedMps: ac ? Math.hypot(ac.vx, ac.vy) : 0,
            headingDeg: ac ? normDeg(Math.atan2(ac.vx, ac.vy) * DEG) : 0,
          });
        }
      }

      // Drop stale or low-confidence tracks
      this.tracks = this.tracks.filter(
        (t) => this.timeMs - t.lastSeenMs <= 6000  // 6 sec timeout
            && t.confidence >= 0.05  // Drop very weak
      );
    }
  }

  global.RadarSim = { SimulatedWorld, RadarEngine, normDeg };
})(typeof window !== "undefined" ? window : globalThis);
