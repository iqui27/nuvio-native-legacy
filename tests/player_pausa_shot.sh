#!/bin/bash
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
# Capturas do painel de pausa. Nao entra na suite (*_shot.sh): janela GL.
#   bash tests/player_pausa_shot.sh /tmp/nv-player-live-shots/pausa
set -eu
cd "$(dirname "$0")/.."
sources=()
for source in src/*.c src/dts/*.c; do [ "$source" != src/main.c ] && sources+=("$source"); done
cc "${sources[@]}" tests/player_pausa_shot.c -Isrc -o ${TMPDIR:-/tmp}/nuvio-player-pausa-shot \
  -O1 -g -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wno-deprecated-declarations -Wno-macro-redefined
D=$(mktemp -d); trap 'rm -rf "$D"' EXIT
NUVIO_DADOS="$D" NUVIO_TESTE_DIR="$D" ${TMPDIR:-/tmp}/nuvio-player-pausa-shot "$@"
