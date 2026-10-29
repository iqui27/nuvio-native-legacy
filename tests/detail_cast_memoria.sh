#!/bin/bash
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
# #205: catalog-memory cast without TMDB person ids gets /credits match on open.
set -eu
cd "$(dirname "$0")/.."
flags=()
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
cc ${flags[@]+"${flags[@]}"} src/catalogo.c src/cwordem.c tests/detalheanime.c src/cotacat.c \
  src/js.c src/metaprov.c src/colecoes.c src/redeurl.c src/catordem.c src/artefontes.c \
  -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -o "${TMPDIR}/nuvio-detail-cast-memoria-tests" -O1 -g \
  -Wall -Wno-deprecated-declarations -Wno-macro-redefined
"${TMPDIR}/nuvio-detail-cast-memoria-tests" | grep -F '#205'
