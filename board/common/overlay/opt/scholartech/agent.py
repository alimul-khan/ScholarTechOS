#!/usr/bin/env python3
"""ScholarTechOS reporting agent.

Watches the player (mpv IPC socket) and reports to the report server:
  - proof of play: every item shown, when it started and for how long
  - heartbeat: online status and device health, every HEARTBEAT seconds
  - screenshot: what is on the screen, every SCREENSHOT seconds
Reports are queued while the network is down and sent when it returns.

Config: /etc/scholartech/cloud.conf (KEY=value): SERVER, DEVICE, TOKEN (agent.conf as fallback)
"""
import json
import os
import socket
import subprocess
import threading
import time
import urllib.request

CONFS = ("/etc/scholartech/cloud.conf", "/etc/scholartech/agent.conf")
MPV_SOCK = "/run/mpv.sock"
HEARTBEAT = 60
SCREENSHOT = 120
SHOT_RAW = "/tmp/agent-shot.jpg"
SHOT_SMALL = "/tmp/agent-shot-small.jpg"


def load_conf():
    """First config file that has a device identity (cloud.conf is written at registration)."""
    for path in CONFS:
        conf = {}
        try:
            with open(path) as f:
                for line in f:
                    if "=" in line and not line.lstrip().startswith("#"):
                        k, v = line.strip().split("=", 1)
                        conf[k] = v
        except FileNotFoundError:
            continue
        if conf.get("DEVICE") and conf.get("TOKEN"):
            return conf
    raise OSError("device not registered yet")
state = {"now_playing": None, "started": None}
queue = []  # play events waiting to be sent
lock = threading.Lock()


def post(path, body, ctype="application/json", extra=None):
    cfg = load_conf()
    req = urllib.request.Request(cfg["SERVER"].rstrip("/") + path, data=body, method="POST")
    req.add_header("Content-Type", ctype)
    req.add_header("X-Device", cfg["DEVICE"])
    req.add_header("X-Token", cfg["TOKEN"])
    for k, v in (extra or {}).items():
        req.add_header(k, v)
    with urllib.request.urlopen(req, timeout=15) as r:
        return r.status == 200


def kind_of(path):
    name = os.path.basename(path or "").lower()
    if name.endswith((".mp4", ".mkv", ".mov", ".webm")):
        return "video"
    if name.startswith("text-image-"):
        return "text+image"
    if name.startswith("text-"):
        return "text"
    return "image"


def flush_queue():
    while True:
        with lock:
            if not queue:
                return
            event = queue[0]
        try:
            post("/api/devices/play", json.dumps(event).encode())
        except OSError:
            return  # offline: keep the event, retry later
        with lock:
            queue.pop(0)


def item_changed(path):
    now = time.time()
    with lock:
        prev, started = state["now_playing"], state["started"]
        # pairing and "no content" screens are not plays
        if prev and started and not os.path.basename(prev).startswith(("pairing-", "empty")):
            queue.append({"item": os.path.basename(prev), "kind": kind_of(prev),
                          "start": started, "duration": round(now - started, 2)})
            del queue[:-5000]  # bound memory if offline for a long time
        state["now_playing"], state["started"] = path, (now if path else None)
    flush_queue()


def watch_player():
    """Follow mpv's current file through its IPC socket; reconnect if mpv restarts."""
    while True:
        try:
            s = socket.socket(socket.AF_UNIX)
            s.connect(MPV_SOCK)
            s.sendall(b'{"command":["observe_property",1,"path"]}\n')
            buf = b""
            while True:
                chunk = s.recv(4096)
                if not chunk:
                    break
                buf += chunk
                while b"\n" in buf:
                    line, buf = buf.split(b"\n", 1)
                    msg = json.loads(line or b"{}")
                    if msg.get("event") == "property-change" and msg.get("name") == "path":
                        item_changed(msg.get("data"))
        except (OSError, ValueError):
            pass
        item_changed(None)  # player gone: close the current play record
        time.sleep(3)


def mpv_command(*args):
    s = socket.socket(socket.AF_UNIX)
    s.settimeout(10)
    s.connect(MPV_SOCK)
    s.sendall(json.dumps({"command": list(args)}).encode() + b"\n")
    s.recv(4096)
    s.close()


def read(path, default=""):
    try:
        with open(path) as f:
            return f.read().strip()
    except OSError:
        return default


def health():
    mem = dict(l.split(":", 1) for l in read("/proc/meminfo").splitlines() if ":" in l)
    mem_total = int(mem.get("MemTotal", "0 kB").split()[0]) // 1024
    mem_avail = int(mem.get("MemAvailable", "0 kB").split()[0]) // 1024
    st = os.statvfs("/")
    wifi = read("/proc/net/wireless").splitlines()
    signal = wifi[2].split()[3].rstrip(".") + " dBm" if len(wifi) > 2 else "-"
    try:
        u = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        u.connect(("1.1.1.1", 53))
        ip = u.getsockname()[0]
        u.close()
    except OSError:
        ip = "-"
    with lock:
        now = os.path.basename(state["now_playing"]) if state["now_playing"] else "-"
    return {
        "now_playing": now,
        "uptime": f"{float(read('/proc/uptime', '0').split()[0]) / 3600:.1f} h",
        "temp_c": round(int(read("/sys/class/thermal/thermal_zone0/temp", "0")) / 1000, 1),
        "memory": f"{mem_total - mem_avail} / {mem_total} MB used",
        "disk": f"{(st.f_blocks - st.f_bavail) * st.f_frsize // 2**20} / {st.f_blocks * st.f_frsize // 2**20} MB used",
        "wifi_signal": signal,
        "ip": ip,
        "model": read("/proc/device-tree/model").rstrip("\x00"),
        "queued_reports": len(queue),
    }


def heartbeat_loop():
    while True:
        try:
            post("/api/devices/heartbeat", json.dumps(health()).encode())
            flush_queue()
        except OSError:
            pass
        time.sleep(HEARTBEAT)


def screenshot_loop():
    time.sleep(10)
    while True:
        try:
            mpv_command("screenshot-to-file", SHOT_RAW, "video")
            subprocess.run(["ffmpeg", "-v", "error", "-y", "-i", SHOT_RAW, "-vf", "scale=960:-2",
                            "-q:v", "6", SHOT_SMALL], check=True, timeout=60)
            with open(SHOT_SMALL, "rb") as f:
                with lock:
                    item = os.path.basename(state["now_playing"] or "")
                post("/api/devices/screenshot", f.read(), "image/jpeg", {"X-Item": item})
        except (OSError, subprocess.SubprocessError):
            pass
        time.sleep(SCREENSHOT)


if __name__ == "__main__":
    threading.Thread(target=heartbeat_loop, daemon=True).start()
    threading.Thread(target=screenshot_loop, daemon=True).start()
    watch_player()
