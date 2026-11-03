#!/bin/bash
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
set -eu
cd "$(dirname "$0")/.."
python3 tests/detail_ep_menu_bounds.py
sources=()
for source in src/*.c src/dts/*.c; do
  if [ "$source" != src/main.c ] && [ "$source" != src/detail.c ]; then sources+=("$source"); fi
done
cc "${sources[@]}" tests/detail_ep_menu_bounds.c -Isrc -o "${TMPDIR}/nuvio-ep-menu-bounds" \
  -O1 -g -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wno-deprecated-declarations -Wno-macro-redefined
"${TMPDIR}/nuvio-ep-menu-bounds"
