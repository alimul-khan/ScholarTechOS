#!/bin/sh
# Copy the prayer display app (display_template) into this repo for the prayer image.
# Takes the last commit of display_template, not uncommitted edits, and records which one.
#
#     scripts/update-prayer-app.sh [path-to-display_template]   (default: ../display_template)
set -eu
SRC=${1:-"$(dirname "$0")/../../display_template"}
DEST="$(cd "$(dirname "$0")/.." && pwd)/apps/prayer"
REV=$(git -C "$SRC" describe --tags --always)   # release tag (v1.2) when HEAD is tagged
rm -rf "$DEST"
mkdir -p "$DEST"
# Only what the screen and settings page need (no docs, design notes or screenshots)
git -C "$SRC" archive HEAD -- '*.html' serve.py css js fonts img samples vendor | tar -x -C "$DEST"
echo "$REV" > "$DEST/VERSION"
if [ -n "$(git -C "$SRC" status --porcelain -- '*.html' serve.py css js fonts img samples vendor)" ]; then
    echo "Note: display_template has uncommitted changes; they were not copied."
fi
echo "apps/prayer now has display_template $REV"
