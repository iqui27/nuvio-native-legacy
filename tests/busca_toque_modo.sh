#!/bin/bash
# Search row drag-scroll is Mobile only (TV keeps arrows). Pure state test, no window.
#   bash tests/busca_toque_modo.sh
set -eu
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-busca-toque.XXXXXX")
trap 'rm -rf "$work"' EXIT
sources=()
flags=()
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
for source in src/*.c src/dts/*.c; do
  case "$source" in src/main.c|src/busca.c) continue;; esac
  sources+=("$source")
done
cc "${sources[@]}" tests/busca_toque_modo.c -DNV_TOUCH_UI -Isrc -o "$work/t" \
  ${flags[@]+"${flags[@]}"} -O1 -g -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wno-deprecated-declarations -Wno-macro-redefined
"$work/t"
