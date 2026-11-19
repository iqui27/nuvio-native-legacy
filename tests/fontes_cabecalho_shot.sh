#!/bin/bash
# Capturas do cabecalho da folha de Fontes (#202). Nao entra na suite
# (*_shot.sh): janela GL. Ver tests/fontes_cabecalho_shot.c.
#   bash tests/fontes_cabecalho_shot.sh <dir>
# Gera <dir>/{pt,de}-{solta,player}-{5bt,6bt}-N-estado.png.
set -eu
cd "$(dirname "$0")/.."
sources=()
for source in src/*.c src/dts/*.c; do [ "$source" != src/main.c ] && sources+=("$source"); done
bin="${TMPDIR:-/tmp}/nuvio-fontes-cabecalho-shot"
cc -DNV_SHOT_HOOKS "${sources[@]}" tests/fontes_cabecalho_shot.c -Isrc -o "$bin" \
  -O1 -g -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wno-deprecated-declarations -Wno-macro-redefined
D=$(mktemp -d); trap 'rm -rf "$D" "$bin"' EXIT
mkdir -p "$1"
for idioma in pt:0 de:6; do
  for modo in solta:0 player:1; do
    for bt in 5bt:0 6bt:1; do
      rm -rf "$D"/*
      NUVIO_DADOS="$D" NUVIO_TESTE_DIR="$D" NUVIO_SHOT_IDIOMA="${idioma#*:}" \
        NUVIO_SHOT_PLAYER="${modo#*:}" NUVIO_SHOT_SEMHDR="${bt#*:}" \
        "$bin" "$1/${idioma%%:*}-${modo%%:*}-${bt%%:*}"
    done
  done
done
