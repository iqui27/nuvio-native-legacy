#!/bin/bash
# Capturas das estatisticas ao vivo do player em BMP (Android, LG, Samsung no
# tamanho completo, o compacto e o minimo). Nao entra na suite (*_shot.sh): janela GL e olho humano.
#   bash tests/estatvivo_shot.sh /tmp/nuvio-estatvivo
set -eu
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-estatvivo-shot.XXXXXX")
trap 'rm -rf "$work"' EXIT
sources=()
for source in src/*.c src/dts/*.c; do [ "$source" != src/main.c ] && sources+=("$source"); done
cc "${sources[@]}" tests/estatvivo_shot.c -Isrc -o "$work/shot" -DESTATVIVO_TESTE \
  -O1 -g -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wno-deprecated-declarations -Wno-macro-redefined
mkdir -p "$work/dados"
NUVIO_DADOS="$work/dados" "$work/shot" "$@"
