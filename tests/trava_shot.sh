#!/bin/bash
# Capturas da tela de Trava de serie. Fora da suite.
#   NUVIO_SHOT_FONTE=3 bash tests/trava_shot.sh [prefixo]
set -eu
cd "$(dirname "$0")/.."
fontes=()
for f in src/*.c src/dts/*.c; do [ "$f" != src/main.c ] && fontes+=("$f"); done
out="${TMPDIR:-/tmp}/nuvio-trava-shot"
cc "${fontes[@]}" tests/trava_shot.c -Isrc -o "$out" \
  -O1 -g -DNV_LEVE -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wno-deprecated-declarations -Wno-macro-redefined
D=$(mktemp -d); trap 'rm -rf "$D"' EXIT
NUVIO_DADOS="$D" NUVIO_TESTE_DIR="$D" "$out" "${1:-${TMPDIR:-/tmp}/nuvio-trava}"
