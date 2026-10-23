#!/bin/bash
# MARGEM + PRATO DE FOCO DA FILEIRA DE EPISODIOS (#205): ver tests/detail_epslista.c
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
set -eu
cd "$(dirname "$0")/.."
flags=()
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
sources=()
for source in src/*.c src/dts/*.c; do
  if [ "$source" != src/main.c ] && [ "$source" != src/detail.c ]; then sources+=("$source"); fi
done
cc ${flags[@]+"${flags[@]}"} "${sources[@]}" tests/detail_epslista.c -Isrc -o ${TMPDIR:-/tmp}/nuvio-detail-epslista-tests \
  -O1 -g -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wno-deprecated-declarations -Wno-macro-redefined
${TMPDIR:-/tmp}/nuvio-detail-epslista-tests
