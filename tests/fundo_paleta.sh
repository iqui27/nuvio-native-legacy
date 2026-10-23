#!/bin/bash
set -eu
cd "$(dirname "$0")/.."
sources=()
for source in src/*.c src/dts/*.c; do
  case "$source" in src/main.c|src/ajustes.c) continue;; esac
  sources+=("$source")
done
binary="$(mktemp "${TMPDIR:-/tmp}/nuvio-fundo-paleta.XXXXXX")"
trap 'rm -f "$binary"' EXIT
cc "${sources[@]}" tests/fundo_paleta.c -Isrc -o "$binary" \
  -DNV_TOUCH_UI -O1 -g -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wno-deprecated-declarations -Wno-macro-redefined
"$binary"
