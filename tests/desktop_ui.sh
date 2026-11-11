#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
WORK=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-desktop-ui.XXXXXXXX")
trap 'rm -rf "$WORK"' EXIT
for source in tests/desktop_ui.c tests/desktop_home.c; do
  cc "$source" src/buscanorm.c -DNV_TOUCH_UI -DNV_DESKTOP_UI \
    -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
    -Wno-deprecated-declarations -ffunction-sections -fdata-sections \
    -Wl,-dead_strip -Wl,-undefined,dynamic_lookup -L/opt/homebrew/lib -lSDL2 -o "$WORK/check"
  "$WORK/check"
done
