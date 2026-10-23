#!/bin/bash
# Recomendacao nova nao come tecla da home. Ver tests/recomenda_tecla.c.
#
#   bash tests/recomenda_tecla.sh
set -eu
cd "$(dirname "$0")/.."
export TMPDIR="${TMPDIR:-/tmp}"
NUVIO_DADOS=$(mktemp -d "$TMPDIR/nuvio-rec-tecla.XXXXXX")
export NUVIO_DADOS
BIN="$NUVIO_DADOS/bin"
trap 'rm -rf "$NUVIO_DADOS"' EXIT
sources=()
for source in src/*.c src/dts/*.c; do
  case "$source" in src/main.c|src/recomenda.c) continue;; esac
  sources+=("$source")
done
cc "${sources[@]}" tests/recomenda_tecla.c -Isrc -o "$BIN" -O1 -g \
  -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wall -Wextra -Wno-deprecated-declarations -Wno-macro-redefined
"$BIN"
