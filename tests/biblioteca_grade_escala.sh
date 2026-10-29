#!/bin/bash
# Library grid fills the row width at every UI scale. Runs without a window.
set -eu
cd "$(dirname "$0")/.."
tmp="$(mktemp -d "${TMPDIR:-/tmp}/nuvio-bibgrade-XXXXXX")"
trap 'rm -rf "$tmp"' EXIT
sources=()
for source in src/*.c src/dts/*.c; do
  if [ "$source" != src/main.c ] && [ "$source" != src/biblioteca.c ]; then sources+=("$source"); fi
done
cc "${sources[@]}" tests/biblioteca_grade_escala.c -Isrc -o "$tmp/t" \
  -DNV_TRAKT_CLIENT_ID='"chave-de-teste"' -O1 -g \
  -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wno-deprecated-declarations -Wno-macro-redefined
mkdir -p "$tmp/dados"
NUVIO_DADOS="$tmp/dados" "$tmp/t"
