#!/usr/bin/env python3
"""
Uçuş Kontrol Paneli — Yer kontrol istasyonu telemetri sunucusu.
MAVLink üzerinden SITL simülasyonu veya fiziksel uçuş kontrol kartına bağlanır.
"""
import asyncio
import json
import math
import os
import subprocess
import sys
import time
from pathlib import Path

from aiohttp import web

try:
    from pymavlink import mavutil
except ImportError:
    print("pymavlink gerekli: pip install -r dashboard/requirements.txt")
    sys.exit(1)

PROJECT_ROOT = Path(__file__).resolve().parent.parent
STATIC_DIR = Path(__file__).resolve().parent / "static"


def _ardupilot_dir() -> Path:
    env = os.environ.get("ARDUPILOT_DIR")
    if env:
        return Path(env)
    return Path(os.path.realpath(PROJECT_ROOT / "ardupilot"))


ARDUPILOT_DIR = _ardupilot_dir()
SITL_BIN = ARDUPILOT_DIR / "build" / "sitl" / "bin" / "arducopter"

HOME_LAT = 41.0082
HOME_LON = 28.9784
HOME_ALT_M = 50.0
MAX_TRACK_POINTS = 2000

SENSORS = {
    "imu": {"name": "IMU / EKF3", "status": "software_ready"},
    "baro": {"name": "Barometre", "status": "software_ready"},
    "compass": {"name": "Pusula", "status": "software_ready"},
    "gps": {"name": "GPS / GNSS", "status": "software_ready"},
    "lidar": {"name": "LIDAR", "status": "pending"},
    "thermal": {"name": "Termal", "status": "pending"},
    "energy": {"name": "Enerji Geri Kazanım", "status": "software_ready"},
}


def haversine_m(lat1, lon1, lat2, lon2):
    r = 6371000.0
    p1, p2 = math.radians(lat1), math.radians(lat2)
    dlat = math.radians(lat2 - lat1)
    dlon = math.radians(lon2 - lon1)
    a = math.sin(dlat / 2) ** 2 + math.cos(p1) * math.cos(p2) * math.sin(dlon / 2) ** 2
    return 2 * r * math.asin(math.sqrt(a))


def bearing_deg(lat1, lon1, lat2, lon2):
    p1, p2 = math.radians(lat1), math.radians(lat2)
    dlon = math.radians(lon2 - lon1)
    x = math.sin(dlon) * math.cos(p2)
    y = math.cos(p1) * math.sin(p2) - math.sin(p1) * math.cos(p2) * math.cos(dlon)
    return (math.degrees(math.atan2(x, y)) + 360) % 360


