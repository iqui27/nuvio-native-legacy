#!/bin/bash
# C5/C6: production source-sheet cache, headless frames and title focus.
set -euo pipefail
cd "$(dirname "$0")/.."
CACHE_DIR=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-streams-cache.XXXXXX")
trap 'rm -rf "$CACHE_DIR"' EXIT
sources=()
for source in src/*.c src/dts/*.c; do
  [[ "$source" != src/main.c && "$source" != src/streams.c ]] && sources+=("$source")
done
cc -c tests/streams_folha_cache.c -Isrc -o "$CACHE_DIR/sheet.o" \
  -O1 -g -finstrument-functions -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -Wno-deprecated-declarations -Wno-macro-redefined
cc "${sources[@]}" "$CACHE_DIR/sheet.o" -Isrc -o "$CACHE_DIR/test" \
  -O1 -g -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wno-deprecated-declarations -Wno-macro-redefined
"$CACHE_DIR/test" "$@"
