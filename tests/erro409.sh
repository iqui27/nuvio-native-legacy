#!/bin/bash
# #409: compila streams/fonteauto/app/player reais no host.
set -eu
cd "$(dirname "$0")/.."
dir=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-erro409.XXXXXX")
trap 'rm -rf "$dir"' EXIT
export NUVIO_DADOS="$dir/dados"
mkdir -p "$NUVIO_DADOS"
sources=()
for source in src/*.c src/dts/*.c; do [ "$source" != src/main.c ] && [ "$source" != src/player.c ] && sources+=("$source"); done
cc -DNV_ANDROID -O1 -g -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -Wno-deprecated-declarations -Wno-macro-redefined -c src/player.c -o "$dir/player.o"
cc "$dir/player.o" "${sources[@]}" tests/erro409.c -Isrc -o "$dir/teste" \
  -O1 -g -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wno-deprecated-declarations -Wno-macro-redefined
"$dir/teste"
