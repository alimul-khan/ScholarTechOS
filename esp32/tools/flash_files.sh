#!/bin/sh
# Writes the settings page (build/www.bin) to the board.
# Never touches the "data" partition, so the screen keeps its settings and pictures.
# Usage: tools/flash_files.sh [port]     (build them first: node tools/build_files.mjs <assets folder>)
set -e
cd "$(dirname "$0")/.."
PORT=${1:-$(ls /dev/cu.usbmodem* 2>/dev/null | head -1)}
PY="$HOME/.platformio/penv/bin/python"
ESPTOOL="$HOME/.platformio/packages/tool-esptoolpy/esptool.py"
"$PY" "$ESPTOOL" --chip esp32s3 -p "$PORT" -b 921600 write_flash 0xBA0000 build/www.bin
