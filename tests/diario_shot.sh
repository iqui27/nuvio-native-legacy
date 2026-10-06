#!/bin/bash
# Capturas do Diario (Perfil sem Trakt, Pegou no sono, cofre). Fora da suite.
#   NUVIO_SHOT_FONTE=3 bash tests/diario_shot.sh [prefixo]
set -eu
cd "$(dirname "$0")/.."
fontes=()
for f in src/*.c src/dts/*.c; do [ "$f" != src/main.c ] && fontes+=("$f"); done
out="${TMPDIR:-/tmp}/nuvio-diario-shot"
cc "${fontes[@]}" tests/diario_shot.c -Isrc -o "$out" \
  -O1 -g -DNV_LEVE -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wno-deprecated-declarations -Wno-macro-redefined
D=$(mktemp -d); trap 'rm -rf "$D"' EXIT
NUVIO_DADOS="$D" NUVIO_TESTE_DIR="$D" "$out" "${1:-${TMPDIR:-/tmp}/nuvio-diario}"
