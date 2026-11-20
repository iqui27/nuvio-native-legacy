#!/bin/bash
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
# Capturas da ilha do relogio (ilha.h) sobre arte, sem rede. Nao entra na suite.
set -eu
cd "$(dirname "$0")/.."
NUVIO_DADOS=$(mktemp -d ${TMPDIR:-/tmp}/nuvio-ilha-dados.XXXXXX)
export NUVIO_DADOS
trap 'rm -rf "$NUVIO_DADOS"' EXIT
sources=()
for source in src/*.c src/dts/*.c; do
  if [ "$source" != src/main.c ] && [ "$source" != src/avisos.c ]; then
    sources+=("$source")
  fi
done
cc "${sources[@]}" tests/ilha_shot.c -Isrc -o ${TMPDIR:-/tmp}/nuvio-ilha-shot \
  -O1 -g -DNV_LEVE -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wno-deprecated-declarations -Wno-macro-redefined
${TMPDIR:-/tmp}/nuvio-ilha-shot "${1:-${TMPDIR:-/tmp}/nuvio-ilha}"
