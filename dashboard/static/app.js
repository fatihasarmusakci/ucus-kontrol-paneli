const FIX_NAMES = ['No Fix', 'No Fix', '2D', '3D', 'DGPS', 'RTK Float', 'RTK Fixed'];

const MAP_LAYERS = {
  satellite: L.tileLayer(
    'https://server.arcgisonline.com/ArcGIS/rest/services/World_Imagery/MapServer/tile/{z}/{y}/{x}',
    { maxZoom: 19, attribution: 'Esri · Maxar' }
  ),
  street: L.tileLayer(
    'https://{s}.tile.openstreetmap.org/{z}/{x}/{y}.png',
    { maxZoom: 19, attribution: '&copy; OpenStreetMap' }
  ),
};

let map, activeLayer = 'satellite', droneMarker, homeMarker, trackLine, homeLine;
let followDrone = true;
let localTrack = [];
let energyHistory = [];

const CHECK_LABELS = {
  mavlink: 'MAVLink bağlantısı',
  gps_3d: 'GPS 3D fix',
  hdop: 'HDOP / konum doğruluğu',
  ekf: 'EKF durum tahmini',
  battery: 'Batarya seviyesi',
};

const RING_C = 2 * Math.PI * 34;

function $(id) { return document.getElementById(id); }

function formatUptime(sec) {
  const h = Math.floor(sec / 3600);
  const m = Math.floor((sec % 3600) / 60);
  const s = sec % 60;
  return [h, m, s].map(v => String(v).padStart(2, '0')).join(':');
}

function formatFlightTime(sec) {
  if (!sec) return '—';
  const m = Math.floor(sec / 60);
  const s = sec % 60;
  return `${m}:${String(s).padStart(2, '0')}`;
}

function fmtLive(connected, val, fmt) {
  if (!connected) return '—';
  if (val === null || val === undefined || (typeof val === 'number' && !Number.isFinite(val))) return '—';
  return fmt ? fmt(val) : String(val);
}

function formatCoord(deg, isLat) {
  const abs = Math.abs(deg);
  const d = Math.floor(abs);
  const m = (abs - d) * 60;
  const hemi = isLat ? (deg >= 0 ? 'N' : 'S') : (deg >= 0 ? 'E' : 'W');
  return `${d}° ${m.toFixed(3)}' ${hemi}`;
}

function setBadge(el, state, text) {
  if (!el) return;
  el.className = `sys-badge ${state}`;
  const textEl = el.querySelector('span:last-child') || el;
  if (textEl !== el) textEl.textContent = text;
  else el.textContent = text;
}

function updateLinkQuality(d) {
  const bars = $('linkBars');
  if (!bars) return;
  bars.className = 'lq-bars';
  if (!d.connected) return;

  const rate = d.msg_rate || 0;
  const lat = d.link_latency_ms || 9999;
  let level = 0;
  if (rate > 5 && lat < 2000) level = 5;
  else if (rate > 3 && lat < 3000) level = 4;
  else if (rate > 1 && lat < 4000) level = 3;
  else if (rate > 0) level = 2;
  else level = 1;
  bars.classList.add(`lq-${level}`);
}

function updateSystemBadges(d) {
  const gpsEl = $('badgeGps');
  const ekfEl = $('badgeEkf');
  const battEl = $('badgeBatt');

  if (!d.connected) {
    setBadge(gpsEl, '', 'GPS —');
    setBadge(ekfEl, '', 'EKF —');
    setBadge(battEl, '', 'BAT —');
    $('badgeGpsText').textContent = 'GPS —';
    $('badgeEkfText').textContent = 'EKF —';
    $('badgeBattText').textContent = 'BAT —';
    return;
  }

  const fix = d.gps_fix || 0;
  let gpsState = 'err';
  let gpsText = 'NO FIX';
  if (fix >= 6) { gpsState = 'ok'; gpsText = `RTK · ${d.satellites}sv`; }
  else if (fix >= 3) { gpsState = 'ok'; gpsText = `3D · ${d.satellites}sv`; }
  else if (fix >= 2) { gpsState = 'warn'; gpsText = `2D · ${d.satellites}sv`; }
  setBadge(gpsEl, gpsState, gpsText);
  $('badgeGpsText').textContent = gpsText;

  const ekfState = d.ekf_ok ? 'ok' : 'warn';
  setBadge(ekfEl, ekfState, d.ekf_ok ? 'EKF OK' : 'EKF WARN');
  $('badgeEkfText').textContent = d.ekf_ok ? 'EKF OK' : 'EKF WARN';

  const pct = d.battery_pct;
  let battState = 'warn';
  let battText = 'BAT —';
  if (pct > 0) {
    battText = `BAT ${pct}%`;
    battState = pct >= 30 ? 'ok' : pct >= 15 ? 'warn' : 'err';
  } else if (d.battery_v > 0) {
    battText = `${d.battery_v.toFixed(1)}V`;
    battState = 'ok';
  }
  setBadge(battEl, battState, battText);
  $('badgeBattText').textContent = battText;
}

