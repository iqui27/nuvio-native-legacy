#!/bin/bash
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
# Capturas do icone do app (apoiadores) no login e na barra. Precisa de janela
# GL e de olho humano; nao entra na suite.
set -eu
cd "$(dirname "$0")/.."
sources=()
for source in src/*.c src/dts/*.c; do
  if [ "$source" != src/main.c ]; then sources+=("$source"); fi
done
cc -DAJUSTES_TESTE "${sources[@]}" tests/iconeapp_shot.c -Isrc -o ${TMPDIR:-/tmp}/nuvio-iconeapp-shot \
  -O1 -g -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wno-deprecated-declarations -Wno-macro-redefined
${TMPDIR:-/tmp}/nuvio-iconeapp-shot "$@"
