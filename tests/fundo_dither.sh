#!/bin/bash
# Dither da arte borrada de tela cheia (fundo.c): o quadro pequeno assado vai a
# tela pelo GFX_SNAP com o nv_dither ligado, e os degraus de 8 bits do degrade
# lento saem quebrados nos pixels. Ver tests/fundo_dither.c.
set -euo pipefail
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-fundo-dither.XXXXXX")
trap 'rm -rf "$work"' EXIT
sources=()
for source in src/*.c src/dts/*.c; do
  [ "$source" != src/main.c ] && sources+=("$source")
done
cc "${sources[@]}" tests/fundo_dither.c -Isrc -o "$work/test" \
  -O1 -g -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wno-deprecated-declarations -Wno-macro-redefined
mkdir -p "$work/dados"
NUVIO_DUMP_FUNDO_DIR="$work" NUVIO_DADOS="$work/dados" NUVIO_TESTE_DIR="$work/dados" \
  NV_DITHER_BMP="${NV_DITHER_BMP:-}" "$work/test" 2>&1 |
  grep -vE "^\[(tex|arte|cat|desc|home|txt|corviva|ajustes|dados|perfil|badges)[a-z-]*\]"
exit "${PIPESTATUS[0]}"
