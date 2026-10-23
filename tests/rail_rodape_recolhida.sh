#!/bin/bash
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
# #210: rodape da rail recolhida sem nome do perfil. Ver o .c.
#
#   bash tests/rail_rodape_recolhida.sh [prefixo-dos-bmp]
set -eu
cd "$(dirname "$0")/.."
sources=()
for source in src/*.c src/dts/*.c; do
  case "$source" in src/main.c) continue;; esac
  sources+=("$source")
done
cc "${sources[@]}" tests/rail_rodape_recolhida.c -Isrc -o ${TMPDIR:-/tmp}/nuvio-rail-rodape \
  -O1 -g -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wno-deprecated-declarations -Wno-macro-redefined
${TMPDIR:-/tmp}/nuvio-rail-rodape "$@"
