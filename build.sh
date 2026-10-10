#!/bin/bash
# Usage: ./build.sh            build the Pi 4 image inside Docker
#        ./build.sh menuconfig  tweak the config (saved back to the fragment manually)
set -euo pipefail
cd "$(dirname "$0")"
BR_VERSION="${BR_VERSION:-2025.02.3}"
docker build -t scholartechos-build docker
mkdir -p out
docker run --rm -it -v "$PWD":/work/src -v scholartech-br-cache:/work/cache scholartechos-build bash -c '
  set -e
  [ -d /work/cache/buildroot ] || git clone --depth 1 --branch '"$BR_VERSION"' https://gitlab.com/buildroot.org/buildroot.git /work/cache/buildroot
  cd /work/cache/buildroot
  export BR2_EXTERNAL=/work/src
  make O=/work/cache/out-rpi4 raspberrypi4_64_defconfig
  cat /work/src/configs/scholartech_rpi4_fragment.config >> /work/cache/out-rpi4/.config
  make O=/work/cache/out-rpi4 olddefconfig
  make O=/work/cache/out-rpi4 '"${1:-}"'
  cp /work/cache/out-rpi4/images/sdcard.img /work/src/out/ 2>/dev/null || true
'
