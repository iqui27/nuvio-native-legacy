#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
sources=()
for source in src/*.c src/dts/*.c; do
  case "$source" in src/main.c|src/player.c|src/faixas.c) continue;; esac
  sources+=("$source")
done
cc "${sources[@]}" tests/tpk_primary_subtitle.c tests/tpk_primary_subtitle_state.c -Isrc -o "$work/test" \
  -O1 -g -ffunction-sections -fdata-sections -Wl,-dead_strip -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wno-deprecated-declarations -Wno-macro-redefined
NUVIO_DADOS="$work" NUVIO_TESTE_DIR="$work" "$work/test"
