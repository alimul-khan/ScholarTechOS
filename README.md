# ScholarTechOS

Minimal Buildroot-based kiosk OS for Raspberry Pi.

- **Target:** Raspberry Pi Zero 2 W (512 MB). **Development board:** Raspberry Pi 4.
- Boots with the STOS logo animation straight into a fullscreen web page (WPE WebKit `cog`, no desktop, no compositor).
- Web pages can show images, text and short videos (H.264 MP4, hardware decoded through GStreamer/V4L2).
- Also: Python 3, mpv/ffmpeg, ALSA audio, Wi-Fi, SSH (dropbear, key only), USB serial console.

## Layout
- `configs/common.config`: packages for every board. `configs/<board>.config`: board-specific.
- `board/common/overlay`: files copied into the root filesystem.
- `board/<board>/config.txt`, `cmdline.txt`: boot partition settings.
- `apps/prayer`: the prayer display app, a copy of [display_template](https://github.com/alimul-khan/display_template)
  (version in `apps/prayer/VERSION`). After committing changes there, refresh it with `scripts/update-prayer-app.sh`.
  Installed in the prayer image by `package/scholartech-prayer` (server on port 8420, settings at `/manage.html`).

## Build
GitHub Actions builds the Pi 4 advertisement image on every push to `main` (artifact `scholartechos-rpi4-ads`).
Other images (Zero 2 W, prayer version with the WebKit browser from `configs/prayer.config`) are built on demand:
Actions → Build ScholarTechOS → Run workflow.
Locally (Docker): `./build.sh zero2w` or `./build.sh rpi4`.

Flash `scholartechos-<board>.img.xz` with Raspberry Pi Imager ("Use custom").

## Settings on the boot partition (no keyboard needed)
After flashing, the card's first partition is readable on any computer. Optional files:

| File | Contents |
|---|---|
| `wifi.txt` | `SSID=...`, `PASSWORD=...`, `COUNTRY=..` (see `board/common/wifi.txt.example`) |
| `authorized_keys` | SSH public key(s) for `root` |
| `kiosk.txt` | `URL=https://...` page for the kiosk (default: built-in start page) |

They are applied on every boot. Then: `ssh root@scholartech.local` (or the IP from your router).

## Console without a network
Connect the Pi's USB-C port (Pi 4) or micro-USB "USB" port (Zero 2 W) to a computer: it appears as a
USB serial port (`/dev/cu.usbmodem*` on macOS) with a login prompt. User `root`, no password.

## Video tips
Encode clips as H.264 MP4, 1080p or lower, for hardware decoding on the Zero 2 W.
Autoplaying videos should be `muted` in the HTML.
