#!/bin/bash
# Captura das linhas de auto-play (#202) em Ajustes (PNG, sem rede). Nao entra
# na suite: precisa de janela GL e de olho humano.
#   mkdir -p /tmp/nv-202 && NUVIO_SHOT_FONTE=3 bash tests/autoplay_ajustes_shot.sh /tmp/nv-202/aj
set -eu
cd "$(dirname "$0")/.."
export NUVIO_DADOS=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-autoplay-dados.XXXXXX")
trap 'rm -rf "$NUVIO_DADOS"' EXIT
sources=()
for source in src/*.c src/dts/*.c; do
  if [ "$source" != src/main.c ]; then sources+=("$source"); fi
done
cc -DAJUSTES_TESTE "${sources[@]}" tests/autoplay_ajustes_shot.c -Isrc -o "$NUVIO_DADOS/shot" \
  -O1 -g -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wno-deprecated-declarations -Wno-macro-redefined -w
"$NUVIO_DADOS/shot" "$@"
