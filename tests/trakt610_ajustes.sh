#!/bin/bash
# Offline Settings behavior plus PNG. Foreground only.
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p /Volumes/ExternalSSD/tmp/rev/trakt610
bin=/Volumes/ExternalSSD/tmp/rev/trakt610/settings-test
sources=()
for source in src/*.c src/dts/*.c; do
  case "$source" in src/main.c|src/ajustes.c) continue;; esac
  sources+=("$source")
done
defs=()
if [ "${TRAKT610_BEFORE:-0}" = 1 ]; then defs+=(-DTRAKT610_CAPTURE_ONLY); fi
cc ${defs[@]+"${defs[@]}"} -DAJUSTES_TESTE "${sources[@]}" tests/trakt610_ajustes.c -Isrc -o "$bin" \
  -O1 -g -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wno-deprecated-declarations -Wno-macro-redefined
"$bin" "${1:-/Volumes/ExternalSSD/tmp/rev/trakt610/after.png}"
