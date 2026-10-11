# ScholarTechOS work list

Rule: test everything live on the Pi 4 first; only start an image build when the list below is
ready for it (each build takes several hours). Items marked **[build]** need packages that are not
in the current image, so they can only be finished after the next build.

## Next build must include
- [ ] **[build] Setup hotspot packages**: `hostapd`, `dnsmasq`, `iw`, `python-segno`.
      Then test `netsetup.py auto` end to end: hotspot `ScholarTech-Setup-XXXX` appears, phone joins
      via QR, picks Wi-Fi, device connects, falls back to hotspot on a wrong password.
- [ ] **[build] mDNS** (avahi or similar) so `http://scholartech.local` works instead of the IP.
- [ ] **[build] A/B OS updates** (RAUC or SWUpdate) with signed bundles and automatic rollback.
      Partition layout: boot, rootfs A, rootfs B, data. Must be in the first deployed image.
- [ ] **[build] Data partition** for content, playlist and settings (survives OS updates).
- [ ] **[build] Tailscale** for remote SSH maintenance.
- [ ] **[build] Audio**: ALSA is already in the project (`configs/common.config`); verify HDMI sound
      with `speaker-test` and in videos.
- [ ] **[build] Kiosk browser** (WPE WebKit `cog`): already in the project; verify web pages,
      images, short H.264 videos on Pi 4 and Zero 2 W.
- [ ] Systemd services for `netsetup.py`, `studio.py` (player + content manager) and `agent.py`,
      with the right start order (netsetup → studio → agent).

## Prayer times app (from github.com/alimul-khan/AzanDisplay)
One OS, two apps: the owner picks "Advertising" or "Prayer times" when linking a screen.
- [ ] **[build] ICU with full data + time zones in WPE WebKit**: the display uses `Intl` time zones
      and the Umm al-Qura Hijri calendar. Verify times and Hijri dates in the kiosk first.
- [ ] **[build] `tzdata`** for Python `zoneinfo` (server side).
- [ ] **[build] `adhanpy` and `hijridate`** bundled into the image (pure Python).
- [ ] **[build] Arabic font** (e.g. Noto Naskh Arabic / Amiri).
- [ ] Run AzanDisplay `server/app.py` on the device (port 5501) as the prayer app's local store.
- [ ] Show its HTML display (`index.html` / `portrait.html` / `landscape.html`) in the kiosk.
- [ ] Drop the Tk native display and the phone-hotspot/NetworkManager onboarding; use the
      ScholarTechOS setup hotspot + pairing instead.
- [ ] App choice on the website when linking; device starts the slide player or prayer display.
- [ ] Prayer settings (location, method, iqamah, Jumu'ah, notices, images) managed on the website,
      synced to the device; reuse AzanDisplay `admin.html`.
- [ ] Test on Zero 2 W: performance, portrait rotation, offline operation, cold boot.

## Website / phone
- [ ] Make the website a PWA (icon, manifest, add to home screen, notifications).

## Boot look
- [x] STOS animation (`/usr/share/scholartech/splash.gif`) plays once when the player starts (tested on Pi 4).
- [x] Old psplash logo removed; boot text hidden (`quiet loglevel=3 logo.nologo`), no firmware rainbow
      (`disable_splash=1`), no text login on the screen (getty@tty1 masked; SSH and USB console remain).
      Tested on the Pi 4; in the project for the next build.
- [ ] Check the animation speed on the Zero 2 W (GIF decoding is in software).

## Features to build and test live (no build needed)
- [ ] Remote content updates from the report server (device pulls playlist + files, checksums,
      atomic switch, keeps playing old content while offline).
- [ ] Remote commands from the dashboard (reboot, restart player, screenshot now).
- [ ] Agent config (`SERVER`, `DEVICE`, `TOKEN`) from the boot partition, like `wifi.txt`.
- [ ] Pairing code on screen to link a new device to the dashboard.
- [ ] Wi-Fi fallback: if the saved network fails for several minutes, show the setup screen again.

## Server (reports + remote management)
- [ ] Host on the internet (VPS/cloud) with a domain and HTTPS.
- [ ] Dashboard login.
- [ ] Per-device tokens issued through pairing instead of by hand.

## Security
- [ ] Root password / local admin password policy for shipped devices.
- [ ] Content manager on HTTPS or restricted to the local network.
