#!/bin/bash
# Tailandes (#369) em todo caminho de texto (ver tests/tailandes_texto.c).
# So fontes embarcadas, como na Samsung. Requer GL (Mac).
set -euo pipefail
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-tailandes-texto.XXXXXX")
trap 'rm -rf "$work"' EXIT
sources=()
for source in src/*.c src/dts/*.c; do [ "$source" != src/main.c ] && sources+=("$source"); done
cc "${sources[@]}" tests/tailandes_texto.c -Isrc -o "$work/test" \
  -O1 -g -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wno-deprecated-declarations -Wno-macro-redefined
NUVIO_SEM_RESERVA_DE_SISTEMA=1 NUVIO_DADOS="$work" "$work/test" | grep -v "^\[t\]\|^fonte\|^reserva"
