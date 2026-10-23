#!/bin/bash
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
# Requires a native GL window; compares framebuffer pixels without TV or network.
set -eu
cd "$(dirname "$0")/.."
tmp=$(mktemp -d ${TMPDIR:-/tmp}/nuvio-gfx-cor-painel.XXXXXX)
trap 'rm -rf "$tmp"' EXIT
sources=()
for source in src/*.c src/dts/*.c; do
  if [ "$source" != src/main.c ]; then sources+=("$source"); fi
done
cc "${sources[@]}" tests/gfx_cor_painel.c -Isrc -o "$tmp/teste" \
  -O1 -g -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wno-deprecated-declarations -Wno-macro-redefined
"$tmp/teste"
