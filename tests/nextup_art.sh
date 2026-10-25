#!/bin/bash
set -eu
cd "$(dirname "$0")/.."
tmp="$(mktemp -d "${TMPDIR:-/tmp}/nuvio-nextup-art-XXXXXX")"
trap 'rm -rf "$tmp"' EXIT
cc src/catalogo.c src/progresso.c src/posterprov.c src/redeurl.c src/colecoes.c src/js.c src/catordem.c src/artehero.c src/cwordem.c src/proximo.c tests/nextup_art.c \
  -Isrc -o "$tmp/test" -O1 -g -ffunction-sections -fdata-sections \
  -Wl,-dead_strip -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -Wno-deprecated-declarations -Wno-macro-redefined
"$tmp/test"
