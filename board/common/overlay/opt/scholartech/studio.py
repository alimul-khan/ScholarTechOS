#!/usr/bin/env python3
"""ScholarTechOS studio: runs the screen player and a password-protected web page for managing
the content while it plays (add, remove, reorder, edit slides). Standard library only.

Slide types: text, image, text+image, video. Each slide is rendered once into a 1920x1080 frame
(text drawn on the device with its own fonts) and played by mpv in a loop. Changes reach the
screen without restarting the player.

Cloud: if /etc/scholartech/cloud.conf has SERVER=<EZ Ads site>, the screen registers there, shows a
pairing code until a manager links it to a SPOT, then plays that SPOT's approved ads (downloaded here,
so it keeps playing offline) and fills empty slots with an "advertise here" screen. The local page is
then read-only.

Data:   /var/lib/scholartech/{playlist.json, media/, render/}
Config: /etc/scholartech/admin.conf  PASSWORD=...   (local web login, user "admin")
        /etc/scholartech/cloud.conf  SERVER=..., DEVICE=..., TOKEN=...  (DEVICE/TOKEN written here)
"""
import base64
import hashlib
import html
import json
import os
import re
import secrets
import shutil
import socket
import subprocess
import textwrap
import threading
import time
import urllib.error
import urllib.request
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from urllib.parse import parse_qs, unquote, urlparse

DATA = "/var/lib/scholartech"
MEDIA = f"{DATA}/media"
RENDER = f"{DATA}/render"
PLAYLIST = f"{DATA}/playlist.json"
MPV_SOCK = "/run/mpv.sock"
APP_CONF = "/etc/scholartech/app.conf"  # APP=ads (EZ Ads player) or APP=prayer (prayer display)
# Boot animation per app, played once at power-on: (file, seconds)
SPLASH = {"ads": ("/usr/share/scholartech/splash.gif", 4.0), "prayer": ("/usr/share/scholartech/splash-prayer.gif", 5.0)}
SPLASH_HOLD = 1.5  # seconds the finished logo stays before the content starts
PRAYER_URL = "http://127.0.0.1:8420/player.html"  # served by scholartech-prayer.service
ADMIN_CONF = "/etc/scholartech/admin.conf"
CLOUD_CONF = "/etc/scholartech/cloud.conf"
CLOUD_POLL = 10
FONT = "/usr/share/fonts/dejavu/DejaVuSans.ttf"
FONT_BOLD = "/usr/share/fonts/dejavu/DejaVuSans-Bold.ttf"
W, H = 1920, 1080
IMAGE_EXT = (".jpg", ".jpeg", ".png", ".webp")
VIDEO_EXT = (".mp4", ".mov", ".mkv", ".webm")
MAX_UPLOAD = 1024 * 1024 * 1024  # 1 GB
TYPES = {"text": "Text", "image": "Image", "text+image": "Text + image", "video": "Video"}

lock = threading.RLock()
cloud = {"linked": False, "pairing": None, "server": ""}  # pairing = frame shown until linked
for d in (MEDIA, RENDER):
    os.makedirs(d, exist_ok=True)


def sh(*cmd, timeout=120):
    return subprocess.run(cmd, capture_output=True, text=True, timeout=timeout)


def password():
    try:
        for line in open(ADMIN_CONF):
            if line.startswith("PASSWORD="):
                return line.strip().split("=", 1)[1]
    except FileNotFoundError:
        pass
    return None


# ------------------------------------------------------------------ playlist storage
def load():
    try:
        with open(PLAYLIST) as f:
            return json.load(f)
    except (FileNotFoundError, json.JSONDecodeError):
        return {"items": []}


def save(pl):
    tmp = PLAYLIST + ".new"
    with open(tmp, "w") as f:
        json.dump(pl, f, indent=2)
    os.replace(tmp, PLAYLIST)


def slug(t, n=40):
    return re.sub(r"[^a-z0-9]+", "-", (t or "").lower()).strip("-")[:n] or "slide"


# ------------------------------------------------------------------ rendering
def esc(t):
    return t.replace("\\", "\\\\").replace("'", "’").replace(":", "\\:").replace("%", "\\%")


def text_filters(x, y, title, body, title_size, body_size, width_chars, center):
    """drawtext filters for a title and wrapped body lines; returns (filters, height)."""
    out, cy = [], y
    for line in textwrap.wrap(title, width_chars) or [""]:
        xpos = "(w-tw)/2" if center else str(x)
        out.append(f"drawtext=fontfile={FONT_BOLD}:text='{esc(line)}':x={xpos}:y={cy}:"
                   f"fontsize={title_size}:fontcolor=white")
        cy += int(title_size * 1.25)
    cy += int(body_size * 0.6)
    for line in textwrap.wrap(body, int(width_chars * title_size / body_size)):
        xpos = "(w-tw)/2" if center else str(x)
        out.append(f"drawtext=fontfile={FONT}:text='{esc(line)}':x={xpos}:y={cy}:"
                   f"fontsize={body_size}:fontcolor=0x9cc8ff")
        cy += int(body_size * 1.35)
    return out, cy - y


