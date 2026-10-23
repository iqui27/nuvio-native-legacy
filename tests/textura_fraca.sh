#!/bin/bash
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
# Textura fraca: a troca pelo material CC0 leva a TINTA e o VEU do material,
# nao do recorte da arte que saiu. Ver tests/textura_fraca.c.
#
#   bash tests/textura_fraca.sh
set -eu
cd "$(dirname "$0")/.."
sources=()
for source in src/*.c src/dts/*.c; do
  case "$source" in src/main.c|src/ajustes.c) continue;; esac
  sources+=("$source")
done
cc "${sources[@]}" tests/textura_fraca.c -Isrc -o ${TMPDIR:-/tmp}/nuvio-textura-fraca \
  -O1 -g -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wno-deprecated-declarations -Wno-macro-redefined
${TMPDIR:-/tmp}/nuvio-textura-fraca