class TelemetryState:
    def __init__(self):
        self.connected = False
        self.link_type = ""
        self.sitl_running = False
        self.simulation = True
        self.last_msg_ms = 0
        self.last_hb_ms = 0
        self.started_at = time.time()
        self.armed_at = 0.0
        self.msg_count_window = 0
        self.msg_rate = 0.0
        self._rate_ts = time.time()
        self.mode = "—"
        self.armed = False
        self.home_lat = HOME_LAT
        self.home_lon = HOME_LON
        self.home_alt_m = HOME_ALT_M
        self.lat = HOME_LAT
        self.lon = HOME_LON
        self.alt_m = HOME_ALT_M
        self.rel_alt_m = 0.0
        self.satellites = 0
        self.gps_fix = 0
        self.gps_hdop = 0.0
        self.gps_eph_m = 0.0
        self.ekf_ok = False
        self.roll = 0.0
        self.pitch = 0.0
        self.yaw = 0.0
        self.groundspeed = 0.0
        self.heading = 0.0
        self.climb_rate = 0.0
        self.battery_v = 0.0
        self.battery_a = 0.0
        self.battery_pct = 0
        self.energy_v = 0.0
        self.energy_a = 0.0
        self.energy_w = 0.0
        self.energy_wh = 0.0
        self.energy_mah = 0.0
        self.energy_charging = False
        self.energy_fault = False
        self.energy_has_data = False
        self.track = []
        self._last_track_ms = 0
        self.events = []

    def tick_msg_rate(self):
        self.msg_count_window += 1
        now = time.time()
        dt = now - self._rate_ts
        if dt >= 1.0:
            self.msg_rate = round(self.msg_count_window / dt, 1)
            self.msg_count_window = 0
            self._rate_ts = now

    def heartbeat_age_ms(self):
        if self.last_hb_ms <= 0:
            return 9999
        return int(time.time() * 1000 - self.last_hb_ms)

    def link_latency_ms(self):
        if self.last_msg_ms <= 0:
            return 9999
        return int(time.time() * 1000 - self.last_msg_ms)

    def distance_home_m(self):
        return round(haversine_m(self.lat, self.lon, self.home_lat, self.home_lon), 1)

    def bearing_home_deg(self):
        return round(bearing_deg(self.lat, self.lon, self.home_lat, self.home_lon), 0)

    def flight_time_sec(self):
        if not self.armed or self.armed_at <= 0:
            return 0
        return int(time.time() - self.armed_at)

    def preflight(self):
        link_ok = self.connected and self.link_latency_ms() < 2500
        gps_ok = self.gps_fix >= 3
        hdop_ok = self.gps_hdop == 0 or self.gps_hdop <= 2.0
        batt_ok = self.battery_pct == 0 or self.battery_pct >= 20
        return {
            "mavlink": link_ok,
            "gps_3d": gps_ok,
            "hdop": hdop_ok,
            "ekf": self.ekf_ok or self.sitl_running,
            "battery": batt_ok,
            "ready": link_ok and gps_ok and hdop_ok and batt_ok and (self.ekf_ok or self.sitl_running),
        }

    def add_event(self, text: str):
        ts = time.strftime("%H:%M:%S")
        self.events.insert(0, f"[{ts}] {text}")
        self.events = self.events[:50]

    def _append_track(self):
        if self.gps_fix < 2:
            return
        now_ms = time.time() * 1000
        if now_ms - self._last_track_ms < 600:
            return
        self._last_track_ms = now_ms
        point = [round(self.lat, 7), round(self.lon, 7), round(self.rel_alt_m, 1)]
        if self.track and self.track[-1][:2] == point[:2]:
            return
        self.track.append(point)
        if len(self.track) > MAX_TRACK_POINTS:
            self.track = self.track[-MAX_TRACK_POINTS:]

    def data_sources(self):
        """Her paneldeki değerin kaynağını açıkça belirt."""
        if not self.connected:
            return {
                "flight": "offline",
                "flight_label": "Uçuş telemetrisi yok — MAVLink bağlantısı kurulmadı",
                "energy": "offline",
                "energy_label": "Enerji modülü verisi yok — bağlantı yok",
            }
        if self.energy_has_data:
            return {
                "flight": "mavlink",
                "flight_label": "Canlı uçuş telemetrisi (MAVLink)",
                "energy": "module",
                "energy_label": "Canlı enerji modülü (MAVLink msg 227)",
            }
        return {
            "flight": "mavlink",
            "flight_label": "Canlı uçuş telemetrisi (MAVLink)",
            "energy": "waiting",
            "energy_label": "Enerji modülü henüz veri göndermiyor — fiziksel kart bekleniyor",
        }

    def uptime_sec(self):
        return int(time.time() - self.started_at)

    def to_json(self):
        pf = self.preflight()
        ds = self.data_sources()
        return {
            "connected": self.connected,
            "data_sources": ds,
            "link_type": self.link_type,
            "link_latency_ms": self.link_latency_ms(),
            "heartbeat_age_ms": self.heartbeat_age_ms(),
            "msg_rate": self.msg_rate,
            "sitl_running": self.sitl_running,
            "simulation": self.simulation,
            "uptime_sec": self.uptime_sec(),
            "flight_time_sec": self.flight_time_sec(),
            "home": {"lat": self.home_lat, "lon": self.home_lon, "alt_m": self.home_alt_m},
            "distance_home_m": self.distance_home_m(),
            "bearing_home_deg": self.bearing_home_deg(),
            "mode": self.mode,
            "armed": self.armed,
            "lat": self.lat,
            "lon": self.lon,
            "alt_m": round(self.alt_m, 1),
            "rel_alt_m": round(self.rel_alt_m, 1),
            "satellites": self.satellites,
            "gps_fix": self.gps_fix,
            "gps_hdop": round(self.gps_hdop, 2),
            "gps_eph_m": round(self.gps_eph_m, 1),
            "ekf_ok": self.ekf_ok,
            "roll": round(math.degrees(self.roll), 1) if abs(self.roll) <= math.pi else round(self.roll, 1),
            "pitch": round(math.degrees(self.pitch), 1) if abs(self.pitch) <= math.pi else round(self.pitch, 1),
            "yaw": round(math.degrees(self.yaw), 1) if abs(self.yaw) <= math.pi else round(self.yaw, 1),
            "groundspeed": round(self.groundspeed, 1),
            "heading": round(self.heading, 1),
            "climb_rate": round(self.climb_rate, 1),
            "battery_v": round(self.battery_v, 2),
            "battery_a": round(self.battery_a, 2),
            "battery_pct": self.battery_pct,
            "energy_v": round(self.energy_v, 2),
            "energy_a": round(self.energy_a, 3),
            "energy_w": round(self.energy_w, 2),
            "energy_wh": round(self.energy_wh, 3),
            "energy_mah": round(self.energy_mah, 1),
            "energy_charging": self.energy_charging,
            "energy_fault": self.energy_fault,
            "energy_has_data": self.energy_has_data,
            "flight_power_w": round(max(self.battery_v * self.battery_a, 0), 2),
            "recovery_efficiency_pct": round(
                min(100.0, (self.energy_w / max(self.battery_v * self.battery_a, 0.05)) * 100)
                if self.energy_has_data and self.energy_w > 0.01 else 0.0,
                1,
            ),
            "preflight": pf,
            "track": self.track,
            "sensors": SENSORS,
            "events": self.events,
        }


