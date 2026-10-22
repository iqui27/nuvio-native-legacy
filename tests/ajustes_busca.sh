#!/bin/bash
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
# Search finds every option on screen, also by old names.
#   bash tests/ajustes_busca.sh
set -eu
cd "$(dirname "$0")/.."
sources=()
for source in src/*.c src/dts/*.c; do
  case "$source" in src/main.c|src/ajustes.c) continue;; esac
  sources+=("$source")
done
binary="$(mktemp ${TMPDIR:-/tmp}/nuvio-aj-busca.XXXXXX)"
trap 'rm -f "$binary"' EXIT
cc "${sources[@]}" tests/ajustes_busca.c -Isrc -o "$binary" \
  -O1 -g -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wno-deprecated-declarations -Wno-macro-redefined
"$binary"
