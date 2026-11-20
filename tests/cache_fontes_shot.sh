#!/bin/bash
# #499: real source sheet, both text modes and player switcher, local fixtures only.
set -euo pipefail
cd "$(dirname "$0")/.."
export TMPDIR=/Volumes/ExternalSSD/tmp/jobtmp
mkdir -p "$TMPDIR"
D=$(mktemp -d "$TMPDIR/nuvio-cache-fontes.XXXXXX")
trap 'rm -rf "$D"' EXIT
sources=()
for source in src/*.c src/dts/*.c; do
  [[ "$source" != src/main.c && "$source" != src/streams.c ]] && sources+=("$source")
done
cc -DNV_SHOT_HOOKS "${sources[@]}" tests/cache_fontes_shot.c -Isrc -o "$D/test" \
  -O1 -g -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wno-deprecated-declarations -Wno-macro-redefined
out="${1:-/Volumes/ExternalSSD/tmp/rev/499cache}"
mkdir -p "$out"
for mode in addon:1 nuvio:0; do
  for context in fontes:0 player:1; do
    mkdir -p "$D/data"
    NUVIO_DADOS="$D/data" NUVIO_TESTE_DIR="$D/data" \
      "$D/test" "$out/${context%:*}-${mode%:*}" "${mode#*:}" "${context#*:}"
  done
done
