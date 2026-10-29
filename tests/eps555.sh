#!/bin/bash
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
# #555 B: serie sem episodios (ficha do addon sem videos, id tmdb:, Cinemeta off).
#
#   bash tests/eps555.sh
set -eu
cd "$(dirname "$0")/.."
flags=()
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
# eps555.c inclui descoberta.c inteiro (buscarEps e static), com o mesmo
# conjunto de link de tests/busca555.sh.
cc ${flags[@]+"${flags[@]}"} src/catalogo.c src/cwordem.c tests/eps555.c src/cotacat.c \
  src/js.c src/metaprov.c src/colecoes.c src/redeurl.c src/catordem.c src/artefontes.c \
  -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -o ${TMPDIR:-/tmp}/nuvio-eps555-tests -O1 -g \
  -Wall -Wno-deprecated-declarations -Wno-macro-redefined
${TMPDIR:-/tmp}/nuvio-eps555-tests
