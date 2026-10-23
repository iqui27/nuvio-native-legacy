#!/bin/bash
# Fullscreen dubbed trailer failure falls back like autoplay. See tests/trailer-cheia-escada.c.
#
#   bash tests/trailer-cheia-escada.sh
set -eu
cd "$(dirname "$0")/.."
T="$(mktemp -d "${TMPDIR:-/tmp}/nv-trailer-cheia.XXXXXX")"
trap 'rm -rf "$T"' EXIT
sources=()
for source in src/*.c src/dts/*.c; do
  case "$source" in src/main.c|src/detail.c) continue;; esac
  sources+=("$source")
done
cc "${sources[@]}" tests/trailer-cheia-escada.c -Isrc -o "$T/t" \
  -O1 -g -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wno-deprecated-declarations -Wno-macro-redefined
"$T/t"
