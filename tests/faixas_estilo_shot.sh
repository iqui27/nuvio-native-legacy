#!/bin/bash
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
# Capturas da barra de estilo da legenda. Nao entra na suite (*_shot.sh): janela GL.
#   bash tests/faixas_estilo_shot.sh /tmp/nv-faixas-estilo
set -eu
cd "$(dirname "$0")/.."
sources=()
for source in src/*.c src/dts/*.c; do [ "$source" != src/main.c ] && sources+=("$source"); done
cc "${sources[@]}" tests/faixas_estilo_shot.c -Isrc -o ${TMPDIR:-/tmp}/nuvio-faixas-estilo-shot \
  -O1 -g -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wno-deprecated-declarations -Wno-macro-redefined
D=$(mktemp -d); trap 'rm -rf "$D"' EXIT
NUVIO_DADOS="$D" NUVIO_TESTE_DIR="$D" ${TMPDIR:-/tmp}/nuvio-faixas-estilo-shot "$@"
