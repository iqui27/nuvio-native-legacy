#!/bin/bash
# #507: Library row on Home (off by default, saved titles when on).
#
#   bash tests/bibhome.sh
set -euo pipefail
cd "$(dirname "$0")/.."
NUVIO_DADOS=$(mktemp -d ${TMPDIR:-/tmp}/nuvio-bibhome.XXXXXX)
export NUVIO_DADOS
trap 'rm -rf "$NUVIO_DADOS"' EXIT
sources=()
for source in src/*.c src/dts/*.c; do
  case "$source" in src/main.c|src/home.c) continue;; esac
  sources+=("$source")
done
out="${TMPDIR:-/tmp}/nuvio-bibhome-tests"
cc "${sources[@]}" tests/bibhome.c -Isrc -o "$out" \
  -O1 -g -DNV_SHOT_HOOKS -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wno-deprecated-declarations -Wno-macro-redefined
"$out"
