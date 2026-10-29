#!/bin/bash
# Local GL screenshots of the focused/idle #205 warning with all filters on.
# Usage: bash tests/mp4oculto_shot.sh /absolute/output/prefix
set -euo pipefail
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-mp4oculto-shot.XXXXXX")
trap 'rm -rf "$work"' EXIT
sources=()
for source in src/*.c src/dts/*.c; do
  [[ "$source" != src/main.c && "$source" != src/streams.c ]] && sources+=("$source")
done
cc "${sources[@]}" tests/mp4oculto_shot.c -Isrc -o "$work/shot" \
  -O1 -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wno-deprecated-declarations -Wno-macro-redefined
NUVIO_DADOS="$work" "$work/shot" "$@"
