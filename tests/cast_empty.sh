#!/bin/bash
# Offline TMDB fallback for metadata with no cast names.
set -eu
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-cast-empty.XXXXXX")
trap 'rm -rf "$work"' EXIT
cc src/catalogo.c src/cwordem.c tests/cast_empty.c src/cotacat.c \
  src/js.c src/metaprov.c src/colecoes.c src/redeurl.c src/catordem.c src/artefontes.c \
  -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -o "$work/test" -O1 -g -Wall -Wno-deprecated-declarations -Wno-macro-redefined
"$work/test"
