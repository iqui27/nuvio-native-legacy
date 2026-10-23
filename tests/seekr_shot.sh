#!/bin/bash
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
# Miniatura do Seekr no player, contra a API real. Fora da suite.
#   SEEKR_API_KEY=... bash tests/seekr_shot.sh /tmp/nv-seekr/shot
set -eu
cd "$(dirname "$0")/.."
sources=()
for source in src/*.c src/dts/*.c; do [ "$source" != src/main.c ] && sources+=("$source"); done
cc "${sources[@]}" tests/seekr_shot.c -Isrc -o ${TMPDIR:-/tmp}/nuvio-seekr-shot \
  -O1 -g -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wno-deprecated-declarations -Wno-macro-redefined
D=$(mktemp -d); trap 'rm -rf "$D"' EXIT
NUVIO_DADOS="$D" NUVIO_TESTE_DIR="$D" ${TMPDIR:-/tmp}/nuvio-seekr-shot "$@"
