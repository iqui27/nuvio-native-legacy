#!/bin/bash
# Exercise Home, Saved/list and Detail row navigation without a window or network.
set -euo pipefail
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-row-navigation.XXXXXX")
trap 'rm -rf "$work"' EXIT
sources=()
for source in src/*.c src/dts/*.c; do
  case "$source" in src/main.c|src/home.c|src/biblioteca.c|src/detail.c) continue;; esac
  sources+=("$source")
done
cc "${sources[@]}" tests/row_navigation.c tests/row_navigation_library.c tests/row_navigation_detail.c -Isrc -o "$work/rows" \
  -O1 -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wno-deprecated-declarations -Wno-macro-redefined
NUVIO_DADOS="$work" "$work/rows"
