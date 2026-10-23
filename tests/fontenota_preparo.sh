#!/bin/bash
# Preparation regressions; real callbacks, no network or playback.
set -eu
cd "$(dirname "$0")/.."
dir=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-fn-preparo.XXXXXX")
trap 'rm -rf "$dir"' EXIT
export NUVIO_DADOS="$dir"
sources=()
for source in src/*.c src/dts/*.c; do
  case "$source" in src/main.c|src/streams.c) continue;; esac
  sources+=("$source")
done
cc "${sources[@]}" tests/fontenota_preparo.c -Isrc -o "$dir/teste" \
  -O1 -g -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wno-deprecated-declarations -Wno-macro-redefined
"$dir/teste"
