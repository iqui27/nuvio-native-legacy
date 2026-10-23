#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-cw-stats524.XXXXXX")
trap 'rm -rf "$work"' EXIT
flags=(-O1 -g -Wall -Wno-deprecated-declarations -Wno-macro-redefined -Wno-unused-function
  -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2)
cc "${flags[@]}" src/catalogo.c src/progresso.c src/cwordem.c tests/cw_stats_identity524.c \
  src/cotacat.c src/js.c src/metaprov.c src/colecoes.c src/redeurl.c src/catordem.c \
  -Wl,-dead_strip -o "$work/cw"
"$work/cw"
cc "${flags[@]}" tests/stats_identity524.c src/metaep.c tests/stub_fichameta.c \
  src/js.c src/metaprov.c -Wl,-dead_strip -lpthread -o "$work/stats"
"$work/stats"
