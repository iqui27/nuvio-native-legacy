#!/bin/bash
# Offline production GL regression; no TV input or network.
set -eu
cd "$(dirname "$0")/.."
tmp=$(mktemp -d /tmp/nuvio-upscaleiguais.XXXXXX)
trap 'rm -rf "$tmp"' EXIT
out=/Volumes/ExternalSSD/tmp/rev/upscaleiguais
mkdir -p "$out"
sources=()
for source in src/*.c src/dts/*.c; do
  case "$source" in src/main.c|src/ajustes.c) continue;; esac
  sources+=("$source")
done
cc "${sources[@]}" tests/upscaleiguais.c -Isrc -o "$tmp/test" \
  -O1 -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wno-deprecated-declarations -Wno-macro-redefined
NUVIO_DADOS="$tmp/data" "$tmp/test" "$out"
