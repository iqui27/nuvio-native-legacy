#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
binary="$(mktemp "${TMPDIR:-/tmp}/nuvio-cw-ok-home.XXXXXX")"
trap 'rm -f "$binary"' EXIT
sources=()
for source in src/*.c src/dts/*.c; do
  case "$source" in src/main.c|src/home.c) continue;; esac
  sources+=("$source")
done
cc "${sources[@]}" tests/cw_ok_home.c -Isrc -o "$binary" \
  -O1 -g -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wno-deprecated-declarations -Wno-macro-redefined
"$binary"
