#!/bin/bash
# Limite de pedidos de episodios do card terminado do Continuar, por titulo.
#   bash tests/cw_pedidos.sh
set -euo pipefail
cd "$(dirname "$0")/.."
sources=()
for source in src/*.c src/dts/*.c; do
  case "$source" in src/main.c|src/continuar.c) continue;; esac
  sources+=("$source")
done
binary="$(mktemp /tmp/nuvio-cw-pedidos.XXXXXX)"
trap 'rm -f "$binary"' EXIT
cc "${sources[@]}" tests/cw_pedidos.c -Isrc -o "$binary" \
  -O1 -g -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wno-deprecated-declarations -Wno-macro-redefined
"$binary" | grep -v '^\[cw\]'
