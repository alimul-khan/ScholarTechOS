#!/bin/bash
# Local build in Docker (the same steps as the GitHub workflow, without using GitHub's minutes).
#
#     ./build.sh [rpi4|zero2w] [ads|prayer] [make target]     e.g. ./build.sh rpi4 prayer
#
# Downloads, the compiler cache and each board/version's build output live in the Docker volume
# "scholartech-br-cache", so a second build only redoes what changed. The image is written to
# out/scholartechos-<board>-<version>.img.
set -euo pipefail
cd "$(dirname "$0")"
BOARD="${1:-rpi4}"
VERSION="${2:-ads}"
TARGET="${3:-}"
case "$BOARD" in
  rpi4)   DEFCONFIG=raspberrypi4_64_defconfig ;;
  zero2w) DEFCONFIG=raspberrypizero2w_64_defconfig ;;
  *) echo "unknown board: $BOARD (rpi4 or zero2w)"; exit 1 ;;
esac
case "$VERSION" in
  ads|prayer) ;;
  *) echo "unknown version: $VERSION (ads or prayer)"; exit 1 ;;
esac
BR_VERSION="${BR_VERSION:-2025.02.3}"
docker build -q -t scholartechos-build docker >/dev/null
mkdir -p out
TTY=$([ -t 0 ] && echo -it || echo -i)
docker run --rm $TTY -v "$PWD":/work/src -v scholartech-br-cache:/work/cache \
  -e BOARD="$BOARD" -e VERSION="$VERSION" -e DEFCONFIG="$DEFCONFIG" -e BR_VERSION="$BR_VERSION" -e TARGET="$TARGET" \
  scholartechos-build bash -c '
  set -e
  [ -d /work/cache/buildroot ] || git clone --depth 1 --branch "$BR_VERSION" https://gitlab.com/buildroot.org/buildroot.git /work/cache/buildroot
  cd /work/cache/buildroot
  export BR2_EXTERNAL=/work/src
  O=/work/cache/out-$BOARD-$VERSION
  FRAGMENTS="configs/common.config configs/$BOARD.config"
  [ "$VERSION" = prayer ] && FRAGMENTS="$FRAGMENTS configs/prayer.config"
  make O=$O $DEFCONFIG
  (cd /work/src && cat $FRAGMENTS) >> $O/.config
  echo "BR2_DL_DIR=\"/work/cache/dl\"" >> $O/.config
  echo "BR2_CCACHE=y" >> $O/.config
  echo "BR2_CCACHE_DIR=\"/work/cache/ccache\"" >> $O/.config
  make O=$O olddefconfig
  # Stop at once if Buildroot silently dropped an option we asked for
  bad=0
  for k in $(cd /work/src && cat $FRAGMENTS | grep -oE "^BR2_[A-Z0-9_]+=y" | cut -d= -f1); do
    grep -q "^$k=y" $O/.config || { echo "Option dropped by Buildroot: $k"; bad=1; }
  done
  [ $bad = 0 ] || exit 1
  make O=$O -j"$(nproc)" $TARGET
  if [ -z "$TARGET" ]; then
    cp $O/images/sdcard.img /work/src/out/scholartechos-$BOARD-$VERSION.img
    echo "Image: out/scholartechos-$BOARD-$VERSION.img"
  fi
'
