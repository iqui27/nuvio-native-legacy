#!/bin/bash
# Padroes de fabrica de ajustes.c (vetor posicional `valor[]`). Ver o .c.
#
#   bash tests/ajustes_padroes.sh
set -eu
cd "$(dirname "$0")/.."
sources=()
for source in src/*.c; do
  case "$source" in src/main.c|src/ajustes.c) continue;; esac
  sources+=("$source")
done
cc "${sources[@]}" tests/ajustes_padroes.c -Isrc -o /tmp/nuvio-ajustes-padroes \
  -O1 -g -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wno-deprecated-declarations -Wno-macro-redefined
/tmp/nuvio-ajustes-padroes
