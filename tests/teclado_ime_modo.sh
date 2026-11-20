#!/bin/bash
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
# Ajuste Teclado (Android). Ver o .c.
#
#   bash tests/teclado_ime_modo.sh
set -eu
cd "$(dirname "$0")/.."
sources=()
for source in src/*.c src/dts/*.c; do
  case "$source" in src/main.c) continue;; esac
  sources+=("$source")
done
cc -DNV_TESTE "${sources[@]}" tests/teclado_ime_modo.c -Isrc -o ${TMPDIR:-/tmp}/nuvio-teclado-ime-modo \
  -O1 -g -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wno-deprecated-declarations -Wno-macro-redefined
${TMPDIR:-/tmp}/nuvio-teclado-ime-modo
