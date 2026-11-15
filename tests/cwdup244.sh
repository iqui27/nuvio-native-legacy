#!/bin/bash
# #244: ver tests/cwdup244.c
set -eu
cd "$(dirname "$0")/.."
export TMPDIR="${TMPDIR:-/tmp}"
cc ${flags[@]+"${flags[@]}"} src/catalogo.c src/progresso.c src/cwordem.c src/cwretido.c tests/cwdup244.c src/cotacat.c \
  src/js.c src/metaprov.c src/colecoes.c src/redeurl.c src/catordem.c \
  tests/discovery_boundary_stub.c \
  -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -o "$TMPDIR/nuvio-cwdup244" -O1 -g \
  -Wall -Wno-deprecated-declarations -Wno-macro-redefined -Wno-unused-function
"$TMPDIR/nuvio-cwdup244"
