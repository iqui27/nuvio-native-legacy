#!/bin/bash
# Offline GL regression: tabs above localized titles, with full transition motion.
set -euo pipefail
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-social-header.XXXXXX")
trap 'rm -rf "$work"' EXIT
out=${1:-/Volumes/ExternalSSD/tmp/rev/socialtopo/after}
mkdir -p "$out" "$work/data"
sources=()
for source in src/*.c src/dts/*.c; do
  case "$source" in src/main.c|src/salvospainel.c) continue;; esac
  sources+=("$source")
done
cc "${sources[@]}" tests/social_header.c -Isrc -Itests -o "$work/test" \
  -DNV_REC_URL='"http://127.0.0.1:8799"' -O1 -g \
  -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 -L/opt/homebrew/lib \
  -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wno-deprecated-declarations -Wno-macro-redefined
NUVIO_DADOS="$work/data" "$work/test" "$out"