function updateMiniAdi(d) {
  const inner = $('adiInner');
  if (!inner) return;
  if (!d.connected) {
    inner.style.transform = 'rotate(0deg)';
    inner.querySelector('.adi-sky').style.transform = 'translateY(0)';
    inner.querySelector('.adi-ground').style.transform = 'translateY(0)';
    return;
  }
  inner.style.transform = `rotate(${-d.roll}deg)`;
  const offset = (d.pitch || 0) * 1.4;
  inner.querySelector('.adi-sky').style.transform = `translateY(${offset}px)`;
  inner.querySelector('.adi-ground').style.transform = `translateY(${offset}px)`;
}

function updateVehicle3D(d) {
  const card = $('vehicleCard');
  const pivot = $('vehiclePivot');
  const dial = $('compassDial');
  const quad = $('quad3d');
  if (!card || !pivot) return;

  if (!d.connected) {
    card.classList.add('stale');
    pivot.style.transform = 'rotateX(14deg) rotateZ(0deg)';
    if (dial) dial.style.transform = 'rotate(0deg)';
    if (quad) quad.classList.remove('armed');
    $('headingReadout').textContent = '—°';
    return;
  }

  card.classList.remove('stale');
  pivot.style.transform = `rotateX(${-d.pitch}deg) rotateZ(${-d.roll}deg)`;
  if (dial) dial.style.transform = `rotate(${-d.heading}deg)`;
  $('headingReadout').textContent = `${Math.round(d.heading)}°`;
  if (quad) quad.classList.toggle('armed', !!d.armed);
}

function quadcopterIcon(heading) {
  const h = Number.isFinite(heading) ? heading : 0;
  return L.divIcon({
    className: 'drone-icon-wrap',
    html: `<div class="drone-marker" style="transform:rotate(${h}deg)">
      <svg viewBox="0 0 32 32" xmlns="http://www.w3.org/2000/svg">
        <circle cx="16" cy="16" r="4" fill="#fff" stroke="#38bdf8" stroke-width="1.5"/>
        <line x1="16" y1="16" x2="6" y2="6" stroke="#fff" stroke-width="2.5" stroke-linecap="round"/>
        <line x1="16" y1="16" x2="26" y2="6" stroke="#fff" stroke-width="2.5" stroke-linecap="round"/>
        <line x1="16" y1="16" x2="6" y2="26" stroke="#fff" stroke-width="2.5" stroke-linecap="round"/>
        <line x1="16" y1="16" x2="26" y2="26" stroke="#fff" stroke-width="2.5" stroke-linecap="round"/>
        <circle cx="6" cy="6" r="3" fill="#38bdf8" stroke="#fff" stroke-width="1"/>
        <circle cx="26" cy="6" r="3" fill="#38bdf8" stroke="#fff" stroke-width="1"/>
        <circle cx="6" cy="26" r="3" fill="#38bdf8" stroke="#fff" stroke-width="1"/>
        <circle cx="26" cy="26" r="3" fill="#38bdf8" stroke="#fff" stroke-width="1"/>
        <polygon points="16,4 14,10 18,10" fill="#fbbf24"/>
      </svg>
    </div>`,
    iconSize: [34, 34],
    iconAnchor: [17, 17],
  });
}

