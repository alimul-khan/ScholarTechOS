# ScholarTechOS

Minimal Buildroot-based OS for Raspberry Pi. Current target: **Pi 4 (64-bit, 8 GB)**.

Features: boot logo (psplash), Python 3, mpv/ffmpeg for images and video, Wi-Fi, SSH (dropbear). Boots to a text login. A kiosk browser is planned as an optional layer (`board/rpi4/optional-kiosk`).

## Build
Requires Docker. The default image has no WebKit, so builds are much faster than with the kiosk.

    ./build.sh

Result: `out/scholartechos.img`. Flash with Raspberry Pi Imager ("Use custom") or `dd`.

## Customise
- Logo: replace `board/rpi4/logo.png`
- Packages: `configs/scholartech_rpi4_fragment.config`

## Not done yet
4K/8K video input and orientation splitting (planned, Pi 4/5 only, 4K max).
