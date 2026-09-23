#!/bin/bash
# Captura isolada do toast de avisos, com tema Ocean e sem servico de rede.
set -eu
cd "$(dirname "$0")/.."
NUVIO_DADOS=$(mktemp -d /tmp/nuvio-avisos-toast-dados.XXXXXX)
export NUVIO_DADOS
trap 'rm -rf "$NUVIO_DADOS"' EXIT
sources=()
for source in src/*.c; do
  if [ "$source" != src/main.c ] && [ "$source" != src/avisos.c ]; then
    sources+=("$source")
  fi
done
cc "${sources[@]}" tests/avisos_toast_shot.c -Isrc -o /tmp/nuvio-avisos-toast-shot \
  -O1 -g -DNV_LEVE -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wno-deprecated-declarations -Wno-macro-redefined
/tmp/nuvio-avisos-toast-shot "${1:-/tmp/nuvio-avisos-toast.bmp}"