function homeIcon() {
  return L.divIcon({
    className: 'home-icon-wrap',
    html: '<div class="home-marker"><span>H</span></div>',
    iconSize: [22, 22],
    iconAnchor: [11, 11],
  });
}

function setMapLayer(name) {
  if (!map || name === activeLayer) return;
  map.removeLayer(MAP_LAYERS[activeLayer]);
  activeLayer = name;
  MAP_LAYERS[activeLayer].addTo(map);
  document.querySelectorAll('[data-layer]').forEach(btn => {
    btn.classList.toggle('active', btn.dataset.layer === name);
  });
}

function initMap() {
  map = L.map('map', { zoomControl: true, attributionControl: true }).setView([41.0082, 28.9784], 17);
  MAP_LAYERS.satellite.addTo(map);
  L.control.scale({ imperial: false, metric: true }).addTo(map);

  homeMarker = L.marker([41.0082, 28.9784], { icon: homeIcon() })
    .addTo(map)
    .bindPopup('<b>Home</b><br>RTL / kalkış');

  droneMarker = L.marker([41.0082, 28.9784], { icon: quadcopterIcon(0), zIndexOffset: 1000 }).addTo(map);
  trackLine = L.polyline([], { color: '#38bdf8', weight: 3, opacity: 0.85, lineJoin: 'round' }).addTo(map);
  homeLine = L.polyline([], { color: '#ffffff', weight: 1.5, opacity: 0.4, dashArray: '6 8' }).addTo(map);

  map.on('dragstart', () => { followDrone = false; });
  document.querySelectorAll('[data-layer]').forEach(btn => {
    btn.addEventListener('click', () => setMapLayer(btn.dataset.layer));
  });
  $('btnCenter').addEventListener('click', () => {
    followDrone = true;
    if (droneMarker) map.setView(droneMarker.getLatLng(), Math.max(map.getZoom(), 17), { animate: true });
  });
  $('btnClearTrack').addEventListener('click', () => {
    localTrack = [];
    trackLine.setLatLngs([]);
    $('trackCount').textContent = '0';
  });
  $('btnExportGpx').addEventListener('click', exportGpx);
  $('btnFullscreen').addEventListener('click', toggleFullscreen);
}

function exportGpx() {
  if (!localTrack.length) return;
  const pts = localTrack.map(([lat, lon]) =>
    `    <trkpt lat="${lat}" lon="${lon}"><ele>0</ele><time>${new Date().toISOString()}</time></trkpt>`
  ).join('\n');
  const gpx = `<?xml version="1.0"?><gpx version="1.1"><trk><name>Ucus Kontrol Paneli</name><trkseg>\n${pts}\n</trkseg></trk></gpx>`;
  const a = document.createElement('a');
  a.href = URL.createObjectURL(new Blob([gpx], { type: 'application/gpx+xml' }));
  a.download = `skytrace-${Date.now()}.gpx`;
  a.click();
}

function toggleFullscreen() {
  const wrap = $('mapWrap');
  if (!document.fullscreenElement) wrap.requestFullscreen?.();
  else document.exitFullscreen?.();
  setTimeout(() => map?.invalidateSize(), 300);
}

function syncTrack(serverTrack) {
  if (!Array.isArray(serverTrack) || serverTrack.length === 0) return;
  if (serverTrack.length >= localTrack.length) {
    localTrack = serverTrack.map(p => [p[0], p[1]]);
    trackLine.setLatLngs(localTrack);
    $('trackCount').textContent = String(localTrack.length);
  }
}

function renderPreflight(pf, connected) {
  const items = Object.entries(CHECK_LABELS);
  $('preflight').innerHTML = items.map(([k, label]) => {
    if (!connected) return `<li class="fail"><span class="ck">·</span>${label}</li>`;
    const ok = pf[k];
    return `<li class="${ok ? 'ok' : 'fail'}"><span class="ck">${ok ? '✓' : '✗'}</span>${label}</li>`;
  }).join('');

  const passed = connected ? items.filter(([k]) => pf[k]).length : 0;
  $('preflightScore').textContent = `${passed}/${items.length}`;

  const banner = $('readyBanner');
  if (!connected) {
    banner.className = 'ready-banner';
    banner.textContent = 'Telemetri bağlantısı bekleniyor';
  } else if (pf.ready) {
    banner.className = 'ready-banner ok';
    banner.textContent = 'Tüm kontroller geçti — uçuşa hazır';
  } else {
    banner.className = 'ready-banner warn';
    banner.textContent = 'Bazı kontroller tamamlanmadı';
  }
}

