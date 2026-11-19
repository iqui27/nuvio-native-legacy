#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-plain-tv.XXXXXX")
trap 'rm -rf "$work"' EXIT
cc -DNV_DESC_MIN_MS=0ull src/cwordem.c tests/plain_tv.c src/homeestado.c src/catalogo.c \
  src/progresso.c src/js.c src/metaprov.c src/colecoes.c src/redeurl.c src/catordem.c src/cotacat.c \
  -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -o "$work/discovery" -O1 -g -pthread \
  -Wno-deprecated-declarations -Wno-macro-redefined
"$work/discovery"
cc tests/plain_tv_guide.c src/addons.c src/watchstate.c src/js.c src/jsw.c src/idcanal.c src/guiafiltro.c src/ltv.c \
  -Isrc -Itests -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -O1 -g -ffunction-sections -fdata-sections -Wl,-dead_strip \
  -Wno-deprecated-declarations -Wno-macro-redefined -o "$work/guide"
"$work/guide"
