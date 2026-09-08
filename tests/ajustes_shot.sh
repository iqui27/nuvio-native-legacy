#!/bin/bash
# Capturas da tela de Ajustes e da folha de fileiras, em BMP, sem interacao.
# Nao entra na suite: precisa de janela GL e de olho humano para julgar.
#
#   bash tests/ajustes_shot.sh /tmp/nuvio-ajustes-antes
set -eu
cd "$(dirname "$0")/.."
sources=()
for source in src/*.c; do
  if [ "$source" != src/main.c ]; then sources+=("$source"); fi
done
cc "${sources[@]}" tests/ajustes_shot.c -Isrc -o /tmp/nuvio-ajustes-shot \
  -O1 -g -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -framework OpenGL \
  -Wno-deprecated-declarations -Wno-macro-redefined
/tmp/nuvio-ajustes-shot "$@"
