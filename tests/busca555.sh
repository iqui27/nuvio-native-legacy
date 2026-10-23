#!/bin/bash
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
# Episodio duplicado (#328) e listas completas de addon (#391).
#
#   bash tests/busca555.sh
set -eu
cd "$(dirname "$0")/.."
flags=()
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
# busca555.c inclui descoberta.c inteiro (buscarEps e deMeta sao static),
# com o mesmo conjunto de link de tests/cateps.sh.
cc ${flags[@]+"${flags[@]}"} src/catalogo.c src/cwordem.c tests/busca555.c src/cotacat.c \
  src/js.c src/metaprov.c src/colecoes.c src/redeurl.c src/catordem.c \
  -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -o ${TMPDIR:-/tmp}/nuvio-busca555-tests -O1 -g \
  -Wall -Wno-deprecated-declarations -Wno-macro-redefined
${TMPDIR:-/tmp}/nuvio-busca555-tests
