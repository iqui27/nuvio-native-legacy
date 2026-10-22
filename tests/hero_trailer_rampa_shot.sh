#!/bin/bash
# Rampas do hero sobre o trailer em janela (#290). Ver tests/hero_trailer_rampa_shot.c.
#
#   bash tests/hero_trailer_rampa_shot.sh [pasta-de-capturas]
set -eu
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-hero-rampa.XXXXXX")
trap 'rm -rf "$work"' EXIT
sources=()
for source in src/*.c src/dts/*.c; do
  [ "$source" != src/main.c ] && sources+=("$source")
done
cc "${sources[@]}" tests/hero_trailer_rampa_shot.c -Isrc -o "$work/test" \
  -O1 -g -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wno-deprecated-declarations -Wno-macro-redefined
mkdir -p "$work/dados"
NUVIO_DADOS="$work/dados" NUVIO_TESTE_DIR="$work/dados" "$work/test" "$@"
