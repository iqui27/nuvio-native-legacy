#!/bin/bash
# #129: idioma de legenda/audio lido do ajustes.txt no arranque. Ver o .c.
#
#   bash tests/ajustes_idioma_disco.sh
set -eu
cd "$(dirname "$0")/.."
sources=()
for source in src/*.c; do
  case "$source" in src/main.c|src/ajustes.c) continue;; esac
  sources+=("$source")
done
cc "${sources[@]}" tests/ajustes_idioma_disco.c -Isrc -o /tmp/nuvio-ajustes-idioma-disco \
  -O1 -g -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wno-deprecated-declarations -Wno-macro-redefined
/tmp/nuvio-ajustes-idioma-disco
