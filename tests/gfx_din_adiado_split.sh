#!/bin/bash
# Requires a native GL window; no TV or network.
set -eu
cd "$(dirname "$0")/.."
tmp=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-din-adiado.XXXXXX")
trap 'rm -rf "$tmp"' EXIT
sources=()
for source in src/*.c src/dts/*.c; do
  case "$source" in src/main.c|src/gfx.c) ;; *) sources+=("$source");; esac
done
cc "${sources[@]}" tests/gfx_din_adiado_split.c -Isrc -o "$tmp/teste" \
  -O1 -g -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wno-deprecated-declarations -Wno-macro-redefined
"$tmp/teste"
