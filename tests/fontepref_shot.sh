#!/bin/bash
# Captura da folha de fontes com a marca "Sua escolha anterior" (issue #56).
# Nao entra na suite: precisa de janela GL e de olho humano para julgar.
#
#   bash tests/fontepref_shot.sh /tmp/nuvio-fontepref
set -eu
cd "$(dirname "$0")/.."
sources=()
for source in src/*.c; do
  if [ "$source" != src/main.c ]; then sources+=("$source"); fi
done
cc "${sources[@]}" tests/fontepref_shot.c -Isrc -o /tmp/nuvio-fontepref-shot \
  -O1 -g -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wno-deprecated-declarations -Wno-macro-redefined
/tmp/nuvio-fontepref-shot "$@"
