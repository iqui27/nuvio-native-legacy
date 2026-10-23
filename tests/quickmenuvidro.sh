#!/bin/bash
# Offline pixel captures and focus-material regression for the real quick menu.
set -euo pipefail
cd "$(dirname "$0")/.."
out="${1:-docs/shots/quickmenuvidro/after}"
mkdir -p "$out"
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-quickmenuvidro.XXXXXX")
trap 'rm -rf "$work"' EXIT
sources=()
for source in src/*.c src/dts/*.c; do
  case "$source" in src/main.c|src/central.c) ;; *) sources+=("$source");; esac
done
cc "${sources[@]}" tests/quickmenuvidro.c -Isrc -o "$work/shot" -DCENTRAL_TESTE \
  -O1 -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wno-deprecated-declarations -Wno-macro-redefined
mkdir -p "$work/data"
NUVIO_DADOS="$work/data" "$work/shot" "$out"
