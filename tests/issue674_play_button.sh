#!/bin/bash
# #674: real detail selection and focus, offline.
set -euo pipefail
cd "$(dirname "$0")/.."
dir=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-674.XXXXXX")
trap 'rm -rf "$dir"' EXIT
cc -Drede_pedir=nv_base_rede_pedir -c src/rede.c -o "$dir/rede.o" \
  -Isrc -O1 -I/opt/homebrew/include -I/opt/homebrew/include/SDL2
sources=()
for source in src/*.c src/dts/*.c; do
  case "$source" in src/main.c|src/detail.c|src/rede.c) continue;; esac
  sources+=("$source")
done
cc "${sources[@]}" tests/issue674_play_button.c "$dir/rede.o" -Isrc -o "$dir/test" \
  -O1 -g -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wno-deprecated-declarations -Wno-macro-redefined
"$dir/test"
