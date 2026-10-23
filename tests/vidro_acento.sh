#!/bin/bash
# Selected/focused pill in glass mode: vivid accent, rim, specular. Ver tests/vidro_acento.c.
#
#   bash tests/vidro_acento.sh [pasta-de-capturas]
set -eu
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-vidro-acento.XXXXXX")
trap 'rm -rf "$work"' EXIT
sources=()
for source in src/*.c src/dts/*.c; do
  [ "$source" != src/main.c ] && sources+=("$source")
done
cc "${sources[@]}" tests/vidro_acento.c -Isrc -o "$work/test" \
  -DAJUSTES_TESTE -O1 -g -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wno-deprecated-declarations -Wno-macro-redefined
mkdir -p "$work/dados"
NUVIO_DADOS="$work/dados" NUVIO_TESTE_DIR="$work/dados" "$work/test" "$@"
