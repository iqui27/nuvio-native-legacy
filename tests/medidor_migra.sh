#!/bin/bash
# Medidor de desempenho: a chave antiga migra para a forma na ilha. Ver o .c.
#
#   bash tests/medidor_migra.sh
set -eu
cd "$(dirname "$0")/.."
sources=()
for source in src/*.c src/dts/*.c; do
  case "$source" in src/main.c|src/ajustes.c) continue;; esac
  sources+=("$source")
done
cc "${sources[@]}" tests/medidor_migra.c -Isrc -o ${TMPDIR:-/tmp}/nuvio-medidor-migra \
  -O1 -g -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wno-deprecated-declarations -Wno-macro-redefined
${TMPDIR:-/tmp}/nuvio-medidor-migra
