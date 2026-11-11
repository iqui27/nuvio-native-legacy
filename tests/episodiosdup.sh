#!/bin/bash
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
# Episodio duplicado (#328) e listas completas de addon (#391).
#
#   bash tests/episodiosdup.sh
set -eu
cd "$(dirname "$0")/.."
flags=()
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
# episodiosdup.c inclui descoberta.c inteiro (buscarEps e deMeta sao static),
# com o mesmo conjunto de link de tests/cateps.sh.
cc ${flags[@]+"${flags[@]}"} src/catalogo.c src/cwordem.c tests/episodiosdup.c tests/discovery_boundary_stub.c src/cotacat.c \
  src/js.c src/metaep.c src/metaprov.c src/colecoes.c src/redeurl.c src/catordem.c \
  -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -o ${TMPDIR:-/tmp}/nuvio-episodiosdup-tests -O1 -g \
  -Wall -Wno-deprecated-declarations -Wno-macro-redefined
${TMPDIR:-/tmp}/nuvio-episodiosdup-tests
if [ -z "${NV_TEST_787:-}" ] && [ -z "${NV_TEST_787_PARSE:-}" ] && [ -z "${NV_TEST_787_MEMORY:-}" ] && [ -z "${NV_TEST_787_STABLE:-}" ]; then
  NV_TEST_787=1 ${TMPDIR:-/tmp}/nuvio-episodiosdup-tests
  NV_TEST_787_PARSE=1 ${TMPDIR:-/tmp}/nuvio-episodiosdup-tests
  NV_TEST_787_MEMORY=1 ${TMPDIR:-/tmp}/nuvio-episodiosdup-tests
fi
