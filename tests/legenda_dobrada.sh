#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
read -r -a sdl_flags <<< "$(pkg-config --cflags sdl2)"
if [ "$(uname)" = Darwin ]; then gc=-Wl,-dead_strip; else gc=-Wl,--gc-sections; fi
cc tests/legenda_dobrada.c -Isrc "${sdl_flags[@]}" \
  -std=gnu11 -O1 -g -ffunction-sections -fdata-sections "$gc" \
  -Wall -Wextra -Wno-unused-function -o "$work/test"
"$work/test"
sources=()
for source in src/*.c src/dts/*.c; do
  case "$source" in src/main.c|src/player.c) continue;; esac
  sources+=("$source")
done
cc "${sources[@]}" tests/legenda_dobrada_render.c -Isrc -o "$work/render" \
  -O1 -g -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wno-deprecated-declarations -Wno-macro-redefined
"$work/render"
