#!/bin/bash
# A tela do Dolby Vision em MKV no player de verdade, com capturas 1920x1080.
# Precisa de janela GL (fora da suite, como todo *_shot.sh).
#   bash tests/dvtela_shot.sh /tmp/nv-dvtela/dv
set -eu
cd "$(dirname "$0")/.."
sources=()
for source in src/*.c src/dts/*.c; do [ "$source" != src/main.c ] && sources+=("$source"); done
BIN="${TMPDIR:-/tmp}/nuvio-dvtela-shot"
cc -DNV_SHOT_HOOKS "${sources[@]}" tests/dvtela_shot.c -Isrc -o "$BIN" \
  -O1 -g -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wno-deprecated-declarations -Wno-macro-redefined
mkdir -p "$(dirname "${1:-/tmp/nv-dvtela/dv}")"
D=$(mktemp -d); trap 'rm -rf "$D"' EXIT
NUVIO_DADOS="$D" NUVIO_TESTE_DIR="$D" "$BIN" "$@"
