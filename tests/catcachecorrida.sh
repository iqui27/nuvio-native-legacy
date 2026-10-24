#!/bin/bash
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
# Cache do catalogo gravado enquanto outro fio mexe no bloco da tela.
#
#   bash tests/catcachecorrida.sh [publicacoes]
#
# ASan LIGADO POR PADRAO (SANITIZE=0 desliga): o defeito e uma escrita alem do
# malloc do cache codificado, e sem o sanitizador ela so aparece como abort do
# glibc num free qualquer, depois — que e exatamente a queda da 2.0.0 na LG.
set -eu
cd "$(dirname "$0")/.."
flags=()
if [ "${SANITIZE:-1}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
cc ${flags[@]+"${flags[@]}"} -DNV_CAT_TEST_CACHE_MEIO=nv_cat_teste_cache_meio \
  src/catalogo.c src/contalib.c src/js.c src/vistoep.c src/colecoes.c src/redeurl.c \
  tests/catcachecorrida.c tests/catalogo_boundary_stub.c \
  -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 -o ${TMPDIR:-/tmp}/nuvio-catcachecorrida-tests -O1 -g -pthread \
  -Wall -Wno-deprecated-declarations -Wno-macro-redefined
${TMPDIR:-/tmp}/nuvio-catcachecorrida-tests "$@"
