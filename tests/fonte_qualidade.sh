#!/bin/bash
# R9: a escolha automatica respeita qualidade/origem/StreamFit. Ver tests/fonte_qualidade.c.
set -eu
cd "$(dirname "$0")/.."
dir=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-fq.XXXXXX")
trap 'rm -rf "$dir"' EXIT
sources=()
for source in src/*.c src/dts/*.c; do [ "$source" != src/main.c ] && sources+=("$source"); done
cc "${sources[@]}" tests/fonte_qualidade.c -Isrc -o "$dir/teste" \
  -O1 -g -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wno-deprecated-declarations -Wno-macro-redefined
"$dir/teste"
