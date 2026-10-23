#!/bin/bash
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
# A abertura do Dolby Vision = a abertura normal (Centro / Canto, fundo, linhas),
# com so a marca DV e a linha de passo a mais; e a troca entre as duas sem salto.
# Precisa de janela GL (fora da suite, como todo *_shot.sh).
#   bash tests/dvabertura_shot.sh [/dir/das/capturas]
set -eu
cd "$(dirname "$0")/.."
sources=()
for source in src/*.c src/dts/*.c; do [ "$source" != src/main.c ] && sources+=("$source"); done
BIN="${TMPDIR:-/tmp}/nuvio-dvabertura-shot"
cc -DNV_SHOT_HOOKS "${sources[@]}" tests/dvabertura_shot.c -Isrc -o "$BIN" \
  -O1 -g -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wno-deprecated-declarations -Wno-macro-redefined
OUT="${1:-${TMPDIR:-/tmp}/nv-dvabertura}"
mkdir -p "$OUT"
D=$(mktemp -d); trap 'rm -rf "$D"' EXIT
NUVIO_DADOS="$D" NUVIO_TESTE_DIR="$D" "$BIN" "$OUT"
