#!/bin/bash
# trailer_recorte: o "cover" da fonte do trailer no destino (trailer.h).
set -eu
cd "$(dirname "$0")/.."
tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT
sources=()
for source in src/*.c src/dts/*.c; do
  case "$source" in src/main.c) continue;; esac
  sources+=("$source")
done
cc "${sources[@]}" tests/trailer-recorte.c -Isrc -o "$tmp/t" -O0 \
  -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 -L/opt/homebrew/lib \
  -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wno-deprecated-declarations -Wno-macro-redefined 2>"$tmp/build.log" || { cat "$tmp/build.log" >&2; exit 1; }
"$tmp/t"
