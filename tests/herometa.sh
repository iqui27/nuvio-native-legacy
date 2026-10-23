#!/bin/bash
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
# A linha de meta do destaque com os ajustes do #205 (sinopse, notas, ano e
# duracao): cada um esconde so o seu campo e o que sobra junta com o mesmo
# separador. Ver o .c.
#
#   bash tests/herometa.sh
set -eu
cd "$(dirname "$0")/.."
sources=()
for source in src/*.c src/dts/*.c; do
  case "$source" in src/main.c|src/ajustes.c) continue;; esac
  sources+=("$source")
done
cc "${sources[@]}" tests/herometa.c -Isrc -o ${TMPDIR:-/tmp}/nuvio-herometa \
  -O1 -g -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wno-deprecated-declarations -Wno-macro-redefined
${TMPDIR:-/tmp}/nuvio-herometa