function drawEnergyChart(w) {
  energyHistory.push(w);
  if (energyHistory.length > 80) energyHistory.shift();
  const canvas = $('energyChart');
  if (!canvas) return;
  const ctx = canvas.getContext('2d');
  const w0 = canvas.width;
  const h0 = canvas.height;
  ctx.clearRect(0, 0, w0, h0);
  const max = Math.max(...energyHistory, 0.5);
  const grad = ctx.createLinearGradient(0, 0, w0, 0);
  grad.addColorStop(0, '#38bdf8');
  grad.addColorStop(1, '#22c55e');
  ctx.strokeStyle = grad;
  ctx.lineWidth = 1.5;
  ctx.beginPath();
  energyHistory.forEach((v, i) => {
    const x = (i / (energyHistory.length - 1 || 1)) * w0;
    const y = h0 - (v / max) * (h0 - 6) - 3;
    i === 0 ? ctx.moveTo(x, y) : ctx.lineTo(x, y);
  });
  ctx.stroke();
}

function updateRecoveryPanel(d) {
  const ds = d.data_sources || {};
  const hasEnergy = d.energy_has_data === true;
  const flowEl = $('recoveryFlow');
  const card = $('recoveryCard');

  $('energySourceNote').textContent = ds.energy_label || '—';
  card?.classList.toggle('has-data', hasEnergy);

  if (!hasEnergy) {
    flowEl.classList.remove('flowing', 'fault');
    $('recoveredWh').textContent = '—';
    $('efficiencyPct').textContent = '—';
    $('efficiencyRing')?.setAttribute('stroke-dasharray', `0 ${RING_C}`);
    $('flowWind').textContent = fmtLive(d.connected, d.groundspeed, v => `${v} m/s`);
    $('flowPower').textContent = '—';
    $('flowBatt').textContent = fmtLive(d.connected && d.battery_pct > 0, d.battery_pct, v => `${v}%`);
    $('flightPower').textContent = fmtLive(d.connected, d.flight_power_w, v => `${v.toFixed(1)} W`);
    $('recoverPower').textContent = '—';
    $('eV').textContent = '—';
    $('eA').textContent = '—';
    $('eMah').textContent = '—';
    $('eCharge').textContent = '—';

    const status = $('recoveryStatus');
    if (!d.connected) {
      status.textContent = 'Offline';
      status.className = 'module-status';
    } else {
      status.textContent = 'Modül Bekleniyor';
      status.className = 'module-status waiting';
    }
    energyHistory = [];
    $('energyChart')?.getContext('2d')?.clearRect(0, 0, 320, 40);
    return;
  }

  const flowing = d.energy_charging && d.energy_w > 0.05 && !d.energy_fault;
  flowEl.classList.toggle('flowing', flowing);
  flowEl.classList.toggle('fault', d.energy_fault);

  $('recoveredWh').textContent = d.energy_wh.toFixed(3);
  const eff = d.recovery_efficiency_pct || 0;
  $('efficiencyPct').textContent = eff.toFixed(0);
  $('efficiencyRing')?.setAttribute('stroke-dasharray', `${(eff / 100) * RING_C} ${RING_C}`);

  $('flowWind').textContent = `${d.groundspeed} m/s`;
  $('flowPower').textContent = `${d.energy_w.toFixed(2)} W`;
  $('flowBatt').textContent = d.battery_pct > 0 ? `${d.battery_pct}%` : '—';
  $('flightPower').textContent = `${(d.flight_power_w || 0).toFixed(1)} W`;
  $('recoverPower').textContent = `${d.energy_w.toFixed(2)} W`;

  const status = $('recoveryStatus');
  if (d.energy_fault) {
    status.textContent = 'Safe-Mode';
    status.className = 'module-status fault';
  } else if (flowing) {
    status.textContent = 'Aktif';
    status.className = 'module-status active';
  } else {
    status.textContent = 'Bağlı';
    status.className = 'module-status live';
  }

  $('eV').textContent = d.energy_v.toFixed(2);
  $('eA').textContent = d.energy_a.toFixed(3);
  $('eMah').textContent = d.energy_mah.toFixed(1);
  $('eCharge').textContent = d.energy_fault ? 'KORUMA' : (d.energy_charging ? 'ŞARJ' : 'BEKLEME');
  drawEnergyChart(d.energy_w);
}

