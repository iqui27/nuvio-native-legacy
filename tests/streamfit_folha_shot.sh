#!/bin/bash
# Captura da folha de fontes com a evidencia do StreamFit (F03). Nao entra na
# suite: precisa de janela GL e de olho humano.
#   bash tests/streamfit_folha_shot.sh /Volumes/ExternalSSD/nv-f03-shots/folha
#   NUVIO_SHOT_IDIOMA=1 bash tests/streamfit_folha_shot.sh .../folha-en   (1 = English, 2 = Romanian)
set -eu
cd "$(dirname "$0")/.."
NUVIO_DADOS=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-streamfit-shot-dados.XXXXXX")
export NUVIO_DADOS
bin="${TMPDIR:-/tmp}/nuvio-streamfit-folha-shot"
trap 'rm -rf "$NUVIO_DADOS"' EXIT
sources=()
for source in src/*.c src/dts/*.c; do [ "$source" != src/main.c ] && sources+=("$source"); done
cc "${sources[@]}" tests/streamfit_folha_shot.c -Isrc -o "$bin" \
  -O1 -g -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wno-deprecated-declarations -Wno-macro-redefined
"$bin" "$@"
