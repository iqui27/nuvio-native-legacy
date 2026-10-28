#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
out=${1:-/Volumes/ExternalSSD/tmp/rev/guiav5-png}
mkdir -p "$out"
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-g5-shot.XXXXXX")
trap 'rm -rf "$work"' EXIT
export NUVIO_DADOS="$work"
sources=()
for source in src/*.c src/dts/*.c; do
  case "$source" in src/main.c|src/guiav5ui.c) continue;; esac
  sources+=("$source")
done
cc "${sources[@]}" tests/guiav5_shot.c -Isrc -o "$work/shot" -O1 -g \
  -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 -L/opt/homebrew/lib \
  -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL -Wno-deprecated-declarations
"$work/shot" "$out"
node tests/guiav5_phone_shot.cjs "$out"
python3 tests/guiav5_compare.py "$out"
