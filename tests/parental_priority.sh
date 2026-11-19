#!/bin/bash
# Offline only; hidden local GL window, no device or remote-control input.
set -eu
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-parental-priority.XXXXXX")
trap 'rm -rf "$work"' EXIT
sources=()
for source in src/*.c src/dts/*.c; do
  case "$source" in src/main.c|src/player.c|src/plrilha.c) continue;; esac
  sources+=("$source")
done
cc -DNV_SHOT_HOOKS "${sources[@]}" tests/parental_priority.c -Isrc -o "$work/test" \
  -O1 -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wno-deprecated-declarations -Wno-macro-redefined
mkdir "$work/data"
NUVIO_DADOS="$work/data" NUVIO_TESTE_DIR="$work/data" "$work/test"
