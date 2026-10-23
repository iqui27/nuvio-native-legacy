#!/bin/bash
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
# Fundo desfocado da abertura Cinema (gfx_desfocado): mede a copia 96x54 e grava
# a tela cheia. Ver tests/desfoque_shot.c. Precisa de janela GL.
#   bash tests/desfoque_shot.sh antes|depois   # saida: ${NUVIO_DESFOQUE_SAIDA:-${TMPDIR:-/tmp}/nuvio-desfoque}-<rotulo>.png
set -eu
cd "$(dirname "$0")/.."
sources=()
for source in src/*.c src/dts/*.c; do
  if [ "$source" != src/main.c ]; then sources+=("$source"); fi
done
BIN="${NUVIO_SHOT_BIN:-${TMPDIR:-/tmp}/nuvio-desfoque-shot}"
cc -DAJUSTES_TESTE "${sources[@]}" tests/desfoque_shot.c -Isrc -o "$BIN" \
  -O1 -g -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wno-deprecated-declarations -Wno-macro-redefined
"$BIN" "${1:-atual}"