def render(item):
    """Render one slide to a frame file; returns its path (videos play as they are)."""
    kind, ident = item["type"], item["id"]
    name = f"{kind.replace('+', '-')}-{slug(item.get('title') or item.get('name') or item.get('file'))}-{ident[:4]}"
    if kind == "video":
        return f"{MEDIA}/{item['file']}"
    out = f"{RENDER}/{name}.jpg"
    key = hashlib.sha1(json.dumps(item, sort_keys=True).encode()).hexdigest()
    if os.path.exists(out) and item.get("_rendered") == key:
        return out
    bg = f"color=c=0x0b1020:s={W}x{H}"
    if kind == "promo":
        r = sh(*promo_cmd(item, out))
        if r.returncode != 0:
            raise RuntimeError(r.stderr.strip()[-300:] or "render failed")
        item["_rendered"] = key
        return out
    if kind == "text":
        # estimate the block height so the text is vertically centred
        _, hgt = text_filters(0, 0, item.get("title", ""), item.get("body", ""), 96, 52, 30, True)
        f, _ = text_filters(0, (H - hgt) // 2, item.get("title", ""), item.get("body", ""), 96, 52, 30, True)
        cmd = ["ffmpeg", "-v", "error", "-y", "-f", "lavfi", "-i", bg, "-vf", ",".join(f), "-frames:v", "1", out]
    elif kind == "image":
        src = f"{MEDIA}/{item['file']}"
        graph = (f"[0]split[a][b];[a]scale={W}:{H}:force_original_aspect_ratio=increase,crop={W}:{H},"
                 f"gblur=sigma=40,drawbox=c=black@0.3:t=fill[bg];"
                 f"[b]scale={W}:{H}:force_original_aspect_ratio=decrease[fg];[bg][fg]overlay=(W-w)/2:(H-h)/2")
        cmd = ["ffmpeg", "-v", "error", "-y", "-i", src, "-filter_complex", graph, "-frames:v", "1", out]
    else:  # text+image: picture on the left, text on the right
        src = f"{MEDIA}/{item['file']}"
        _, hgt = text_filters(0, 0, item.get("title", ""), item.get("body", ""), 64, 36, 18, False)
        f, _ = text_filters(1180, (H - hgt) // 2, item.get("title", ""), item.get("body", ""), 64, 36, 18, False)
        graph = (f"[0]scale=1060:900:force_original_aspect_ratio=decrease[p];"
                 f"[1][p]overlay=60+(1060-w)/2:(H-h)/2," + ",".join(f))
        cmd = ["ffmpeg", "-v", "error", "-y", "-i", src, "-f", "lavfi", "-i", bg,
               "-filter_complex", graph, "-frames:v", "1", out]
    r = sh(*cmd)
    if r.returncode != 0:
        raise RuntimeError(r.stderr.strip()[-300:] or "render failed")
    item["_rendered"] = key
    return out


def promo_cmd(item, out):
    """Default 'advertise here' slides, each in its own colour template (gradient, shading, border)."""
    v = item["variant"]
    grad = lambda a, b: ["-f", "lavfi", "-i", f"gradients=s={W}x{H}:c0={a}:c1={b}:x0=0:y0=0:x1={W}:y1={H}:nb_colors=2"]
    center = lambda t, font, size, col, y: (f"drawtext=fontfile={font}:text='{esc(t)}':x=(w-tw)/2:y={y}:"
                                            f"fontsize={size}:fontcolor={col}")
    left = lambda t, font, size, col, y: f"drawtext=fontfile={font}:text='{esc(t)}':x=150:y={y}:fontsize={size}:fontcolor={col}"
    if v == "here":  # bold orange with a thick white frame
        f = ["vignette=PI/5", f"drawbox=x=50:y=50:w={W - 100}:h={H - 100}:color=white:t=24",
             center("Show your", FONT_BOLD, 120, "white", 300), center("advertisement here", FONT_BOLD, 120, "white", 440),
             center("Reach everyone who walks past this screen.", FONT, 52, "0xffe9df", 640),
             center(item["site"], FONT_BOLD, 46, "white", 800)]
        return ["ffmpeg", "-v", "error", "-y", *grad("0xf06a3c", "0x9c2d10"), "-vf", ",".join(f), "-frames:v", "1", out]
    if v == "scan":  # navy with a gold frame and the booking QR code
        f = [f"drawbox=x=60:y=60:w={W - 120}:h={H - 120}:color=0xe0a526:t=8",
             left("Scan the QR code", FONT_BOLD, 96, "white", 330),
             left("to put your advertisement", FONT, 54, "0xcfd6e4", 470),
             left("on this screen.", FONT, 54, "0xcfd6e4", 540),
             left("Upload it in minutes.", FONT, 44, "0xe0a526", 660),
             left(item["site"], FONT_BOLD, 40, "0x9cc8ff", 760)]
        cmd = ["ffmpeg", "-v", "error", "-y", *grad("0x16243f", "0x050912")]
        if item.get("qr") and os.path.exists(item["qr"]):
            cmd += ["-i", item["qr"], "-filter_complex",
                    f"[0]vignette=PI/6[b];[1]scale=600:600:flags=neighbor[q];[b][q]overlay=1190:240," + ",".join(f)]
        else:
            cmd += ["-vf", ",".join(f)]
        return cmd + ["-frames:v", "1", out]
    # price: cream with a green double frame
    f = [f"drawbox=x=40:y=40:w={W - 80}:h={H - 80}:color=0x20a464:t=22",
         f"drawbox=x=92:y=92:w={W - 184}:h={H - 184}:color=0x20a464:t=6",
         center("Place your advertisement", FONT_BOLD, 92, "0x2a2116", 250),
         center("for only", FONT, 56, "0x5c4f3d", 400),
         center(f"{item['price']} per day", FONT_BOLD, 170, "0x168a52", 490),
         center(f"Book this screen at {item['site']}", FONT, 44, "0x5c4f3d", 780)]
    return ["ffmpeg", "-v", "error", "-y", *grad("0xfffaf0", "0xeadcc0"), "-vf", "vignette=PI/7," + ",".join(f),
            "-frames:v", "1", out]


def price_per_day(fee_cents, currency):
    amount = fee_cents / 7 / 100
    symbol = {"usd": "$", "cad": "$", "aud": "$", "eur": "€", "gbp": "£"}.get(currency, currency.upper() + " ")
    return f"{symbol}{amount:.0f}" if abs(amount - round(amount)) < 0.005 else f"{symbol}{amount:.2f}"


# ------------------------------------------------------------------ player (mpv)
def mpv(*args):
    s = socket.socket(socket.AF_UNIX)
    s.settimeout(5)
    s.connect(MPV_SOCK)
    s.sendall(json.dumps({"command": list(args)}).encode() + b"\n")
    reply = s.recv(65536).decode().splitlines()
    s.close()
    for line in reply:
        msg = json.loads(line)
        if "error" in msg:
            return msg
    return {}


def placeholder():
    out = f"{RENDER}/empty.jpg"
    if not os.path.exists(out):
        f, _ = text_filters(0, 440, "ScholarTechOS", "No content yet. Add slides in the content manager.",
                            96, 44, 30, True)
        sh("ffmpeg", "-v", "error", "-y", "-f", "lavfi", "-i", f"color=c=0x0b1020:s={W}x{H}",
           "-vf", ",".join(f), "-frames:v", "1", out)
    return out


player = {"entries": None, "proc": None, "changed": threading.Event()}


def push_to_player():
    """Render everything and restart the player with the new playlist (each entry with its own duration).

    The player is restarted rather than given a new playlist while running: on the Pi's KMS display,
    swapping files inside a running mpv can leave the monitor frozen on the old picture."""
    with lock:
        pl = load()
        entries = []
        for item in pl["items"]:
            try:
                entries.append((render(item), item.get("duration", 5)))
                item.pop("_error", None)
            except (RuntimeError, OSError, subprocess.SubprocessError) as e:
                item["_error"] = str(e)
        save(pl)
        if cloud["pairing"]:
            entries = [(cloud["pairing"], 3600)]
        elif not entries:
            entries = [(placeholder(), 3600)]
        if entries == player["entries"]:
            return
        player["entries"] = entries
        if player["proc"] and player["proc"].poll() is None:
            player["proc"].terminate()  # run_player() starts it again with the new entries
        player["changed"].set()


def app_mode():
    return "prayer" if read_conf(APP_CONF).get("APP", "ads").strip().lower() == "prayer" else "ads"


def play_splash(app):
    """The app's boot animation, once: centred, sides filled with its own background colour."""
    path, seconds = SPLASH[app]
    if not os.path.exists(path):
        return
    p = subprocess.Popen(["mpv", "--vo=gpu", "--gpu-context=drm", "--fs", "--no-input-terminal", "--really-quiet",
                          "--background=#FDFDFD", "--keep-open=yes", "--loop-file=no", path],
                         stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    time.sleep(seconds + SPLASH_HOLD)
    p.terminate()
    p.wait()


def wait_for(url, seconds):
    end = time.time() + seconds
    while time.time() < end:
        try:
            urllib.request.urlopen(url, timeout=3).close()
            return True
        except OSError:
            time.sleep(1)
    return False


def run_prayer():
    """Prayer display: the three pages (prayer / notices / prayer + notices) in the kiosk browser,
    fullscreen, from the local prayer server. Restarted if it ever exits."""
    while True:
        cog = shutil.which("cog")
        if not cog:
            # This image has no web browser (the advertisement image). Say so instead of a blank screen.
            msg = f"{RENDER}/prayer-needs-browser.jpg"
            if not os.path.exists(msg):
                f, _ = text_filters(0, 380, "Prayer display ready",
                                    "This screen needs the prayer version of ScholarTechOS (with the web browser) "
                                    "to show the pages. Settings: http://<this screen>:8420/manage.html", 84, 42, 32, True)
                sh("ffmpeg", "-v", "error", "-y", "-f", "lavfi", "-i", f"color=c=0x0b1020:s={W}x{H}",
                   "-vf", ",".join(f), "-frames:v", "1", msg)
            p = subprocess.Popen(["mpv", "--vo=gpu", "--gpu-context=drm", "--fs", "--no-input-terminal",
                                  "--really-quiet", "--image-display-duration=inf", msg],
                                 stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
            p.wait()
            time.sleep(5)
            continue
        wait_for(PRAYER_URL, 60)
        os.makedirs("/run/kiosk", mode=0o700, exist_ok=True)
        env = dict(os.environ, XDG_RUNTIME_DIR="/run/kiosk", HOME="/run/kiosk",
                   LIBSEAT_BACKEND="builtin", WLR_LIBINPUT_NO_DEVICES="1")
        if shutil.which("cage"):
            cmd = ["cage", "-s", "--", cog, "--platform=wl", PRAYER_URL]   # inside the cage compositor (tested)
        else:
            cmd = [cog, "--platform=drm", PRAYER_URL]          # draws straight to the display
        p = subprocess.Popen(cmd, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, env=env)
        p.wait()
        time.sleep(3)


def run_player():
    """Keep mpv running with the current playlist; restart it whenever the playlist changes."""
    app = app_mode()
    play_splash(app)
    if app == "prayer":
        return run_prayer()
    while True:
        if player["entries"] is None:
            player["changed"].wait(8)  # give the cloud check a moment to decide what to show first
            if player["entries"] is None:
                push_to_player()
            continue
        player["changed"].clear()
        try:
            os.unlink(MPV_SOCK)
        except FileNotFoundError:
            pass
        files = []
        for path, dur in player["entries"]:
            files += ["--{", f"--image-display-duration={dur}", path, "--}"]
        player["proc"] = subprocess.Popen(
            ["mpv", "--vo=gpu", "--gpu-context=drm", "--fs", "--no-input-terminal", "--hwdec=v4l2m2m-copy",
             "--loop-playlist=inf", "--really-quiet", f"--input-ipc-server={MPV_SOCK}", *files],
            stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        player["proc"].wait()
        if not player["changed"].is_set():
            time.sleep(2)  # crashed rather than replaced: brief pause before restarting


def now_playing():
    try:
        s = socket.socket(socket.AF_UNIX)
        s.settimeout(2)
        s.connect(MPV_SOCK)
        s.sendall(b'{"command":["get_property","path"]}\n')
        path = json.loads(s.recv(4096).decode().splitlines()[0]).get("data") or ""
        s.close()
        return path
    except (OSError, ValueError, IndexError):
        return ""


# ------------------------------------------------------------------ cloud (EZ Ads website)
def read_conf(path):
    conf = {}
    try:
        for line in open(path):
            if "=" in line and not line.lstrip().startswith("#"):
                k, v = line.strip().split("=", 1)
                conf[k] = v
    except FileNotFoundError:
        pass
    return conf


def request(conf, path, body=None, method="GET", auth=True, timeout=20):
    req = urllib.request.Request(conf["SERVER"].rstrip("/") + path, data=body, method=method)
    req.add_header("Content-Type", "application/json")
    if auth and conf.get("DEVICE"):
        req.add_header("X-Device", conf["DEVICE"])
        req.add_header("X-Token", conf["TOKEN"])
    return urllib.request.urlopen(req, timeout=timeout)


def api(conf, path, body=None, method="GET", auth=True):
    with request(conf, path, body, method, auth) as r:
        return json.loads(r.read())


def register(conf):
    model = open("/proc/device-tree/model").read().strip("\x00")
    serial = open("/proc/device-tree/serial-number").read().strip("\x00")
    r = api(conf, "/api/devices/register", json.dumps({"serial": serial, "model": model}).encode(), "POST")
    conf.update(DEVICE=r["device"], TOKEN=r["token"])
    tmp = CLOUD_CONF + ".new"
    with open(tmp, "w") as f:
        f.write("".join(f"{k}={v}\n" for k, v in conf.items()))
    os.chmod(tmp, 0o600)
    os.replace(tmp, CLOUD_CONF)


def pairing_frame(conf, status):
    """Full-screen 'link this screen' page: code, address and (when the server has it) a QR code."""
    qr = f"{RENDER}/pair-qr-{status['code']}.png"
    has_qr = os.path.exists(qr)
    if not has_qr:
        try:
            with request(conf, "/api/devices/pair-qr") as r:
                data = r.read()
            with open(qr, "wb") as f:
                f.write(data)
            has_qr = True
        except OSError:
            pass
    out = f"{RENDER}/pairing-{status['code']}{'-qr' if has_qr else ''}.jpg"
    if os.path.exists(out):
        return out
    lines = [("Link this screen", FONT_BOLD, 84, "white", 230),
             ("Scan the code with your phone," if has_qr else "On your phone or computer,", FONT, 46, "0xd0d8ee", 400),
             ("or go to" if has_qr else "go to", FONT, 46, "0xd0d8ee", 470),
             (status["link"], FONT_BOLD, 46, "0x9cc8ff", 540),
             ("and enter this code:", FONT, 46, "0xd0d8ee", 610),
             (status["code"], FONT_BOLD, 120, "white", 700)]
    f = ",".join(f"drawtext=fontfile={font}:text='{esc(t)}':x=140:y={y}:fontsize={size}:fontcolor={col}"
                 for t, font, size, col, y in lines)
    bg = ["-f", "lavfi", "-i", f"color=c=0x0b1020:s={W}x{H}"]
    if has_qr:
        cmd = ["ffmpeg", "-v", "error", "-y", *bg, "-i", qr, "-filter_complex",
               f"[1]scale=560:560:flags=neighbor[q];[0][q]overlay=1220:260,{f}", "-frames:v", "1", out]
    else:
        cmd = ["ffmpeg", "-v", "error", "-y", *bg, "-vf", f, "-frames:v", "1", out]
    sh(*cmd)
    return out


EXT = {"image/jpeg": ".jpg", "image/png": ".png", "image/webp": ".webp", "video/mp4": ".mp4"}


def sync(conf, status, playlist):
    """Download the SPOT's approved ads (one file per booking), then switch the playlist over at once."""
    spot = playlist["spot"]
    items = []
    for ad in playlist["items"]:
        ident = re.sub(r"[^a-z0-9]", "", ad["id"].lower())
        existing = [n for n in os.listdir(MEDIA) if n.startswith(f"ad-{ident}.")]
        if existing:
            fname = existing[0]
        else:
            with request(conf, ad["url"]) as r:
                ext = EXT.get(r.headers.get("Content-Type", "").split(";")[0], "")
                if not ext:
                    continue
                fname = f"ad-{ident}{ext}"
                expected = int(r.headers.get("Content-Length") or 0)
                tmp, size = f"{MEDIA}/{fname}.part", 0
                with open(tmp, "wb") as f:
                    while chunk := r.read(1 << 20):
                        f.write(chunk)
                        size += len(chunk)
            if expected and size != expected:
                os.unlink(tmp)
                raise OSError(f"incomplete download for {fname}")
            os.replace(tmp, f"{MEDIA}/{fname}")
        kind = "video" if fname.endswith(".mp4") else "image"
        item = {"id": ident, "type": kind, "file": fname, "name": f"ad-{ident}"}
        if kind == "image":
            item["duration"] = spot["secondsPerAd"]
        items.append(item)
    # Fewer than 10 ads: add the default "advertise here" slides (one with a QR code to book this SPOT).
    if len(items) < 10:
        qr = f"{RENDER}/book-qr.png"
        try:
            with request(conf, "/api/devices/book-qr") as r:
                data = r.read()
            with open(qr, "wb") as f:
                f.write(data)
        except OSError:
            qr = ""
        common = {"type": "promo", "site": status["site"], "duration": spot["secondsPerAd"]}
        items += [{**common, "id": "promohere", "variant": "here", "name": "promo-here"},
                  {**common, "id": "promoscan", "variant": "scan", "name": "promo-scan", "qr": qr,
                   "qrsig": hashlib.sha1(playlist.get("bookUrl", "").encode()).hexdigest()[:8]},
                  {**common, "id": "promoprice", "variant": "price", "name": "promo-price",
                   "price": price_per_day(spot.get("feeCents", 700), spot.get("currency", "usd"))}]
    with lock:
        old = {i["id"]: i for i in load()["items"]}
        for it in items:
            prev = old.get(it["id"], {})
            if {k: v for k, v in prev.items() if not k.startswith("_")} == it and "_rendered" in prev:
                it["_rendered"] = prev["_rendered"]  # unchanged slide: reuse its rendered frame
        save({"version": signature(status, playlist), "items": items})
        used = {i.get("file") for i in items}
        for name in os.listdir(MEDIA):  # remove ads that are no longer booked
            if name not in used and not name.endswith(".part"):
                os.unlink(f"{MEDIA}/{name}")


def signature(status, playlist):
    spot = playlist["spot"]
    key = [status["spot"]["displayToken"], spot["secondsPerAd"], spot["totalSlots"], spot.get("feeCents"),
           playlist.get("bookUrl"), [a["id"] for a in playlist["items"]]]
    return hashlib.sha1(json.dumps(key).encode()).hexdigest()[:16]


def cloud_loop():
    while True:
        conf = read_conf(CLOUD_CONF)
        if not conf.get("SERVER"):
            return  # no server configured: local management only
        cloud["server"] = conf["SERVER"]
        try:
            if not conf.get("DEVICE"):
                register(conf)
            status = api(conf, "/api/devices/status")
            if not status["linked"]:
                frame = pairing_frame(conf, status)
                if cloud["pairing"] != frame:
                    cloud["pairing"], cloud["linked"] = frame, False
                    push_to_player()
            else:
                was_pairing = cloud["pairing"] is not None
                cloud["linked"], cloud["pairing"] = True, None
                playlist = api(conf, f"/api/display/{status['spot']['displayToken']}", auth=False)
                if load().get("version") != signature(status, playlist):
                    sync(conf, status, playlist)
                    push_to_player()
                elif was_pairing:
                    push_to_player()
        except urllib.error.HTTPError as e:
            if e.code == 401:  # this device was removed on the server: register again
                conf.pop("DEVICE", None)
                conf.pop("TOKEN", None)
                with open(CLOUD_CONF, "w") as f:
                    f.write(f"SERVER={conf['SERVER']}\n")
        except (OSError, ValueError, KeyError):
            pass  # offline or server error: keep playing what we have
        time.sleep(CLOUD_POLL)


# ------------------------------------------------------------------ web page
CSS = """:root{--bg:#0b1020;--card:#151c33;--fg:#e8ecf5;--mut:#93a0bd;--acc:#4da0ff;--bad:#ff6b6b}
*{box-sizing:border-box}body{margin:0;padding:16px;background:var(--bg);color:var(--fg);font:16px/1.45 system-ui,sans-serif;max-width:960px;margin:auto}
h1{font-size:22px;margin:4px 0 2px}h2{font-size:18px;margin:28px 0 10px}.mut{color:var(--mut);font-size:14px}
.item{display:grid;grid-template-columns:160px 1fr auto;gap:12px;align-items:center;background:var(--card);border-radius:10px;padding:10px;margin:8px 0}
.item.now{outline:2px solid var(--acc)}.item img{width:160px;aspect-ratio:16/9;object-fit:cover;border-radius:6px;background:#000}
.err{color:var(--bad);font-size:13px}.btns{display:flex;flex-wrap:wrap;gap:6px;justify-content:flex-end}
button,.btn{background:#24305a;color:var(--fg);border:0;border-radius:7px;padding:8px 12px;font-size:14px;cursor:pointer;text-decoration:none}
button.del{background:#5a2430}button.go{background:var(--acc);color:#04122b;font-weight:600}
form.card{background:var(--card);border-radius:10px;padding:14px;display:grid;gap:10px}
input,textarea,select{width:100%;padding:10px;border-radius:7px;border:1px solid #2a3558;background:#0f1530;color:var(--fg);font-size:16px}
label{color:var(--mut);font-size:14px}.row{display:grid;grid-template-columns:1fr 120px;gap:10px}
@media(max-width:600px){.item{grid-template-columns:110px 1fr}.item img{width:110px}.btns{grid-column:1/-1;justify-content:flex-start}}"""

ADD_FORM = """<h2>Add a slide</h2>
<form class="card" id="add" onsubmit="return addSlide(event)">
<label>Type<select name="type" id="type" onchange="fields()">
<option value="text">Text</option><option value="image">Image</option>
<option value="text+image">Text + image</option><option value="video">Video</option></select></label>
<label class="f-file">File<input type="file" name="file" id="file"></label>
<label class="f-text">Title<input name="title" maxlength="80"></label>
<label class="f-text">Text<textarea name="body" rows="3" maxlength="300"></textarea></label>
<label class="f-dur">Show for (seconds)<input name="duration" type="number" min="1" max="3600" value="5"></label>
<button class="go" type="submit" id="addbtn">Add slide</button><div class="mut" id="status"></div></form>
<script>
function fields(){const t=document.getElementById('type').value;
 document.querySelectorAll('.f-file').forEach(e=>e.style.display=(t=='text')?'none':'');
 document.querySelectorAll('.f-text').forEach(e=>e.style.display=(t=='image'||t=='video')?'none':'');
 document.querySelectorAll('.f-dur').forEach(e=>e.style.display=(t=='video')?'none':'');
 document.getElementById('file').accept=t=='video'?'video/*':'image/*';}
fields();
async function addSlide(ev){ev.preventDefault();const f=ev.target,st=document.getElementById('status');
 const data=Object.fromEntries(new FormData(f));const file=document.getElementById('file').files[0];
 if(data.type!='text'&&!file){st.textContent='Choose a file first.';return false}
 document.getElementById('addbtn').disabled=true;
 if(file){st.textContent='Uploading '+file.name+'…';
  const r=await fetch('/upload?name='+encodeURIComponent(file.name),{method:'POST',body:file});
  if(!r.ok){st.textContent='Upload failed: '+await r.text();document.getElementById('addbtn').disabled=false;return false}
  data.file=await r.text();}
 delete data.file_input;st.textContent='Adding…';
 const r=await fetch('/add',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(data)});
 if(r.ok)location.reload();else{st.textContent='Failed: '+await r.text();document.getElementById('addbtn').disabled=false}
 return false}
</script>"""


class Studio(BaseHTTPRequestHandler):
    def log_message(self, *a):
        pass

    def authed(self):
        pw = password()
        got = self.headers.get("Authorization", "")
        if pw and got.startswith("Basic "):
            try:
                user, _, given = base64.b64decode(got[6:]).decode().partition(":")
                if secrets.compare_digest(given, pw):
                    return True
            except ValueError:
                pass
        self.send_response(401)
        self.send_header("WWW-Authenticate", 'Basic realm="ScholarTechOS content manager"')
        self.send_header("Content-Length", "0")
        self.end_headers()
        return False

    def send(self, code, body, ctype="text/html; charset=utf-8", headers=()):
        data = body if isinstance(body, bytes) else body.encode()
        self.send_response(code)
        for k, v in headers:
            self.send_header(k, v)
        self.send_header("Content-Type", ctype)
        self.send_header("Content-Length", str(len(data)))
        self.send_header("Cache-Control", "no-store")
        self.end_headers()
        self.wfile.write(data)

    def back(self):
        self.send(303, "", headers=[("Location", "/")])

    def do_GET(self):
        if not self.authed():
            return
        url = urlparse(self.path)
        if url.path.startswith("/thumb/"):
            ident = url.path[7:]
            item = next((i for i in load()["items"] if i["id"] == ident), None)
            if item and item["type"] == "video":
                path = f"{RENDER}/thumb-{ident}.jpg"
                if not os.path.exists(path):
                    sh("ffmpeg", "-v", "error", "-y", "-ss", "1", "-i", f"{MEDIA}/{item['file']}",
                       "-frames:v", "1", "-vf", "scale=480:-2", path)
            else:
                path = render(item) if item else ""
            if path and os.path.exists(path):
                return self.send(200, open(path, "rb").read(), "image/jpeg")
            return self.send(404, "no preview")
        if url.path == "/edit":
            return self.edit_page(parse_qs(url.query).get("id", [""])[0])
        self.send(200, self.index())

    def index(self):
        items = load()["items"]
        playing = now_playing()
        rows = []
        if cloud["linked"]:
            rows.append(f'<p class="mut">This screen is managed online at {html.escape(cloud["server"])}. '
                        'Changes made there appear here automatically.</p>')
        for n, it in enumerate(items):
            e = html.escape(it["id"])
            label = it.get("title") or it.get("file", "")
            now = playing and (playing.endswith(it.get("file", "\0")) or it["id"][:4] in os.path.basename(playing))
            err = f'<div class="err">{html.escape(it["_error"])}</div>' if it.get("_error") else ""
            dur = "full video" if it["type"] == "video" else f'{it.get("duration", 5)} s'
            rows.append(f"""<div class="item{' now' if now else ''}"><img src="/thumb/{e}?v={html.escape(it.get('_rendered', '')[:8])}" alt="">
<div><b>{n + 1}. {html.escape(label)}</b><div class="mut">{TYPES[it['type']]} · {dur}{' · now showing' if now else ''}</div>{err}</div>
<div class="btns"><form method="post" action="/move?id={e}&d=-1"><button title="Move up">↑</button></form>
<form method="post" action="/move?id={e}&d=1"><button title="Move down">↓</button></form>
<a class="btn" href="/edit?id={e}">Edit</a>
<form method="post" action="/delete?id={e}" onsubmit="return confirm('Delete this slide?')"><button class="del">Delete</button></form></div></div>""")
        body = "".join(rows) or '<p class="mut">No slides yet.</p>'
        return (f"<!doctype html><html><head><meta charset=utf-8><meta name=viewport content='width=device-width,initial-scale=1'>"
                f"<title>ScholarTechOS content</title><style>{CSS}</style></head><body>"
                f"<h1>ScholarTechOS content</h1><div class=mut>{len(items)} slides, playing in a loop. Changes show on the screen right away.</div>"
                f"<h2>Playlist</h2>{body}{'' if cloud['linked'] else ADD_FORM}</body></html>")

    def edit_page(self, ident):
        it = next((i for i in load()["items"] if i["id"] == ident), None)
        if not it:
            return self.back()
        e = html.escape
        text = "" if it["type"] in ("image", "video") else (
            f'<label>Title<input name="title" maxlength="80" value="{e(it.get("title", ""))}"></label>'
            f'<label>Text<textarea name="body" rows="3" maxlength="300">{e(it.get("body", ""))}</textarea></label>')
        dur = "" if it["type"] == "video" else (
            f'<label>Show for (seconds)<input name="duration" type="number" min="1" max="3600" value="{it.get("duration", 5)}"></label>')
        self.send(200, f"""<!doctype html><html><head><meta charset=utf-8><meta name=viewport content='width=device-width,initial-scale=1'>
<title>Edit slide</title><style>{CSS}</style></head><body><p><a class="btn" href="/">← Back</a></p><h1>Edit slide</h1>
<form class="card" method="post" action="/update?id={e(ident)}"><img src="/thumb/{e(ident)}" style="width:100%;border-radius:8px" alt="">
<div class="mut">{TYPES[it['type']]}{' · ' + e(it.get('file', '')) if it.get('file') else ''}</div>{text}{dur}
<button class="go">Save</button></form></body></html>""")

    def do_POST(self):
        if not self.authed():
            return
        if cloud["linked"]:
            return self.send(403, "This screen is managed online at " + cloud["server"], "text/plain")
        url = urlparse(self.path)
        q = {k: v[0] for k, v in parse_qs(url.query).items()}
        n = int(self.headers.get("Content-Length", 0))
        if url.path == "/upload":
            return self.upload(unquote(q.get("name", "file")), n)
        raw = self.rfile.read(min(n, 64 * 1024)).decode()
        with lock:
            pl = load()
            items = pl["items"]
            idx = next((k for k, i in enumerate(items) if i["id"] == q.get("id")), None)
            if url.path == "/add":
                d = json.loads(raw)
                kind = d.get("type")
                if kind not in TYPES or (kind != "text" and not d.get("file")):
                    return self.send(400, "missing type or file")
                item = {"id": secrets.token_hex(4), "type": kind}
                if kind != "text":
                    item["file"] = os.path.basename(d["file"])
                if kind in ("text", "text+image"):
                    item["title"], item["body"] = d.get("title", "")[:80], d.get("body", "")[:300]
                if kind != "video":
                    item["duration"] = max(1, min(3600, int(d.get("duration") or 5)))
                items.append(item)
            elif idx is None:
                return self.back()
            elif url.path == "/delete":
                gone = items.pop(idx)
                if gone.get("file") and not any(i.get("file") == gone["file"] for i in items):
                    try:
                        os.unlink(f"{MEDIA}/{gone['file']}")
                    except FileNotFoundError:
                        pass
            elif url.path == "/move":
                j = idx + (1 if q.get("d") == "1" else -1)
                if 0 <= j < len(items):
                    items[idx], items[j] = items[j], items[idx]
            elif url.path == "/update":
                form = {k: v[0] for k, v in parse_qs(raw, keep_blank_values=True).items()}
                it = items[idx]
                if "title" in form:
                    it["title"], it["body"] = form["title"][:80], form.get("body", "")[:300]
                if "duration" in form:
                    it["duration"] = max(1, min(3600, int(form["duration"] or 5)))
            save(pl)
        threading.Thread(target=push_to_player, daemon=True).start()
        if url.path == "/add":
            return self.send(200, "ok", "text/plain")
        self.back()

    def upload(self, name, n):
        base, ext = os.path.splitext(os.path.basename(name))
        ext = ext.lower()
        if ext not in IMAGE_EXT + VIDEO_EXT:
            self.rfile.read(n)
            return self.send(400, "Only images (jpg, png, webp) and videos (mp4, mov, mkv, webm).", "text/plain")
        if n > MAX_UPLOAD:
            return self.send(413, "File too large (1 GB max).", "text/plain")
        fname = f"{slug(base)}-{secrets.token_hex(2)}{ext}"
        tmp = f"{MEDIA}/.{fname}.part"
        with open(tmp, "wb") as f:
            left = n
            while left:
                chunk = self.rfile.read(min(left, 1 << 20))
                if not chunk:
                    break
                f.write(chunk)
                left -= len(chunk)
        if left:
            os.unlink(tmp)
            return self.send(400, "Upload interrupted.", "text/plain")
        os.replace(tmp, f"{MEDIA}/{fname}")
        self.send(200, fname, "text/plain")


if __name__ == "__main__":
    if not password():
        os.makedirs(os.path.dirname(ADMIN_CONF), exist_ok=True)
        with open(ADMIN_CONF, "w") as f:
            f.write(f"PASSWORD={secrets.token_urlsafe(9)}\n")
        os.chmod(ADMIN_CONF, 0o600)
    threading.Thread(target=run_player, daemon=True).start()
    if app_mode() == "ads":  # the prayer display is managed by its own settings page, not EZ Ads
        threading.Thread(target=cloud_loop, daemon=True).start()
    ThreadingHTTPServer(("0.0.0.0", 80), Studio).serve_forever()