function updateStatusStrip(d) {
  const bar = $('flightSourceBar');
  const tag = $('flightSourceTag');
  const text = $('flightSourceText');

  if (d.connected) {
    bar.className = 'status-strip live';
    tag.textContent = 'CANLI';
    text.textContent = d.data_sources?.flight_label || 'MAVLink telemetrisi aktif';
    $('stripMode').textContent = d.mode || '—';
    $('stripArm').textContent = d.armed ? 'ARMED' : 'DISARMED';
    $('stripArm').className = d.armed ? 'strip-arm armed-yes' : 'strip-arm armed-no';
  } else {
    bar.className = 'status-strip offline';
    tag.textContent = 'STANDBY';
    text.textContent = 'MAVLink bağlantısı bekleniyor';
    $('stripMode').textContent = '—';
    $('stripArm').textContent = '—';
    $('stripArm').className = 'strip-arm';
  }
}

function updateMap(d) {
  const hasPos = d.connected && d.gps_fix >= 2;
  const lat = hasPos ? d.lat : (d.home?.lat ?? 41.0082);
  const lon = hasPos ? d.lon : (d.home?.lon ?? 28.9784);
  droneMarker.setLatLng([lat, lon]);
  droneMarker.setIcon(quadcopterIcon(d.connected ? d.heading : 0));
  if (d.home) {
    homeMarker.setLatLng([d.home.lat, d.home.lon]);
    homeLine.setLatLngs(hasPos ? [[lat, lon], [d.home.lat, d.home.lon]] : []);
  }
  syncTrack(d.track);
  if (followDrone && hasPos) map.panTo([lat, lon], { animate: true, duration: 0.3 });
  $('mapVeil')?.classList.toggle('hidden', !!d.connected);
}

