#!/bin/bash
# Issue #132: a folha de Fontes lista tudo que os addons mandaram, nos dois
# modos de "Fonte automatica". Ver tests/fontes_lista.c.
#
#   bash tests/fontes_lista.sh
set -eu
cd "$(dirname "$0")/.."
sources=()
for source in src/*.c; do
  [ "$source" != src/main.c ] && sources+=("$source")
done
cc "${sources[@]}" tests/fontes_lista.c -Isrc -o /tmp/nuvio-fontes-lista \
  -O1 -g -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wno-deprecated-declarations -Wno-macro-redefined
/tmp/nuvio-fontes-lista
