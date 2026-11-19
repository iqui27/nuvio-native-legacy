#!/bin/bash
# Headless Multiview flow over the real guide + v5 UI (see the .c).
set -euo pipefail
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-g5-mv.XXXXXX")
trap 'rm -rf "$work"' EXIT
export NUVIO_DADOS="$work"
sources=()
for source in src/*.c src/dts/*.c; do
  case "$source" in src/main.c|src/guia.c|src/guiav5ui.c) continue;; esac
  sources+=("$source")
done
cc "${sources[@]}" tests/guiav5_multiview.c ${GUIAV5_MV_EXTRA:-} -Isrc -o "$work/test" -O0 -g \
  -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 -L/opt/homebrew/lib \
  -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL -Wno-deprecated-declarations -Wno-macro-redefined
"$work/test" | grep -v '^\[dados\]'
