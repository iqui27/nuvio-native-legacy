#!/bin/bash
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
# #461: menu na barra do topo. Ver o .c.
#
#   bash tests/menu_topo.sh
set -eu
cd "$(dirname "$0")/.."
sources=()
for source in src/*.c src/dts/*.c; do
  case "$source" in src/main.c) continue;; esac
  sources+=("$source")
done
cc -DNV_TOUCH_UI -DNV_TESTE "${sources[@]}" tests/menu_topo.c -Isrc -o ${TMPDIR:-/tmp}/nuvio-menu-topo \
  -O1 -g -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wno-deprecated-declarations -Wno-macro-redefined
${TMPDIR:-/tmp}/nuvio-menu-topo
