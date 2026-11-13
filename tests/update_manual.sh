#!/bin/bash
# Manual update check with update notifications OFF; offline fixture release.
#   bash tests/update_manual.sh
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
set -eu
cd "$(dirname "$0")/.."
sources=()
for source in src/*.c src/dts/*.c; do
  case "$source" in src/main.c|src/atualizacao.c) continue;; esac
  sources+=("$source")
done
binary="$(mktemp "$TMPDIR/nuvio-update-manual.XXXXXX")"
trap 'rm -f "$binary"' EXIT
cc "${sources[@]}" tests/update_manual.c -Isrc -Itests -o "$binary" \
  -O1 -g -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wno-deprecated-declarations -Wno-macro-redefined
"$binary"