state = TelemetryState()
sitl_proc = None
mav_master = None

MODE_MAP = {
    0: "STABILIZE", 2: "ALT_HOLD", 3: "AUTO", 4: "GUIDED",
    5: "LOITER", 6: "RTL", 9: "LAND",
}


def start_sitl():
    global sitl_proc
    if not SITL_BIN.exists():
        state.sitl_running = False
        state.add_event("SITL binary bulunamadı — scripts/setup-env.sh")
        return False
    if sitl_proc and sitl_proc.poll() is None:
        state.sitl_running = True
        state.simulation = True
        return True
    cmd = [
        str(SITL_BIN),
        "--model", "+",
        "--speedup", "1",
        "--home", f"{HOME_LAT},{HOME_LON},{HOME_ALT_M},0",
    ]
    sitl_proc = subprocess.Popen(
        cmd,
        cwd=str(ARDUPILOT_DIR),
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    )
    state.sitl_running = True
    state.simulation = True
    state.add_event(f"SITL başlatıldı · Home {HOME_LAT:.4f}°N {HOME_LON:.4f}°E")
    time.sleep(4)
    return True


def _request_telemetry_streams(m):
    m.target_system = 1
    m.target_component = 1
    streams = (
        mavutil.mavlink.MAV_DATA_STREAM_ALL,
        mavutil.mavlink.MAV_DATA_STREAM_POSITION,
        mavutil.mavlink.MAV_DATA_STREAM_EXTRA1,
        mavutil.mavlink.MAV_DATA_STREAM_EXTRA2,
        mavutil.mavlink.MAV_DATA_STREAM_EXTENDED_STATUS,
    )
    for stream_id in streams:
        try:
            m.mav.request_data_stream_send(1, 1, stream_id, 10, 1)
        except Exception:
            pass


