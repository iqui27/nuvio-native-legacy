#!/bin/bash
# Real settings persistence plus production Kotlin audio flags; no devices/network.
set -eu
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-guiasom.XXXXXX")
trap 'rm -rf "$work"' EXIT
sources=()
for source in src/*.c src/dts/*.c; do
  case "$source" in src/main.c|src/ajustes.c) continue;; esac
  sources+=("$source")
done
cc "${sources[@]}" tests/guiasom.c -Isrc -o "$work/check" \
  -O1 -g -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wno-deprecated-declarations -Wno-macro-redefined
"$work/check"
python3 tests/guiasom_android.py
