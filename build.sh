#!/bin/bash
# Usage: ./build.sh [rpi4|zero2w] [make target]   (local Docker build; CI does the same on GitHub)
set -euo pipefail
cd "$(dirname "$0")"
BOARD="${1:-zero2w}"
case "$BOARD" in
  rpi4)   DEFCONFIG=raspberrypi4_64_defconfig ;;
  zero2w) DEFCONFIG=raspberrypizero2w_64_defconfig ;;
  *) echo "unknown board: $BOARD"; exit 1 ;;
esac
BR_VERSION="${BR_VERSION:-2025.02.3}"
docker build -t scholartechos-build docker
mkdir -p out
docker run --rm -it -v "$PWD":/work/src -v scholartech-br-cache:/work/cache scholartechos-build bash -c '
  set -e
  [ -d /work/cache/buildroot ] || git clone --depth 1 --branch '"$BR_VERSION"' https://gitlab.com/buildroot.org/buildroot.git /work/cache/buildroot
  cd /work/cache/buildroot
  export BR2_EXTERNAL=/work/src
  O=/work/cache/out-'"$BOARD"'
  make O=$O '"$DEFCONFIG"'
  cat /work/src/configs/common.config /work/src/configs/'"$BOARD"'.config >> $O/.config
  make O=$O olddefconfig
  make O=$O '"${2:-}"'
  cp $O/images/sdcard.img /work/src/out/scholartechos-'"$BOARD"'.img 2>/dev/null || true
'
