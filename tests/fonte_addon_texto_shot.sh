#!/bin/bash
# Sources sheet in add-on text mode (Texto das fontes = Do addon), fed by real
# Stremio JSON. Not in the suite (GL window).
#   NUVIO_SHOT_FONTE=3 bash tests/fonte_addon_texto_shot.sh <dir>
set -eu
cd "$(dirname "$0")/.."
sources=()
for source in src/*.c src/dts/*.c; do
  [ "$source" != src/main.c ] && [ "$source" != src/streams.c ] && sources+=("$source")
done
bin="${TMPDIR:-/tmp}/nuvio-fonte-addon-shot"
cc -DNV_SHOT_HOOKS ${NV_ADDON_TEST_FLAGS:-}  "${sources[@]}" tests/fonte_addon_texto_shot.c -Isrc -o "$bin" \
  -O1 -g -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wno-deprecated-declarations -Wno-macro-redefined
D=$(mktemp -d); trap 'rm -rf "$D" "$bin"' EXIT
out=${1:-${TMPDIR:-/tmp}/rev/fonteaddon/after}
mkdir -p "$out"
NUVIO_DADOS="$D" NUVIO_TESTE_DIR="$D" "$bin" "$out/addon"
