#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-guiafiltro-shot.XXXXXX")
trap 'rm -rf "$work"' EXIT
export NUVIO_DADOS="$work/data"
output="${1:-/Volumes/ExternalSSD/tmp/rev/livefiltro}"
mkdir -p "$output"
sources=()
for source in src/*.c src/dts/*.c; do
  case "$source" in src/main.c|src/guia.c) continue;; esac
  sources+=("$source")
done
cc "${sources[@]}" tests/guiafiltro_shot.c -Isrc -o "$work/shot" -O1 -g \
  -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 -L/opt/homebrew/lib \
  -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wno-deprecated-declarations -Wno-macro-redefined
"$work/shot" "$output"
