#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-gifperfis.XXXXXX")
trap 'rm -rf "$work"' EXIT
sources=()
for source in src/*.c src/dts/*.c; do
  case "$source" in src/main.c|src/perfilsel.c|src/gif.c) continue;; esac
  sources+=("$source")
done
flags=(-Isrc -O1 -I/opt/homebrew/include -I/opt/homebrew/include/SDL2
       -Wno-deprecated-declarations -Wno-macro-redefined)
cc "${flags[@]}" -c tests/gifperfis_decoder.c -o "$work/gif.o"
cc "${flags[@]}" -DNV_PERFILSEL_TEST "${sources[@]}" tests/gifperfis.c "$work/gif.o" \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL -o "$work/test"
mkdir -p "$work/data"
NUVIO_DADOS="$work/data" NUVIO_TESTE_DIR="$work/data" "$work/test"