def connect_mavlink():
    global mav_master
    for target in ("tcp:127.0.0.1:5760", "udp:127.0.0.1:14550"):
        try:
            m = mavutil.mavlink_connection(target, autoreconnect=True)
            m.wait_heartbeat(timeout=8)
            _request_telemetry_streams(m)
            mav_master = m
            state.connected = True
            state.link_type = target.split(":")[0].upper()
            state.simulation = state.sitl_running
            state.add_event(f"MAVLink · {state.link_type} · {target}")
            return True
        except Exception:
            continue
    state.connected = False
    state.link_type = ""
    return False


def _apply_position(lat, lon, alt_m=None, rel_alt_m=None):
    if lat != 0.0 or lon != 0.0:
        state.lat = lat
        state.lon = lon
    if alt_m is not None:
        state.alt_m = alt_m
    if rel_alt_m is not None:
        state.rel_alt_m = rel_alt_m
    state._append_track()


def _set_armed(armed):
    if armed and not state.armed:
        state.armed_at = time.time()
        state.add_event("ARM — motorlar aktif")
    elif not armed and state.armed:
        state.armed_at = 0.0
        state.add_event("DISARM — motorlar kapalı")
    state.armed = armed


def mavlink_reader():
    global mav_master
    while True:
        if mav_master is None:
            if not connect_mavlink():
                time.sleep(2)
                continue
        try:
            msg = mav_master.recv_match(blocking=True, timeout=1)
            if msg is None:
                if time.time() * 1000 - state.last_msg_ms > 5000:
                    state.connected = False
                continue
            state.last_msg_ms = time.time() * 1000
            state.connected = True
            state.tick_msg_rate()
            t = msg.get_type()

            if t == "HEARTBEAT" and msg.get_srcComponent() == 1:
                state.last_hb_ms = time.time() * 1000
                state.mode = MODE_MAP.get(msg.custom_mode, f"MODE_{msg.custom_mode}")
                _set_armed(bool(msg.base_mode & mavutil.mavlink.MAV_MODE_FLAG_SAFETY_ARMED))
            elif t == "HOME_POSITION":
                state.home_lat = msg.latitude / 1e7
                state.home_lon = msg.longitude / 1e7
                state.home_alt_m = msg.altitude / 1000.0
            elif t == "GLOBAL_POSITION_INT":
                _apply_position(
                    msg.lat / 1e7,
                    msg.lon / 1e7,
                    alt_m=msg.alt / 1000.0,
                    rel_alt_m=msg.relative_alt / 1000.0,
                )
                if hasattr(msg, "vx") and hasattr(msg, "vy"):
                    vx, vy = msg.vx / 100.0, msg.vy / 100.0
                    calc_gs = math.sqrt(vx * vx + vy * vy)
                    if calc_gs > 0:
                        state.groundspeed = calc_gs
            elif t == "GPS_RAW_INT":
                if msg.fix_type >= 2:
                    _apply_position(msg.lat / 1e7, msg.lon / 1e7, alt_m=msg.alt / 1000.0)
                state.satellites = msg.satellites_visible
                state.gps_fix = msg.fix_type
                if msg.eph != 65535 and msg.eph > 0:
                    state.gps_eph_m = msg.eph / 100.0
                    state.gps_hdop = msg.eph / 100.0
            elif t == "GPS2_RAW":
                if msg.fix_type >= state.gps_fix and msg.fix_type >= 2:
                    _apply_position(msg.lat / 1e7, msg.lon / 1e7, alt_m=msg.alt / 1000.0)
                    state.satellites = max(state.satellites, msg.satellites_visible)
                    state.gps_fix = msg.fix_type
            elif t == "ATTITUDE":
                state.roll = msg.roll
                state.pitch = msg.pitch
                state.yaw = msg.yaw
            elif t == "VFR_HUD":
                state.groundspeed = msg.groundspeed
                state.heading = msg.heading
                state.climb_rate = msg.climb
                if msg.alt:
                    state.alt_m = msg.alt
            elif t == "SYS_STATUS":
                if msg.voltage_battery > 0:
                    state.battery_v = msg.voltage_battery / 1000.0
                if msg.current_battery != -1:
                    state.battery_a = abs(msg.current_battery / 100.0)
                if msg.battery_remaining >= 0:
                    state.battery_pct = msg.battery_remaining
            elif t == "BATTERY_STATUS":
                if msg.voltages and msg.voltages[0] != 65535:
                    state.battery_v = msg.voltages[0] / 1000.0
                if msg.current_battery != -1:
                    state.battery_a = abs(msg.current_battery / 100.0)
                if msg.battery_remaining >= 0:
                    state.battery_pct = msg.battery_remaining
            elif t == "EKF_STATUS_REPORT":
                # ArduPilot: flags bit 0 = attitude OK, bit 1 = vel horiz, bit 2 = vel vert
                state.ekf_ok = bool(msg.flags & 0x07)
            elif t == "STATUSTEXT":
                try:
                    txt = msg.text
                    if isinstance(txt, bytes):
                        txt = txt.decode("utf-8", errors="ignore").rstrip("\0")
                    state.add_event(f"FC: {txt}")
                except Exception:
                    pass
            elif t == "ENERGY_RECOVERY_DATA":
                state.energy_v = msg.voltage
                state.energy_a = msg.current
                state.energy_w = msg.power
                state.energy_wh = msg.energy_wh
                state.energy_mah = msg.energy_mah
                state.energy_charging = bool(msg.charging)
                state.energy_fault = bool(msg.fault)
                state.energy_has_data = True

        except Exception as e:
            state.connected = False
            mav_master = None
            state.add_event(f"Bağlantı hatası: {e}")
            time.sleep(2)


