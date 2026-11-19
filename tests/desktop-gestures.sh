#!/bin/bash
# Gesture routing runs without creating or activating a window.
set -euo pipefail
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-desktop-gestures.XXXXXXXX")
trap 'rm -rf "$work"' EXIT
cc tests/desktop_input.c -DNV_TOUCH_UI -DNV_DESKTOP_UI -Isrc \
  -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -Wno-deprecated-declarations -ffunction-sections -fdata-sections \
  -Wl,-dead_strip -L/opt/homebrew/lib -lSDL2 -o "$work/input"
"$work/input"
