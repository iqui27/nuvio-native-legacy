#!/bin/bash
# Actual Search worker and parser; public response fixture, no live HTTP.
set -eu
cd "$(dirname "$0")/.."
tmp=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-busca-limite.XXXXXX")
trap 'rm -rf "$tmp"' EXIT
flags=()
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
cc ${flags[@]+"${flags[@]}"} src/catalogo.c src/cwordem.c tests/busca_limite.c src/cotacat.c \
  src/js.c src/metaprov.c src/colecoes.c src/redeurl.c src/catordem.c src/artefontes.c \
  -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -o "$tmp/t" -O1 -g -Wall -Wno-deprecated-declarations -Wno-macro-redefined
"$tmp/t"
