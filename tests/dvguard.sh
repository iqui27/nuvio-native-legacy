#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
dir=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-dvguard.XXXXXX")
trap 'rm -rf "$dir"' EXIT
export NUVIO_DADOS="$dir/dados"
mkdir -p "$NUVIO_DADOS"
sources=()
for source in src/*.c src/dts/*.c; do
  case "$source" in src/main.c|src/video.c|src/ajustes.c) continue;; esac
  sources+=("$source")
done
cc -O1 -g -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -Wno-deprecated-declarations -Wno-macro-redefined \
  "${sources[@]}" tests/dvguard.c tests/dvguard_settings.c -o "$dir/test" \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL
"$dir/test"
"$dir/test" dts
