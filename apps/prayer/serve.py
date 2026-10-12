#!/usr/bin/env python3
"""STOS prayer display server: serves the app and keeps its settings and notice images on this
device, so the screen and any phone or computer on the network share one setup.
Standard library only.

    python3 serve.py            # http://<device>:8420/player.html (screen), /manage.html (settings)

Environment: PORT (default 8420), STOS_DATA (default ./data)
"""
import json
import mimetypes
import os
import re
import threading
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
from urllib.parse import unquote, urlparse

ROOT = os.path.dirname(os.path.abspath(__file__))
DATA = os.path.abspath(os.environ.get("STOS_DATA", os.path.join(ROOT, "data")))
MEDIA = os.path.join(DATA, "media")
CONFIG = os.path.join(DATA, "config.json")
MAX_MEDIA = 20 * 1024 * 1024
MAX_CONFIG = 4 * 1024 * 1024
ID_OK = re.compile(r"^[A-Za-z0-9._-]{1,80}$")
# Minimal systems may lack a MIME database; make sure fonts, images and scripts get the right types.
for _ext, _type in ((".woff2", "font/woff2"), (".svg", "image/svg+xml"), (".js", "text/javascript"),
                    (".css", "text/css"), (".json", "application/json"), (".webp", "image/webp")):
    mimetypes.add_type(_type, _ext)
lock = threading.Lock()
os.makedirs(MEDIA, exist_ok=True)


def read_state():
    try:
        with open(CONFIG) as f:
            return json.load(f)
    except (FileNotFoundError, ValueError):
        return {"rev": 0, "config": None}


def write_json(path, data):
    tmp = path + ".tmp"
    with open(tmp, "w") as f:
        json.dump(data, f)
    os.replace(tmp, path)


class Handler(SimpleHTTPRequestHandler):
    def __init__(self, *a, **kw):
        super().__init__(*a, directory=ROOT, **kw)

    def log_message(self, *a):
        pass

    def end_headers(self):
        # Screens must never run stale settings or code.
        self.send_header("Cache-Control", "no-store")
        super().end_headers()

    def send_json(self, code, data):
        body = json.dumps(data).encode()
        self.send_response(code)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def body(self, limit):
        n = int(self.headers.get("Content-Length", 0))
        if n > limit:
            raise ValueError("too large")
        return self.rfile.read(n)

    def media_path(self, ident):
        if not ID_OK.match(ident):
            raise ValueError("bad id")
        return os.path.join(MEDIA, ident)

    def do_GET(self):
        path = urlparse(self.path).path
        if path == "/api/config":
            with lock:
                return self.send_json(200, read_state())
        if path == "/api/media":
            return self.send_json(200, sorted(n for n in os.listdir(MEDIA) if not n.endswith(".type")))
        if path.startswith("/media/"):
            try:
                p = self.media_path(unquote(path[7:]))
            except ValueError:
                return self.send_error(400)
            if not os.path.exists(p):
                return self.send_error(404)
            ctype = open(p + ".type").read() if os.path.exists(p + ".type") else "application/octet-stream"
            data = open(p, "rb").read()
            self.send_response(200)
            self.send_header("Content-Type", ctype)
            self.send_header("Content-Length", str(len(data)))
            self.end_headers()
            return self.wfile.write(data)
        if path.startswith("/data/") or path.endswith(".py"):
            return self.send_error(404)
        return super().do_GET()

    def do_PUT(self):
        if urlparse(self.path).path != "/api/config":
            return self.send_error(404)
        try:
            cfg = json.loads(self.body(MAX_CONFIG))["config"]
        except (ValueError, KeyError):
            return self.send_error(400)
        with lock:
            state = read_state()
            state = {"rev": state["rev"] + 1, "config": cfg}
            write_json(CONFIG, state)
        self.send_json(200, {"rev": state["rev"]})

    def do_POST(self):
        url = urlparse(self.path)
        if url.path != "/api/media":
            return self.send_error(404)
        ident = dict(p.split("=", 1) for p in url.query.split("&") if "=" in p).get("id", "")
        try:
            p = self.media_path(unquote(ident))
            data = self.body(MAX_MEDIA)
        except ValueError:
            return self.send_error(400)
        with open(p, "wb") as f:
            f.write(data)
        with open(p + ".type", "w") as f:
            f.write(self.headers.get("Content-Type") or mimetypes.guess_type(ident)[0] or "application/octet-stream")
        self.send_json(200, {"id": unquote(ident)})

    def do_DELETE(self):
        path = urlparse(self.path).path
        if not path.startswith("/api/media/"):
            return self.send_error(404)
        try:
            p = self.media_path(unquote(path[len("/api/media/"):]))
        except ValueError:
            return self.send_error(400)
        for f in (p, p + ".type"):
            if os.path.exists(f):
                os.unlink(f)
        self.send_json(200, {"ok": True})


if __name__ == "__main__":
    port = int(os.environ.get("PORT", 8420))
    print(f"STOS display server: http://0.0.0.0:{port}/player.html  (settings: /manage.html, data: {DATA})")
    ThreadingHTTPServer(("0.0.0.0", port), Handler).serve_forever()