async def sse_handler(request):
    response = web.StreamResponse(
        status=200,
        reason="OK",
        headers={
            "Content-Type": "text/event-stream",
            "Cache-Control": "no-cache",
            "Connection": "keep-alive",
            "Access-Control-Allow-Origin": "*",
        },
    )
    await response.prepare(request)
    try:
        while True:
            data = json.dumps(state.to_json(), ensure_ascii=False)
            await response.write(f"data: {data}\n\n".encode())
            await asyncio.sleep(0.3)
    except (ConnectionResetError, asyncio.CancelledError, BrokenPipeError):
        pass
    return response


async def index_handler(request):
    response = web.FileResponse(STATIC_DIR / "index.html")
    response.headers["Cache-Control"] = "no-cache, no-store, must-revalidate"
    response.headers["Pragma"] = "no-cache"
    response.headers["Expires"] = "0"
    return response


@web.middleware
async def no_cache_middleware(request, handler):
    response = await handler(request)
    if request.path.startswith("/static/"):
        response.headers["Cache-Control"] = "no-cache, no-store, must-revalidate"
        response.headers["Pragma"] = "no-cache"
        response.headers["Expires"] = "0"
    return response


async def on_startup(app):
    import threading
    start_sitl()
    threading.Thread(target=mavlink_reader, daemon=True).start()


def main():
    if not STATIC_DIR.exists():
        print(f"Static klasör yok: {STATIC_DIR}")
        sys.exit(1)
    app = web.Application(middlewares=[no_cache_middleware])
    app.router.add_get("/", index_handler)
    app.router.add_get("/events", sse_handler)
    app.router.add_static("/static/", STATIC_DIR)
    app.on_startup.append(on_startup)
    port = int(os.environ.get("GCS_PORT", os.environ.get("LOP_PORT", "8080")))
    print(f"\n  Uçuş Kontrol Paneli → http://localhost:{port}\n")
    web.run_app(app, host="0.0.0.0", port=port, print=lambda x: None)


if __name__ == "__main__":
    main()
