#!/bin/bash
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
# Aviso legal: item, primeira adicao e persistencia entre processos.
#   bash tests/ajustes_aviso_legal.sh
set -eu
cd "$(dirname "$0")/.."
sources=()
for source in src/*.c src/dts/*.c; do
  case "$source" in src/main.c|src/ajustes.c) continue;; esac
  sources+=("$source")
done
binary="$(mktemp ${TMPDIR:-/tmp}/nuvio-ajustes-aviso-legal.XXXXXX)"
trap 'rm -f "$binary"' EXIT
cc -DAJUSTES_TESTE "${sources[@]}" tests/ajustes_aviso_legal.c -Isrc -o "$binary" \
  -O1 -g -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wno-deprecated-declarations -Wno-macro-redefined
dados="$(mktemp -d ${TMPDIR:-/tmp}/nuvio-aviso-legal.XXXXXX)"
trap 'rm -f "$binary"; rm -rf "$dados"' EXIT
"$binary" "$dados" primeira
"$binary" "$dados" repetida
