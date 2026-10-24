#!/bin/sh
# Makes the home page's fonts (src/fonts/serif_*.c) from DejaVu Serif, the Home Display app's font.
set -e
cd "$(dirname "$0")/../.."
T=tools/home_art
TEXT=0x20-0x7E,0xA0-0xFF,0x2013-0x2014,0x2018-0x201D,0x2022,0x2026,0x100-0x17F
conv() { npx --yes lv_font_conv@1.5.3 --no-compress --no-prefilter --bpp 4 --format lvgl --lv-include lvgl.h "$@"; }
conv --size 70 --font $T/Serif-Bold.ttf -r 0x30-0x3A -o src/fonts/serif_clock.c --lv-font-name serif_clock
conv --size 22 --font $T/Serif-Bold.ttf -r 0x20,0x30-0x3A,0x41,0x4D,0x50 -o src/fonts/serif_time.c --lv-font-name serif_time
conv --size 10 --font $T/Serif-Bold.ttf -r 0x20,0x41,0x4D,0x50 -o src/fonts/serif_ampm.c --lv-font-name serif_ampm
conv --size 30 --font $T/Serif-Bold.ttf -r $TEXT -o src/fonts/serif_b30.c --lv-font-name serif_b30
conv --size 18 --font $T/Serif-Bold.ttf -r $TEXT -o src/fonts/serif_b18.c --lv-font-name serif_b18
conv --size 17 --font $T/Serif-Regular.ttf -r $TEXT -o src/fonts/serif_r17.c --lv-font-name serif_r17
conv --size 11 --font $T/Serif-Regular.ttf -r $TEXT -o src/fonts/serif_r11.c --lv-font-name serif_r11
conv --size 64 --font $T/Serif-Bold.ttf -r 0x25,0x2D,0x30-0x39,0x3A,0x43,0xB0 -o src/fonts/serif_temp.c --lv-font-name serif_temp
# Icons (Font Awesome 5): gear, wifi, map marker, mosque, volume, moon, close, left, right, plus, minus, check, play, stop, search
FA=managed_components/lvgl__lvgl/scripts/built_in_font/FontAwesome5-Solid+Brands+Regular.woff
conv --size 26 --font $FA -r 0xF013,0xF1EB,0xF3C5,0xF678,0xF028,0xF186,0xF00D,0xF053,0xF054,0xF067,0xF068,0xF00C,0xF04B,0xF04D,0xF002 -o src/fonts/icons_26.c --lv-font-name icons_26
