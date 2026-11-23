#!/bin/bash
# #739: prov= persiste pela troca de provedor (linka ajustes.c de verdade).
set -eu
cd "$(dirname "$0")/.."
sources=()
for source in src/*.c src/dts/*.c; do
  case "$source" in src/main.c|src/ajustes.c) continue;; esac
  sources+=("$source")
done
binary="$(mktemp ${TMPDIR:-/tmp}/nuvio-d1-739.XXXXXX)"
trap 'rm -f "$binary"' EXIT
cc -DAJUSTES_TESTE "${sources[@]}" tests/d1_poster_migrar_real.c -Isrc -o "$binary" \
  -O1 -g -Wno-deprecated-declarations -Wno-macro-redefined \
  $(pkg-config --cflags --libs sdl2 SDL2_image SDL2_ttf 2>/dev/null || echo "-lSDL2 -lSDL2_image -lSDL2_ttf") -lz -lm -lpthread -lGL
"$binary"
printf 'd1_poster_migrar.sh: ok\n'
