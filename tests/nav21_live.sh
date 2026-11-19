#!/bin/bash
# Shared playback restrictions, without network access or rendered captures.
set -eu
cd "$(dirname "$0")/.."
binary="$(mktemp "${TMPDIR:-/tmp}/nav21-live.XXXXXX")"
trap 'rm -f "$binary"' EXIT
sources=()
for source in src/*.c src/dts/*.c; do
  case "$source" in src/main.c|src/app.c) continue;; esac
  sources+=("$source")
done
cc "${sources[@]}" tests/nav21_live.c -Isrc -o "$binary" \
  -O1 -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wno-deprecated-declarations -Wno-macro-redefined
"$binary"
