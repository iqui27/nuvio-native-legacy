#!/bin/bash
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
# Captura dos selos embutidos (padrao e colorido) na folha de fontes. Nao entra
# na suite: precisa de janela GL e de olho humano.
#
#   bash tests/selos_shot.sh /tmp/nuvio-selos    # grava -padrao-*.bmp e -colorido-*.bmp
set -eu
cd "$(dirname "$0")/.."
sources=()
for source in src/*.c src/dts/*.c; do
  if [ "$source" != src/main.c ]; then sources+=("$source"); fi
done
bin="${TMPDIR:-/tmp}/nuvio-selos-shot"
cc "${sources[@]}" tests/selos_shot.c -Isrc -o "$bin" \
  -O1 -g -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wno-deprecated-declarations -Wno-macro-redefined
out="${1:-${TMPDIR:-/tmp}/nuvio-selos}"
for modo in padrao colorido; do
  D=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-selos-dados.XXXXXX")
  if [ "$modo" = colorido ]; then export NUVIO_SHOT_SELOS=1; else unset NUVIO_SHOT_SELOS; fi
  NUVIO_DADOS="$D" "$bin" "$out-$modo"
  rm -rf "$D"
done
rm -f "$bin"
