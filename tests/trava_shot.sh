#!/bin/bash
# Capturas da trava de serie: tests/trava_shot.sh <prefixo>. Fonte 3 (Montserrat,
# a da TV) por padrao.
set -eu
cd "$(dirname "$0")/.."
T="${TMPDIR:-/tmp}"
D=$(mktemp -d "$T/nv-trava-shot.XXXXXX"); trap 'rm -rf "$D"' EXIT
fontes=()
for f in src/*.c src/dts/*.c; do [ "$f" != src/main.c ] && fontes+=("$f"); done
cc "${fontes[@]}" tests/trava_shot.c -Isrc -o "$D/shot" -O1 -g \
  -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 -L/opt/homebrew/lib \
  -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL -Wno-deprecated-declarations -Wno-macro-redefined
NUVIO_DADOS="$D" NUVIO_SHOT_FONTE="${NUVIO_SHOT_FONTE:-3}" "$D/shot" "${1:-$T/nuvio-trava}"
