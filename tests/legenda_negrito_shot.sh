#!/bin/bash
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
# Bold subtitle line through the real text path at 1080p (GL window, not in
# the suite: *_shot.sh). See tests/legenda_negrito_shot.c.
#   bash tests/legenda_negrito_shot.sh <dir>
set -eu
cd "$(dirname "$0")/.."
sources=()
for source in src/*.c src/dts/*.c; do [ "$source" != src/main.c ] && sources+=("$source"); done
bin="${TMPDIR:-/tmp}/nuvio-legenda-negrito-shot"
cc "${sources[@]}" tests/legenda_negrito_shot.c -Isrc -o "$bin" \
  -O1 -g -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wno-deprecated-declarations -Wno-macro-redefined
D=$(mktemp -d); trap 'rm -rf "$D" "$bin"' EXIT
mkdir -p "${1:-${TMPDIR:-/tmp}/nv-legenda-negrito}"
NUVIO_DADOS="$D" NUVIO_TESTE_DIR="$D" "$bin" "${1:-${TMPDIR:-/tmp}/nv-legenda-negrito}"
