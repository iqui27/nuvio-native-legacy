#!/bin/bash
# #410B: ajuda dos Ajustes e i18n pelo caminho real; sem GL/janela/rede.
set -eu
cd "$(dirname "$0")/.."
sources=()
for source in src/*.c src/dts/*.c; do
  case "$source" in src/main.c|src/ajustes.c) continue;; esac
  sources+=("$source")
done
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-gpunivel-ajuda.XXXXXX")
trap 'rm -rf "$work"' EXIT
cc "${sources[@]}" tests/gpunivel_ajuda.c -Isrc -o "$work/teste" \
  -DNV_GPUN_TESTE -O1 -g -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wno-deprecated-declarations -Wno-macro-redefined
NUVIO_DADOS="$work" "$work/teste"
