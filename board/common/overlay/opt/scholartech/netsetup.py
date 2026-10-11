#!/usr/bin/env python3
"""ScholarTechOS first-time Wi-Fi setup, for screens with no keyboard or mouse.

If the device has no working Wi-Fi it creates its own setup hotspot and shows a QR code on
the screen. A phone scans it, joins the hotspot, and a setup page opens where the user picks
their Wi-Fi and types the password. The device then connects and shows the result.

Modes:
  netsetup.py auto          boot: wait for Wi-Fi; if none, run the hotspot setup (final image)
  netsetup.py lan-test      prototype: serve the setup page on the current network, no hotspot
"""
import html
import json
import os
import re
import subprocess
import sys
import threading
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from urllib.parse import parse_qs

IFACE = "wlan0"
AP_IP = "192.168.4.1"
WPA_CONF = f"/etc/wpa_supplicant/wpa_supplicant-{IFACE}.conf"
RUN = "/run/netsetup"
FONT = "/usr/share/fonts/dejavu/DejaVuSans.ttf"
FONT_BOLD = "/usr/share/fonts/dejavu/DejaVuSans-Bold.ttf"
CONNECT_WAIT = 45  # seconds to wait for an IP address after joining a network

state = {"mode": None, "networks": [], "message": "", "ap_ssid": "", "ap_pass": "", "url": ""}


def sh(*cmd, check=False, timeout=30):
    return subprocess.run(cmd, capture_output=True, text=True, check=check, timeout=timeout)


# ---------------------------------------------------------------- network state
def wifi_ip():
    """IPv4 address on the Wi-Fi interface, ignoring our own hotspot address."""
    out = sh("ip", "-4", "-o", "addr", "show", IFACE).stdout
    for ip in re.findall(r"inet (\d+\.\d+\.\d+\.\d+)", out):
        if ip != AP_IP:
            return ip
    return None


def wait_for_ip(seconds):
    end = time.time() + seconds
    while time.time() < end:
        ip = wifi_ip()
        if ip:
            return ip
        time.sleep(2)
    return None


def scan():
    """Nearby networks as [(ssid, signal_dbm)], strongest first. Needs `iw`."""
    try:
        out = sh("iw", "dev", IFACE, "scan", timeout=20).stdout
    except (OSError, subprocess.SubprocessError):
        return []
    nets, signal = {}, None
    for line in out.splitlines():
        line = line.strip()
        if line.startswith("signal:"):
            signal = float(line.split()[1])
        elif line.startswith("SSID:"):
            ssid = line[5:].strip()
            if ssid and (ssid not in nets or signal > nets[ssid]):
                nets[ssid] = signal
    return sorted(nets.items(), key=lambda n: -n[1])


def save_wifi(ssid, password):
    """Write the client config the same way scholartech-config does (router-compatible)."""
    os.makedirs(os.path.dirname(WPA_CONF), exist_ok=True)
    if password:
        out = sh("wpa_passphrase", ssid, password, check=True).stdout
        lines = [l for l in out.splitlines() if not l.strip().startswith("#psk")]
        lines.insert(2, "key_mgmt=WPA-PSK WPA-PSK-SHA256")
        lines.insert(3, "ieee80211w=1")
        conf = "\n".join(lines) + "\n"
    else:
        conf = 'network={\n\tssid=%s\n\tkey_mgmt=NONE\n}\n' % json.dumps(ssid)
    tmp = WPA_CONF + ".new"
    with open(tmp, "w") as f:
        f.write(conf)
    os.chmod(tmp, 0o600)
    os.replace(tmp, WPA_CONF)


def join_wifi():
    sh("systemctl", "enable", f"wpa_supplicant@{IFACE}")
    sh("systemctl", "restart", f"wpa_supplicant@{IFACE}")
    return wait_for_ip(CONNECT_WAIT)


