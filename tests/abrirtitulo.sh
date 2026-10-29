#!/bin/bash
# Abrir titulo sem espera (R2) (tests/abrirtitulo.c).
#
#   bash tests/abrirtitulo.sh
set -eu
cd "$(dirname "$0")/.."
flags=()
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-abrirtitulo.XXXXXX")
trap 'rm -rf "$work"' EXIT
# abrirtitulo.c inclui descoberta.c inteiro (buscarEps e deMeta sao static),
# com o mesmo conjunto de link de tests/cateps.sh.
cc ${flags[@]+"${flags[@]}"} src/catalogo.c src/cwordem.c tests/abrirtitulo.c src/cotacat.c \
  src/js.c src/metaprov.c src/colecoes.c src/redeurl.c src/catordem.c \
  -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -o "$work/abrirtitulo-tests" -O1 -g -DCOM_SEMENTE \
  -Wall -Wno-deprecated-declarations -Wno-macro-redefined
"$work/abrirtitulo-tests"
