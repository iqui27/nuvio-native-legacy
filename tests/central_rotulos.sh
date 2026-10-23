#!/bin/bash
# Os nomes dos 27 botoes da Central cabem no botao, nos 30 idiomas, em
# Montserrat. Ver tests/central_rotulos.c (janela GL escondida).
#
#   bash tests/central_rotulos.sh
set -eu
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-central-rotulos.XXXXXX")
trap 'rm -rf "$work"' EXIT
sources=()
for source in src/*.c src/dts/*.c; do [ "$source" != src/main.c ] && sources+=("$source"); done
cc "${sources[@]}" tests/central_rotulos.c -Isrc -o "$work/test" -DNV_SHOT_HOOKS \
  -O1 -g -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wno-deprecated-declarations -Wno-macro-redefined
mkdir -p "$work/dados"
NUVIO_DADOS="$work/dados" "$work/test"
