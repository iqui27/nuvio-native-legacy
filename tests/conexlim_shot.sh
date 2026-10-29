#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
sources=()
for source in src/*.c src/dts/*.c; do [ "$source" = src/main.c ] || sources+=("$source"); done
cc "${sources[@]}" tests/conexlim_shot.c -Isrc -DAJUSTES_TESTE \
  -o "$work/shot" -O1 -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wno-deprecated-declarations -Wno-macro-redefined
mkdir -p /Volumes/ExternalSSD/tmp/rev/conexlim
NUVIO_DADOS="$work" NUVIO_TESTE_DIR="$work" "$work/shot" /Volumes/ExternalSSD/tmp/rev/conexlim
