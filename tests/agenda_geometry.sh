#!/bin/bash
set -eu
cd "$(dirname "$0")/.."
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
cc tests/agenda_geometry.c src/layout.c -DNV_TOUCH_UI -O1 -Isrc \
  -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -Wno-deprecated-declarations -Wno-macro-redefined \
  -Wl,-dead_strip -Wl,-undefined,dynamic_lookup -o "$work/agenda-geometry"
"$work/agenda-geometry"
