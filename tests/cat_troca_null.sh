#!/bin/bash
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
set -eu
cd "$(dirname "$0")/.."
sources=()
for source in src/*.c src/dts/*.c; do
  if [ "$source" != src/main.c ] && [ "$source" != src/detail.c ]; then sources+=("$source"); fi
done
# --wrap faz cat_item devolver NULL sob demanda: o instante em que a troca do
# catalogo publica n = 0, sem precisar de um segundo fio nem de sorte.
cc "${sources[@]}" tests/cat_troca_null.c -Isrc -o "${TMPDIR:-/tmp}/nuvio-cat-troca-null-tests" \
  -O1 -g -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wl,--wrap=cat_item \
  -Wno-deprecated-declarations -Wno-macro-redefined
"${TMPDIR:-/tmp}/nuvio-cat-troca-null-tests"
