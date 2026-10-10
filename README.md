# ScholarTechOS

Minimal Buildroot-based OS for Raspberry Pi. Current target: **Pi 4 (64-bit, 8 GB)**.

Features: boot logo (Plymouth), Python 3, kiosk web browser (cage + Cog), mpv/ffmpeg for images and video, Wi-Fi, SSH (dropbear).

## Build
Requires Docker. First build takes 1-2 hours (WebKit is large).

    ./build.sh

Result: `out/sdcard.img`. Flash with Raspberry Pi Imager ("Use custom") or `dd`.

## Customise
- Logo: replace `board/rpi4/overlay/usr/share/plymouth/themes/scholartech/logo.png`
- Kiosk URL: `board/rpi4/overlay/etc/scholartech/kiosk.conf`
- Packages: `configs/scholartech_rpi4_fragment.config`

## Not done yet
4K/8K video input and orientation splitting (planned, Pi 4/5 only, 4K max).
