#!/bin/bash
# O modelo do social (src/socialvis.c), sem GL. Escreve so em NUVIO_DADOS.
set -euo pipefail
cd "$(dirname "$0")/.."
tmp="$(mktemp -d "${TMPDIR:-/tmp}/nuvio-socialvis-XXXXXX")"
trap 'rm -rf "$tmp"' EXIT
sources=()
for source in src/*.c src/dts/*.c; do
  if [ "$source" != src/main.c ]; then sources+=("$source"); fi
done
cc "${sources[@]}" tests/socialvis.c -Isrc -o "$tmp/teste" -O1 -g \
  -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -w
mkdir -p "$tmp/dados"
NUVIO_DADOS="$tmp/dados" "$tmp/teste" 2>&1 | grep -E 'socialvis|status|dia|Assert|assert'
