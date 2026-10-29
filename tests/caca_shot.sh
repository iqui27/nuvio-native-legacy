#!/bin/bash
# Capturas da tela Caca e do aviso de conquista na ilha, em PNG, sem rede.
# Nao entra na suite (*_shot).
#   NUVIO_SHOT_FONTE=3 bash tests/caca_shot.sh [prefixo]   (3 = Montserrat, a fonte da TV)
set -eu
cd "$(dirname "$0")/.."
NUVIO_DADOS=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-caca-shot.XXXXXX")
export NUVIO_DADOS
trap 'rm -rf "$NUVIO_DADOS"' EXIT
sources=()
for source in src/*.c src/dts/*.c; do
  if [ "$source" != src/main.c ]; then sources+=("$source"); fi
done
cc "${sources[@]}" tests/caca_shot.c -Isrc -o "$NUVIO_DADOS/shot" \
  -O1 -g -DNV_LEVE -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wno-deprecated-declarations -Wno-macro-redefined
"$NUVIO_DADOS/shot" "${1:-${TMPDIR:-/tmp}/nuvio-caca}"
