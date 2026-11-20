#!/bin/bash
# Captura do selo de assistido nos modos Icone e Escrito. Nao entra na suite:
# precisa de janela GL e de olho humano.
#
#   bash tests/selo_visto_shot.sh /tmp/nuvio-selo   # grava -icone.bmp e -escrito.bmp
set -eu
cd "$(dirname "$0")/.."
sources=()
for source in src/*.c src/dts/*.c; do
  if [ "$source" != src/main.c ]; then sources+=("$source"); fi
done
bin="${TMPDIR:-/tmp}/nuvio-selo-visto-shot"
cc "${sources[@]}" tests/selo_visto_shot.c -Isrc -o "$bin" \
  -O1 -g -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wno-deprecated-declarations -Wno-macro-redefined
out="${1:-/tmp/nuvio-selo}"
for par in 0:icone 2:escrito; do
  D=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-selo-dados.XXXXXX")
  NUVIO_SHOT_SELO="${par%%:*}" NUVIO_DADOS="$D" "$bin" "$out-${par##*:}.bmp"
  rm -rf "$D"
done
rm -f "$bin"
