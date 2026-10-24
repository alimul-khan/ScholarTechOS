#!/bin/sh
# Makes the screen's fonts (src/fonts/*.c) from Poppins, the display app's font, with lv_font_conv.
# Usage: tools/make_fonts.sh <folder with Poppins-<weight>-latin.ttf and -latin-ext.ttf>
#   (the display app ships .woff2; convert with fontTools: TTFont(f).flavor = None; save as .ttf)
set -e
cd "$(dirname "$0")/.."
T=$1
FA=managed_components/lvgl__lvgl/scripts/built_in_font/FontAwesome5-Solid+Brands+Regular.woff
TEXT=0x20-0x7E,0xA0-0xFF,0x2013-0x2014,0x2018-0x201D,0x2022,0x2026
EXT=0x100-0x17F
# new line, keyboard, wifi, gear, check, close, left, right, warning, refresh, lock, eye, backspace, ok-circle, globe
ICONS=0xF8A2,0xF11C,0xF1EB,0xF013,0xF00C,0xF00D,0xF053,0xF054,0xF071,0xF021,0xF023,0xF06E,0xF55A,0xF058,0xF0AC
mkdir -p src/fonts
conv() { npx --yes lv_font_conv@1.5.3 --no-compress --no-prefilter --bpp 4 --format lvgl --lv-include lvgl.h "$@"; }
conv --size 104 --font "$T/Poppins-600-latin.ttf" -r 0x30-0x3A -o src/fonts/font_clock.c --lv-font-name font_clock
conv --size 46 --font "$T/Poppins-600-latin.ttf" -r 0x20,0x2D,0x30-0x3A,0x41,0x4D,0x50 -o src/fonts/font_count.c --lv-font-name font_count
for spec in "30 600 font_title" "24 600 font_big" "20 500 font_text" "16 500 font_small"; do
  set -- $spec
  conv --size $1 --font "$T/Poppins-$2-latin.ttf" -r $TEXT --font "$T/Poppins-$2-latin-ext.ttf" -r $EXT \
       --font $FA -r $ICONS -o src/fonts/$3.c --lv-font-name $3
done
ls -la src/fonts
