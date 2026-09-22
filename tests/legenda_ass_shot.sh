#!/bin/bash
# Capturas do overlay ASS. Nao entra na suite (*_shot.sh): janela GL.
#   bash tests/legenda_ass_shot.sh /tmp/nuvio-legass
set -eu
cd "$(dirname "$0")/.."
sources=()
for source in src/*.c; do [ "$source" != src/main.c ] && sources+=("$source"); done
cc "${sources[@]}" tests/legenda_ass_shot.c -Isrc -o /tmp/nuvio-legass-shot \
  -O1 -g -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wno-deprecated-declarations -Wno-macro-redefined
D=$(mktemp -d); trap 'rm -rf "$D"' EXIT
NUVIO_DADOS="$D" NUVIO_TESTE_DIR="$D" /tmp/nuvio-legass-shot "$@"
