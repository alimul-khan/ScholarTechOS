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

## Build
GitHub Actions builds both boards on every push to `main` (artifacts `scholartechos-rpi4`, `scholartechos-zero2w`).
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
