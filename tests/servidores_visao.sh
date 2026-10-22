#!/bin/bash
# Focused server-view regression; no renderer, network, credentials or account.
set -euo pipefail
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-servidores-visao.XXXXXX")
trap 'rm -rf "$work"' EXIT
cc tests/servidores_visao.c -Isrc -o "$work/test" -O1 -g \
  -ffunction-sections -fdata-sections -Wl,-dead_strip \
  -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -Wno-deprecated-declarations -Wno-macro-redefined
"$work/test"
cc src/catalogo.c src/progresso.c src/focus.c src/ajustes.c src/posterprov.c \
  src/redeurl.c src/colecoes.c src/js.c src/catordem.c src/fileiras.c \
  src/artehero.c src/cwordem.c src/cwretido.c src/colfileiras.c \
  tests/servidores_hero.c tests/amigosfil_stub.c -Isrc -o "$work/hero" -O1 -g \
  -ffunction-sections -fdata-sections -Wl,-dead_strip \
  -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -Wno-deprecated-declarations -Wno-macro-redefined
"$work/hero"
