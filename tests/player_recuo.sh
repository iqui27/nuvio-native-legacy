#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
dir=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-player-recuo.XXXXXX")
trap 'rm -rf "$dir"' EXIT
mkdir "$dir/dados"
sources=()
for source in src/*.c src/dts/*.c; do
  case "$source" in src/main.c|src/player.c) continue;; esac
  sources+=("$source")
done
cc "${sources[@]}" tests/player_recuo.c -Isrc -o "$dir/test" \
  -O1 -g -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wno-deprecated-declarations -Wno-macro-redefined
NUVIO_DADOS="$dir/dados" "$dir/test"
