#!/bin/bash
set -eu
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-ondever-sheet.XXXXXX")
NUVIO_DADOS="$work/data";mkdir -p "$NUVIO_DADOS";export NUVIO_DADOS
trap 'rm -rf "$work"' EXIT
sources=()
for source in src/*.c src/dts/*.c;do
  case "$source" in src/main.c|src/ondever.c) continue;;esac
  sources+=("$source")
done
cc "${sources[@]}" tests/ondever_folha.c -Isrc -o "$work/test" -O1 -g \
  -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 -L/opt/homebrew/lib \
  -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wno-deprecated-declarations -Wno-macro-redefined
"$work/test"