function updateUI(d) {
  document.body.classList.toggle('telemetry-live', !!d.connected);
  document.body.classList.toggle('telemetry-offline', !d.connected);

  const conn = $('connStatus');
  if (d.connected) {
    conn.className = 'status-pill ok';
    conn.innerHTML = `<span class="dot"></span> ${d.link_type || 'MAVLink'}`;
  } else {
    conn.className = 'status-pill err';
    conn.innerHTML = '<span class="dot pulse"></span> Bağlantı Yok';
  }

  $('linkChip').textContent = d.connected ? String(d.msg_rate || 0) : '—';
  $('linkLatency').textContent = d.connected ? `${d.link_latency_ms} ms` : '—';
  $('uptimeChip').textContent = formatUptime(d.uptime_sec || 0);

  updateLinkQuality(d);
  updateSystemBadges(d);
  updateStatusStrip(d);

  // HUD
  $('hudMode').textContent = fmtLive(d.connected, d.mode);
  if (d.connected) {
    $('hudArmed').textContent = d.armed ? 'ARMED' : 'DISARMED';
    $('hudArmed').className = d.armed ? 'armed-yes' : 'armed-no';
  } else {
    $('hudArmed').textContent = '—';
    $('hudArmed').className = '';
  }
  $('hudRelAlt').textContent = fmtLive(d.connected, d.rel_alt_m, v => `${v} m`);
  $('hudSpeed').textContent = fmtLive(d.connected, d.groundspeed, v => `${v} m/s`);
  $('hudClimb').textContent = fmtLive(d.connected, d.climb_rate, v => `${v > 0 ? '+' : ''}${v} m/s`);
  $('hudHdg').textContent = fmtLive(d.connected, d.heading, v => `${Math.round(v)}°`);
  $('hudHomeDist').textContent = fmtLive(d.connected, d.distance_home_m, v => `${v} m`);
  $('hudFlightTime').textContent = d.connected && d.armed ? formatFlightTime(d.flight_time_sec) : '—';

  // Koordinat grid
  if (d.connected && d.gps_fix >= 2) {
    $('lat').textContent = formatCoord(d.lat, true);
    $('lon').textContent = formatCoord(d.lon, false);
    $('gpsQuality').textContent = FIX_NAMES[d.gps_fix] || String(d.gps_fix);
    $('sats').textContent = String(d.satellites);
    $('gpsAcc').textContent = d.gps_eph_m > 0 ? `±${d.gps_eph_m.toFixed(1)} m` : (d.gps_hdop ? `HDOP ${d.gps_hdop.toFixed(1)}` : '—');
  } else {
    $('lat').textContent = '—';
    $('lon').textContent = '—';
    $('gpsQuality').textContent = '—';
    $('sats').textContent = '—';
    $('gpsAcc').textContent = '—';
  }
  $('homeDist').textContent = fmtLive(d.connected, d.distance_home_m, v => `${v} m`);
  $('homeBrg').textContent = fmtLive(d.connected, d.bearing_home_deg, v => `${Math.round(v)}°`);

  // Attitude
  $('roll').textContent = fmtLive(d.connected, d.roll, v => `${v}°`);
  $('pitch').textContent = fmtLive(d.connected, d.pitch, v => `${v}°`);
  $('yaw').textContent = fmtLive(d.connected, d.yaw, v => `${Math.round(v)}°`);
  $('alt').textContent = fmtLive(d.connected, d.alt_m, v => `${v} m`);
  $('relAlt').textContent = fmtLive(d.connected, d.rel_alt_m, v => `${v} m`);
  $('speed').textContent = fmtLive(d.connected, d.groundspeed, v => `${v} m/s`);
  $('climb').textContent = fmtLive(d.connected, d.climb_rate, v => `${v > 0 ? '+' : ''}${v} m/s`);
  $('mode').textContent = fmtLive(d.connected, d.mode);
  $('flightTime').textContent = d.connected && d.armed ? formatFlightTime(d.flight_time_sec) : '—';

  updateVehicle3D(d);
  updateMiniAdi(d);

  // Güç
  const pct = d.connected && d.battery_pct > 0 ? d.battery_pct : null;
  $('battPct').textContent = pct != null ? `${pct}%` : '—';
  $('batteryV').textContent = fmtLive(d.connected, d.battery_v, v => `${v.toFixed(2)} V`);
  $('batteryA').textContent = fmtLive(d.connected, d.battery_a, v => `${v.toFixed(1)} A`);
  const pwr = d.connected && d.flight_power_w ? `${d.flight_power_w.toFixed(1)} W` : '—';
  $('flightPowerMain').textContent = pwr;
  $('armed').textContent = !d.connected ? '—' : (d.armed ? 'ARMED' : 'DISARMED');
  $('armed').className = `val ${d.armed ? 'armed-yes' : 'armed-no'}`;

  const gauge = $('battGauge');
  if (gauge) {
    gauge.setAttribute('stroke-dasharray', pct != null ? `${pct}, 100` : '0, 100');
    gauge.classList.toggle('low', pct != null && pct < 15);
    gauge.classList.toggle('mid', pct != null && pct >= 15 && pct < 30);
  }

  updateRecoveryPanel(d);
  renderPreflight(d.preflight || {}, d.connected);
  $('events').innerHTML = (d.events || []).slice(0, 20).map(e => `<li>${e}</li>`).join('');
  updateMap(d);
}

function connect() {
  const es = new EventSource('/events');
  es.onmessage = (ev) => {
    try { updateUI(JSON.parse(ev.data)); } catch (e) { console.error(e); }
  };
  es.onerror = () => {
    $('connStatus').className = 'status-pill err';
    $('connStatus').innerHTML = '<span class="dot pulse"></span> Sunucu Koptu';
    es.close();
    setTimeout(connect, 3000);
  };
}

document.addEventListener('DOMContentLoaded', () => {
  initMap();
  connect();
  setTimeout(() => map.invalidateSize(), 250);
});

document.addEventListener('fullscreenchange', () => setTimeout(() => map?.invalidateSize(), 200));
