#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-follow.XXXXXX")
trap 'rm -rf "$work"' EXIT
cc -std=gnu11 -Isrc -O1 -g -Wall -Wextra -Werror \
  src/esportes.c src/esportes_parse.c src/esportes_seguir.c src/lembrete.c src/dados.c src/js.c src/buscanorm.c \
  tests/esportes_seguir.c -pthread -lm -o "$work/test"
"$work/test" "$work/data" write
"$work/test" "$work/data" read
for suite in esportes esportes_estado esportes_ordem esportes_ui lembrete guiav5 guiav5_epg_lifetime; do bash "tests/$suite.sh"; done
if [ "${FOLLOW_PNG:-1}" = 1 ]; then
  export NUVIO_DADOS="$work/shots"
  mkdir -p "$NUVIO_DADOS" /Volumes/ExternalSSD/tmp/rev/espseguir
  sources=()
  for source in src/*.c src/dts/*.c; do
    case "$source" in src/main.c|src/guia.c|src/guiav5ui.c|src/esportes.c) continue;; esac
    sources+=("$source")
  done
  cc "${sources[@]}" tests/esportes_seguir_shot.c -Isrc -o "$work/shot" -O1 -g \
    -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 -L/opt/homebrew/lib \
    -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL -Wno-deprecated-declarations
  "$work/shot" /Volumes/ExternalSSD/tmp/rev/espseguir
fi
