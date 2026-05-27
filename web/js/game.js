/**
 * 2D intercept mini-game: fly your aircraft, PPI radar detects traffic like a real sweep radar.
 */
(function () {
  const { SimulatedWorld, RadarEngine, normDeg } = window.RadarSim;

  const mapCanvas = document.getElementById("map");
  const ppiCanvas = document.getElementById("ppi");
  const mapCtx = mapCanvas.getContext("2d");
  const ppiCtx = ppiCanvas.getContext("2d");
  const trackList = document.getElementById("tracks");
  const statusEl = document.getElementById("status");

  const WORLD_SCALE = 0.004; // m -> px on map
  const PPI_R = 170;
  const MAX_RANGE = 100000;

  const world = new SimulatedWorld();
  const player = {
    id: 99,
    x: -25000,
    y: -20000,
    vx: 0,
    vy: 0,
    heading: 45,
    speed: 180,
    callsign: "YOU",
    hostile: false,
  };

  const gameSensor = {
    tick(dt) {
      world.tick(dt);
    },
    snapshot() {
      return [
        ...world.snapshot(),
        {
          id: player.id,
          x: player.x,
          y: player.y,
          vx: player.vx,
          vy: player.vy,
          rcs: 4,
          callsign: player.callsign,
          hostile: false,
        },
      ];
    },
  };
  const radar = new RadarEngine({ maxRangeM: MAX_RANGE, sweepDegPerSec: 60 });
  radar.setSensor(gameSensor);
  radar.ignoreIds.add(player.id);

  const blips = [];
  const sweepTrail = [];
  let paused = false;
  let score = 0;
  const keys = {};

  window.addEventListener("keydown", (e) => {
    keys[e.code] = true;
    if (e.code === "Space") {
      paused = !paused;
      e.preventDefault();
    }
  });
  window.addEventListener("keyup", (e) => (keys[e.code] = false));

  function allAircraft() {
    return [player, ...world.aircraft];
  }

  function tickSensor(dt) {
    player.x += player.vx * dt;
    player.y += player.vy * dt;

    let thrust = 0;
    let turn = 0;
    if (keys["ArrowUp"] || keys["KeyW"]) thrust = 1;
    if (keys["ArrowDown"] || keys["KeyS"]) thrust = -0.5;
    if (keys["ArrowLeft"] || keys["KeyA"]) turn = -1;
    if (keys["ArrowRight"] || keys["KeyD"]) turn = 1;

    player.heading = normDeg(player.heading + turn * 90 * dt);
    const rad = (player.heading - 90) * (Math.PI / 180);
    const accel = 280 * dt;
    if (thrust > 0) {
      player.vx += Math.cos(rad) * accel;
      player.vy += Math.sin(rad) * accel;
    } else if (thrust < 0) {
      player.vx *= 0.98;
      player.vy *= 0.98;
    }
    const spd = Math.hypot(player.vx, player.vy);
    const maxSpd = player.speed * (thrust > 0 ? 1.0 : 0.35);
    if (spd > maxSpd && spd > 1) {
      player.vx = (player.vx / spd) * maxSpd;
      player.vy = (player.vy / spd) * maxSpd;
    }

    const r = Math.hypot(player.x, player.y);
    if (r > 98000) {
      player.x *= 0.98;
      player.y *= 0.98;
    }

    gameSensor.tick(dt);
    radar.tick(dt);

    for (const d of radar.lastHits) {
      blips.push({ ...d, age: 0 });
    }
  }

  function updateBlips(dt) {
    for (const b of blips) b.age += dt;
    for (let i = blips.length - 1; i >= 0; i--) {
      if (blips[i].age > 2.5) blips.splice(i, 1);
    }
    sweepTrail.push(radar.sweepAz);
    if (sweepTrail.length > 40) sweepTrail.shift();
  }

  function polarPx(rangeM, azDeg, cx, cy) {
    const r = (rangeM / MAX_RANGE) * PPI_R;
    const rad = ((azDeg - 90) * Math.PI) / 180;
    return { x: cx + r * Math.cos(rad), y: cy + r * Math.sin(rad) };
  }

  function drawMap() {
    const w = mapCanvas.width;
    const h = mapCanvas.height;
    const cx = w / 2;
    const cy = h / 2;

    mapCtx.fillStyle = "#0a1210";
    mapCtx.fillRect(0, 0, w, h);

    mapCtx.strokeStyle = "#1a3d28";
    mapCtx.lineWidth = 1;
    for (let i = -4; i <= 4; i++) {
      mapCtx.beginPath();
      mapCtx.moveTo(cx + i * 80, 0);
      mapCtx.lineTo(cx + i * 80, h);
      mapCtx.stroke();
      mapCtx.beginPath();
      mapCtx.moveTo(0, cy + i * 80);
      mapCtx.lineTo(w, cy + i * 80);
      mapCtx.stroke();
    }

    // Radar site (origin)
    mapCtx.fillStyle = "#3f6";
    mapCtx.beginPath();
    mapCtx.arc(cx, cy, 8, 0, Math.PI * 2);
    mapCtx.fill();
    mapCtx.fillStyle = "#8fa";
    mapCtx.font = "12px Consolas, monospace";
    mapCtx.fillText("RADAR", cx + 12, cy + 4);

    // Sweep line on map
    const rad = ((radar.sweepAz - 90) * Math.PI) / 180;
    mapCtx.strokeStyle = "rgba(80,255,120,0.35)";
    mapCtx.lineWidth = 2;
    mapCtx.beginPath();
    mapCtx.moveTo(cx, cy);
    mapCtx.lineTo(cx + Math.cos(rad) * 280, cy + Math.sin(rad) * 280);
    mapCtx.stroke();

    for (const ac of allAircraft()) {
      const px = cx + ac.x * WORLD_SCALE;
      const py = cy - ac.y * WORLD_SCALE;
      const hdg = ((ac.heading ?? normDeg(Math.atan2(ac.vx, ac.vy) * (180 / Math.PI))) - 90) * (Math.PI / 180);
      mapCtx.save();
      mapCtx.translate(px, py);
      mapCtx.rotate(hdg);
      mapCtx.fillStyle = ac.id === player.id ? "#4af" : ac.hostile ? "#f55" : "#aa8";
      mapCtx.beginPath();
      mapCtx.moveTo(0, -10);
      mapCtx.lineTo(8, 8);
      mapCtx.lineTo(-8, 8);
      mapCtx.closePath();
      mapCtx.fill();
      mapCtx.restore();
      if (ac.id !== player.id) {
        mapCtx.fillStyle = "#aaa";
        mapCtx.font = "11px monospace";
        mapCtx.fillText(ac.callsign, px + 10, py - 8);
      }
    }
  }

  function drawPpi() {
    const w = ppiCanvas.width;
    const h = ppiCanvas.height;
    const cx = w / 2;
    const cy = h / 2;

    ppiCtx.fillStyle = "#050a08";
    ppiCtx.fillRect(0, 0, w, h);

    ppiCtx.strokeStyle = "#1e4d32";
    for (let ring = 1; ring <= 4; ring++) {
      ppiCtx.beginPath();
      ppiCtx.arc(cx, cy, (PPI_R * ring) / 4, 0, Math.PI * 2);
      ppiCtx.stroke();
      ppiCtx.fillStyle = "#2a5a3a";
      ppiCtx.font = "11px monospace";
      ppiCtx.fillText(String((25 * ring)), cx + 4, cy - (PPI_R * ring) / 4);
    }
    ppiCtx.strokeStyle = "#3f6";
    ppiCtx.beginPath();
    ppiCtx.arc(cx, cy, PPI_R, 0, Math.PI * 2);
    ppiCtx.stroke();

    for (const az of sweepTrail) {
      const rad = ((az - 90) * Math.PI) / 180;
      ppiCtx.strokeStyle = "rgba(80,255,120,0.15)";
      ppiCtx.beginPath();
      ppiCtx.moveTo(cx, cy);
      ppiCtx.lineTo(cx + PPI_R * Math.cos(rad), cy + PPI_R * Math.sin(rad));
      ppiCtx.stroke();
    }

    const sweepRad = ((radar.sweepAz - 90) * Math.PI) / 180;
    ppiCtx.strokeStyle = "#6f9";
    ppiCtx.lineWidth = 2;
    ppiCtx.beginPath();
    ppiCtx.moveTo(cx, cy);
    ppiCtx.lineTo(cx + PPI_R * Math.cos(sweepRad), cy + PPI_R * Math.sin(sweepRad));
    ppiCtx.stroke();
    ppiCtx.lineWidth = 1;

    for (const b of blips) {
      const p = polarPx(b.rangeM, b.azimuthDeg, cx, cy);
      const a = (1 - b.age / 2.5) * b.amplitude;
      ppiCtx.fillStyle = `rgba(100,255,150,${a})`;
      ppiCtx.beginPath();
      ppiCtx.arc(p.x, p.y, 3 + b.amplitude * 5, 0, Math.PI * 2);
      ppiCtx.fill();
    }

    for (const t of radar.tracks) {
      if (t.confidence < 0.25) continue;
      const p = polarPx(t.rangeM, t.azimuthDeg, cx, cy);
      ppiCtx.strokeStyle = t.hostile ? "#f66" : "#6d6";
      ppiCtx.beginPath();
      ppiCtx.arc(p.x, p.y, 10, 0, Math.PI * 2);
      ppiCtx.stroke();
      ppiCtx.fillStyle = "#afa";
      ppiCtx.font = "10px monospace";
      ppiCtx.fillText(t.callsign, p.x + 12, p.y + 4);
    }

    ppiCtx.fillStyle = "#6a8";
    ppiCtx.font = "12px monospace";
    ppiCtx.fillText("PPI — " + radar.sweepAz.toFixed(0) + "°", 8, 18);
  }

  function updateHud() {
    trackList.innerHTML = "";
    for (const t of radar.tracks) {
      const li = document.createElement("li");
      const tag = t.hostile ? "HOSTILE" : " CIV  ";
      li.textContent = `${tag} ${t.callsign}  ${(t.rangeM / 1000).toFixed(1)} km  BRG ${t.azimuthDeg.toFixed(0)}°`;
      li.className = t.hostile ? "hostile" : "civil";
      trackList.appendChild(li);
    }

    let nearest = null;
    let nd = 1e9;
    for (const t of radar.tracks.filter((x) => x.hostile)) {
      const ac = world.aircraft.find((a) => a.callsign === t.callsign);
      if (!ac) continue;
      const d = Math.hypot(ac.x - player.x, ac.y - player.y);
      if (d < nd) {
        nd = d;
        nearest = t;
      }
    }
    if (nearest && nd < 8000) score += 1;
    statusEl.textContent = paused
      ? "PAUSED — Space to resume"
      : `WASD fly | Tracks: ${radar.tracks.length} | Proximity score: ${score} | Fly near hostile blip to score`;
  }

  let last = performance.now();
  function loop(now) {
    const dt = Math.min(0.05, (now - last) / 1000);
    last = now;
    if (!paused) {
      tickSensor(dt);
      updateBlips(dt);
    }
    drawMap();
    drawPpi();
    updateHud();
    requestAnimationFrame(loop);
  }

  requestAnimationFrame(loop);
})();