# ---------------------------------------------------------------- hotspot (final image)
def start_hotspot():
    serial = open("/proc/device-tree/serial-number").read().strip("\x00")[-4:].upper()
    state["ap_ssid"] = f"ScholarTech-Setup-{serial}"
    state["ap_pass"] = "".join(str(int(c, 16) % 10) for c in os.urandom(4).hex())  # 8 digits
    os.makedirs(RUN, exist_ok=True)
    with open(f"{RUN}/hostapd.conf", "w") as f:
        f.write(f"interface={IFACE}\ndriver=nl80211\nssid={state['ap_ssid']}\nhw_mode=g\nchannel=6\n"
                f"wpa=2\nwpa_key_mgmt=WPA-PSK\nrsn_pairwise=CCMP\nwpa_passphrase={state['ap_pass']}\n")
    with open(f"{RUN}/dnsmasq.conf", "w") as f:
        # hand out addresses and answer every DNS name with ourselves -> phones open the portal
        f.write(f"interface={IFACE}\nbind-interfaces\ndhcp-range=192.168.4.10,192.168.4.50,10m\n"
                f"address=/#/{AP_IP}\nno-resolv\n")
    sh("systemctl", "stop", f"wpa_supplicant@{IFACE}")
    sh("ip", "addr", "flush", "dev", IFACE)
    sh("ip", "addr", "add", f"{AP_IP}/24", "dev", IFACE)
    sh("ip", "link", "set", IFACE, "up")
    subprocess.Popen(["hostapd", f"{RUN}/hostapd.conf"], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    subprocess.Popen(["dnsmasq", "-k", "-C", f"{RUN}/dnsmasq.conf"], stdout=subprocess.DEVNULL,
                     stderr=subprocess.DEVNULL)


def stop_hotspot():
    sh("killall", "hostapd")
    sh("killall", "dnsmasq")
    sh("ip", "addr", "del", f"{AP_IP}/24", "dev", IFACE)


# ---------------------------------------------------------------- screen
def qr_png(text, out):
    """QR code PNG: python-segno on the device; falls back to a pre-made file for prototypes."""
    try:
        import segno
        segno.make(text, error="m").save(out, scale=16, border=2, dark="#000", light="#fff")
        return True
    except ImportError:
        return os.path.exists(out)


def esc(t):
    return t.replace("\\", "\\\\").replace("'", "’").replace(":", "\\:").replace("%", "\\%")


def show(title, lines, qrs=()):
    """Render a full-screen 1920x1080 status page and show it with mpv (replaces any player)."""
    os.makedirs(RUN, exist_ok=True)
    out = f"{RUN}/screen.png"
    inputs, filt, y = ["-f", "lavfi", "-i", "color=c=0x0b1020:s=1920x1080"], [], 150
    filt.append(f"drawtext=fontfile={FONT_BOLD}:text='{esc(title)}':x=120:y={y}:fontsize=72:fontcolor=white")
    y += 150
    for ln in lines:
        filt.append(f"drawtext=fontfile={FONT}:text='{esc(ln)}':x=120:y={y}:fontsize=40:fontcolor=0xd0d8ee")
        y += 70
    graph = "[0]" + ",".join(filt) + "[t]"
    last = "t"
    for i, (png, caption) in enumerate(qrs):
        inputs += ["-i", png]
        x = 1240 + i * 0  # one QR column on the right; captions under each
        top = 160 + i * 460
        graph += f";[{i + 1}]scale=380:380[q{i}];[{last}][q{i}]overlay={x}:{top},drawtext=fontfile={FONT}:" \
                 f"text='{esc(caption)}':x={x}:y={top + 395}:fontsize=30:fontcolor=0x4da0ff[o{i}]"
        last = f"o{i}"
    sh("ffmpeg", "-v", "error", "-y", *inputs, "-filter_complex", graph, "-map", f"[{last}]",
       "-frames:v", "1", out, timeout=60)
    sh("killall", "mpv")
    subprocess.Popen(["mpv", "--vo=gpu", "--gpu-context=drm", "--fs", "--no-input-terminal",
                      "--image-display-duration=inf", "--really-quiet", out],
                     stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)


def show_setup():
    url = state["url"]
    qrs = []
    if state["mode"] == "hotspot":
        join = f"WIFI:T:WPA;S:{state['ap_ssid']};P:{state['ap_pass']};;"
        if qr_png(join, f"{RUN}/qr-join.png"):
            qrs.append((f"{RUN}/qr-join.png", "1. Scan to join setup Wi-Fi"))
        lines = [f"Wi-Fi:  {state['ap_ssid']}", f"Password:  {state['ap_pass']}",
                 "", f"Then open  {url}", "and choose your Wi-Fi network."]
    else:
        lines = ["Open this page on a phone on the same Wi-Fi:", url, "",
                 "Choose the Wi-Fi network and enter its password."]
    if qr_png(url, f"{RUN}/qr-url.png"):
        qrs.append((f"{RUN}/qr-url.png", "2. Scan to open setup" if qrs else "Scan to open setup"))
    if state["message"]:
        lines += ["", state["message"]]
    show("Set up Wi-Fi", lines, qrs)


# ---------------------------------------------------------------- phone setup page
PAGE = """<!doctype html><html><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1"><title>ScholarTechOS setup</title><style>
body{{margin:0;padding:20px;background:#0b1020;color:#e8ecf5;font:17px/1.5 system-ui,sans-serif}}
h1{{font-size:22px}} label{{display:block;margin:16px 0 6px;color:#93a0bd}}
select,input,button{{width:100%;box-sizing:border-box;font-size:17px;padding:12px;border-radius:8px;border:1px solid #2a3558;background:#151c33;color:#e8ecf5}}
button{{background:#4da0ff;color:#04122b;border:0;font-weight:600;margin-top:22px}}
.msg{{background:#151c33;padding:12px;border-radius:8px;margin-top:16px}}
</style></head><body><h1>Connect this screen to Wi-Fi</h1>{msg}
<form method="post" action="/connect">
<label for="net">Network</label><select id="net" name="ssid">{options}<option value="">Other (type below)</option></select>
<label for="other">Other network name</label><input id="other" name="other" autocomplete="off" autocapitalize="off">
<label for="pw">Password</label><input id="pw" name="password" type="password" autocomplete="off">
<button type="submit">Connect</button></form></body></html>"""


class Portal(BaseHTTPRequestHandler):
    def log_message(self, *a):
        pass

    def send(self, code, body, ctype="text/html; charset=utf-8", headers=()):
        data = body.encode()
        self.send_response(code)
        for k, v in headers:
            self.send_header(k, v)
        self.send_header("Content-Type", ctype)
        self.send_header("Content-Length", str(len(data)))
        self.send_header("Cache-Control", "no-store")
        self.end_headers()
        self.wfile.write(data)

    def do_GET(self):
        if self.path.split("?")[0] not in ("/", "/setup"):
            # captive-portal probes (generate_204, hotspot-detect.html, ...) -> send to setup
            return self.send(302, "", headers=[("Location", state["url"])])
        opts = "".join(f'<option value="{html.escape(s)}">{html.escape(s)}'
                       f'{f" ({int(sig)} dBm)" if sig else ""}</option>' for s, sig in state["networks"])
        msg = f'<p class="msg">{html.escape(state["message"])}</p>' if state["message"] else ""
        self.send(200, PAGE.format(options=opts, msg=msg))

    def do_POST(self):
        n = int(self.headers.get("Content-Length", 0))
        form = parse_qs(self.rfile.read(min(n, 4096)).decode(), keep_blank_values=True)
        ssid = (form.get("other", [""])[0].strip() or form.get("ssid", [""])[0].strip())[:32]
        password = form.get("password", [""])[0]
        if not ssid or (password and not 8 <= len(password) <= 63):
            state["message"] = "Choose a network. Wi-Fi passwords are 8 to 63 characters."
            return self.send(303, "", headers=[("Location", "/")])
        self.send(200, f"<meta name=viewport content='width=device-width'><body style='background:#0b1020;"
                       f"color:#e8ecf5;font:18px system-ui;padding:20px'><h2>Connecting to "
                       f"{html.escape(ssid)}…</h2><p>Watch the screen for the result.</p>")
        threading.Thread(target=apply, args=(ssid, password), daemon=True).start()


def apply(ssid, password):
    show("Connecting…", [f"Joining  {ssid}", "This can take up to a minute."])
    time.sleep(1)
    try:
        save_wifi(ssid, password)
    except subprocess.CalledProcessError:
        state["message"] = "That password could not be used. Try again."
        return show_setup()
    if state["mode"] == "hotspot":
        stop_hotspot()
    ip = join_wifi()
    if ip:
        show("Connected", [f"Wi-Fi:  {ssid}", f"Address:  {ip}", "", "Starting ScholarTechOS…"])
        time.sleep(6)
        os._exit(0)  # the player service is ordered after this one and starts now
    state["message"] = f"Could not connect to {ssid}. Check the password and try again."
    if state["mode"] == "hotspot":
        start_hotspot()
    show_setup()


def serve(port):
    ThreadingHTTPServer(("0.0.0.0", port), Portal).serve_forever()


if __name__ == "__main__":
    mode = sys.argv[1] if len(sys.argv) > 1 else "auto"
    if mode == "lan-test":
        state["mode"], port = "lan", 8081
        state["url"] = f"http://{wifi_ip()}:{port}/"
        state["networks"] = scan() or [(l.split('"')[1], 0) for l in open(WPA_CONF) if "ssid=" in l]
        show_setup()
        serve(port)
    else:
        # A saved network gets time to connect after boot; with none saved, go to setup quickly.
        if wait_for_ip(90 if os.path.exists(WPA_CONF) else 10):
            sys.exit(0)  # Wi-Fi works: nothing to do, the player starts normally
        state["mode"], state["url"] = "hotspot", f"http://{AP_IP}/"
        state["networks"] = scan()  # scan before the radio switches to hotspot mode
        start_hotspot()
        show_setup()
        serve(80)
