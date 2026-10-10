#!/bin/sh
# Runs before Buildroot's raspberrypi post-image.sh so genimage packs our cmdline.
CMDLINE="$1/rpi-firmware/cmdline.txt"
grep -q splash "$CMDLINE" || sed -i 's/$/ quiet splash plymouth.ignore-serial-consoles vt.global_cursor_default=0/' "$CMDLINE"
