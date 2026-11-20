#!/bin/bash
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
# Renderer de familias tipograficas e legenda SRT. Janela GL oculta.
#   bash tests/text_familias.sh /tmp/nuvio-fontes
set -eu
cd "$(dirname "$0")/.."
prefix=${1:-${TMPDIR:-/tmp}/nuvio-fontes}
mkdir -p "$(dirname "$prefix")"
sources=()
for source in src/*.c src/dts/*.c; do [ "$source" != src/main.c ] && sources+=("$source"); done
cc "${sources[@]}" tests/text_familias.c -Isrc -o ${TMPDIR:-/tmp}/nuvio-text-familias \
  -O1 -g -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wno-deprecated-declarations -Wno-macro-redefined
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
mkdir -p "$tmp/minimo/fonts" "$tmp/dados"
cp deploy/app/fonts/InterDisplay-{Regular,Medium,Bold}.ttf "$tmp/minimo/fonts/"
NUVIO_DADOS="$tmp/dados" ${TMPDIR:-/tmp}/nuvio-text-familias \
  "$prefix" "$tmp/minimo"
