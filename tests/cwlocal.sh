#!/bin/bash
set -eu
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-cwlocal.XXXXXX")
trap 'rm -rf "$work"' EXIT
flags=(-ffunction-sections -fdata-sections)
if [ "$(uname -s)" != Darwin ]; then flags+=(-Wl,--gc-sections -pthread); fi
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
cc "${flags[@]}" src/catalogo.c src/progresso.c src/cwordem.c tests/cwlocal.c \
  src/cotacat.c src/js.c src/metaprov.c src/colecoes.c src/redeurl.c src/catordem.c \
  -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -o "$work/test" -O1 -g -Wall -Wno-deprecated-declarations \
  -Wno-macro-redefined -Wno-unused-function
"$work/test"
