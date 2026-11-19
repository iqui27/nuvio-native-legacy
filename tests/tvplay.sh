#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-tvplay.XXXXXX")
trap 'rm -rf "$work"' EXIT
cc -DNV_DESC_MIN_MS=0ull src/cwordem.c tests/tvplay.c src/homeestado.c src/catalogo.c \
  src/progresso.c src/js.c src/metaprov.c src/colecoes.c src/redeurl.c src/catordem.c src/cotacat.c \
  -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -o "$work/discovery" -O1 -g -pthread \
  -Wno-deprecated-declarations -Wno-macro-redefined
"$work/discovery"
cc -std=gnu11 -O1 -g -pthread -Isrc -ffunction-sections -fdata-sections \
  -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 -Wno-macro-redefined -Wl,-dead_strip \
  tests/tvplay_stream.c src/idcanal.c src/js.c src/jsw.c src/watchstate.c src/addonstats.c src/fontefalha.c \
  -o "$work/stream"
"$work/stream"
