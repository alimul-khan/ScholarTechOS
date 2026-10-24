#!/bin/sh
# Draws the home prayer page's fixed artwork (src/gen/home_bg.c, preview in build/home_bg.png).
# Usage: tools/home_art/make.sh <HomeDisplay/home-display/app/src/main/assets>
set -e
cd "$(dirname "$0")/../.."
OUT=$(mktemp -d)
javac -d "$OUT" tools/home_art/HomeArt.java
mkdir -p build src/gen
java -Djava.awt.headless=true -cp "$OUT" HomeArt tools/home_art "$1/art/prayer.png" build src/gen
rm -rf "$OUT"
