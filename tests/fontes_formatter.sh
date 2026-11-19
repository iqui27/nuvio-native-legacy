#!/bin/bash
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
# #479: hidden-window RGB/alpha pixel assertions and zero-rasterization regression; no network.
set -euo pipefail
cd "$(dirname "$0")/.."
FF_DIR=$(mktemp -d ${TMPDIR:-/tmp}/nuvio-fontes-formatter.XXXXXX)
trap 'rm -rf "$FF_DIR"' EXIT
sources=()
for source in src/*.c src/dts/*.c; do
  [[ "$source" != src/main.c && "$source" != src/streams.c ]] && sources+=("$source")
done
cc "${sources[@]}" tests/fontes_formatter.c -Isrc -o "$FF_DIR/test" \
  -O1 -g -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wno-deprecated-declarations -Wno-macro-redefined
NUVIO_DADOS="$FF_DIR" "$FF_DIR/test" "$@"
