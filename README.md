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

## Wi-Fi and SSH without a keyboard
After flashing, the SD card's first (boot) partition shows up on any computer. Put these files in it:

- `wifi.txt` (see `board/rpi4/wifi.txt.example`):

      SSID=YourNetwork
      PASSWORD=YourPassword
      COUNTRY=US

- `authorized_keys`: your SSH public key (e.g. copy of `~/.ssh/id_ed25519.pub`).

On every boot the Pi reads them, connects to Wi-Fi and allows SSH as `root` with that key:

    ssh root@scholartech.local   # or use the IP from your router
