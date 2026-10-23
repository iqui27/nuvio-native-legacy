#!/bin/bash
# Full source-sheet frame regression, local fixtures and hidden GL only.
set -euo pipefail
cd "$(dirname "$0")/.."
FRAME_DIR=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-fontes-quadro.XXXXXX")
trap 'rm -rf "$FRAME_DIR"' EXIT
sources=()
for source in src/*.c src/dts/*.c; do
  [[ "$source" != src/main.c && "$source" != src/streams.c ]] && sources+=("$source")
done
cc "${sources[@]}" tests/fontes_quadro.c -Isrc -o "$FRAME_DIR/test" \
  -O1 -g -finstrument-functions -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wno-deprecated-declarations -Wno-macro-redefined
NUVIO_DADOS="$FRAME_DIR" "$FRAME_DIR/test" "$@"
