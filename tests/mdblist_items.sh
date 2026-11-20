#!/bin/bash
# #762 (itens): fonte "mdblist" no vertudo, SEM REDE (duble de rede_baixar).
# Mesmo conjunto de link de tests/collections.sh.
set -eu
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
cd "$(dirname "$0")/.."
flags=(-O1 -g -ffunction-sections -fdata-sections -Wl,-dead_strip -Isrc
       -I/opt/homebrew/include -I/opt/homebrew/include/SDL2
       -Wno-macro-redefined -Wno-deprecated-declarations)
cc "${flags[@]}" src/cwordem.c tests/mdblist_items.c tests/discovery_boundary_stub.c \
   src/js.c src/metaprov.c src/colecoes.c src/redeurl.c \
   -o "${TMPDIR}/nuvio-mdblist-items"
"${TMPDIR}/nuvio-mdblist-items"
