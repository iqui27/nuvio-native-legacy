#!/bin/bash
set -eu
cd "$(dirname "$0")/.."
work=$(mktemp -d /tmp/nuvio-j3-glass.XXXXXX)
trap 'rm -rf "$work"' EXIT
sources=()
for source in src/*.c src/dts/*.c; do
  [ "$source" != src/main.c ] && sources+=("$source")
done
cc "${sources[@]}" tests/j3_glass.c -DAJUSTES_TESTE -Isrc -o "$work/test" \
  -O1 -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wno-deprecated-declarations -Wno-macro-redefined
"$work/test"
