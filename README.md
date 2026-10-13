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

## Prayer screens: set-up and app updates
1. Flash the prayer image, connect the screen to the network.
2. From this Mac: `scripts/provision.sh <screen address> [time zone]`. It copies the read-only deploy key
   (`~/.ssh/scholartech_prayer_deploy`, never in a repo or the image) to the screen.
3. To update every screen: commit in display_template, then tag a release (`git tag v1.1 && git push origin v1.1`).
   Screens check hourly, install the newest `vX.Y` tag, reload by themselves, and go back to the previous
   version if the new one does not start. `ssh root@<screen> /opt/scholartech/prayer-update --status` shows the version.

## Build
GitHub Actions builds only on demand (Actions → Build ScholarTechOS → Run workflow, then the board and version); a run on a private repo uses paid minutes, and a long run continues where the previous one stopped.
Locally (Docker, no GitHub minutes): `./build.sh rpi4 prayer` (board `rpi4` or `zero2w`, version `ads` or `prayer`); the image is written to `out/`. Downloads, the compiler cache and build output are kept in the Docker volume `scholartech-br-cache`, so later builds only redo what changed.

Flash the image (`out/scholartechos-<board>-<version>.img`, or the `.img.xz` from GitHub) with Raspberry Pi Imager ("Use custom").

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
