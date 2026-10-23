#!/bin/bash
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
# Mobile List overlap matrix; real badge geometry and clock draw, no network.
# Uses a hidden GL window (like tests/ajustes_shot.sh).
#   bash tests/ajustes_lista_cabecalho.sh
set -eu
cd "$(dirname "$0")/.."
sources=()
for source in src/*.c src/dts/*.c; do
  case "$source" in src/main.c|src/ajustes.c) continue;; esac
  sources+=("$source")
done
NUVIO_DADOS="$(mktemp -d ${TMPDIR:-/tmp}/nuvio-ajustes-lista-cabecalho.XXXXXX)"
export NUVIO_DADOS
binary="$NUVIO_DADOS/teste"
trap 'rm -rf "$NUVIO_DADOS"' EXIT
flags=(-O1 -g)
if [ "${SANITIZE:-0}" = 1 ]; then
  flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer)
fi
cc "${sources[@]}" tests/ajustes_lista_cabecalho.c -DNV_TOUCH_UI -Isrc -o "$binary" \
  "${flags[@]}" -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wno-deprecated-declarations -Wno-macro-redefined
"$binary"
