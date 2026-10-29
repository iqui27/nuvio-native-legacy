#!/bin/bash
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
# Mock das tres direcoes do cabecalho das colecoes (job 205-colhdr): 12 PNGs
# 1920x1080 {A,B,C}-{zero,parcial,completo,foco}.png. Nao entra na suite:
# precisa de janela GL e de olho humano.
#
#   bash tests/colhdr_shot.sh [pasta]   (padrao /Volumes/ExternalSSD/tmp/rev/colhdr)
set -eu
cd "$(dirname "$0")/.."
saida="${1:-/Volumes/ExternalSSD/tmp/rev/colhdr}"
mkdir -p "$saida"
NUVIO_DADOS=$(mktemp -d "$TMPDIR/nuvio-colhdr-dados.XXXXXX")
export NUVIO_DADOS
trap 'rm -rf "$NUVIO_DADOS"' EXIT
sources=()
for source in src/*.c src/dts/*.c; do
  if [ "$source" != src/main.c ]; then sources+=("$source"); fi
done
cc "${sources[@]}" tests/colhdr_shot.c -Isrc -o "$TMPDIR/nuvio-colhdr-shot" \
  -O1 -g -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wall -Wno-deprecated-declarations -Wno-macro-redefined
"$TMPDIR/nuvio-colhdr-shot" "$saida"
