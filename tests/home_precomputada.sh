#!/bin/bash
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
# Home precomputada: fileiras de catalogo servidas do cache enquanto frescas.
#
#   bash tests/home_precomputada.sh
#
# montar() de verdade (descoberta.c por #include); ver o cabecalho de
# tests/home_precomputada.c.
set -eu
cd "$(dirname "$0")/.."
flags=()
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
cc tests/discovery_boundary_stub.c ${flags[@]+"${flags[@]}"} -DNV_DESC_MIN_MS=0ull \
  src/cwordem.c tests/home_precomputada.c src/homeestado.c src/catalogo.c \
  src/progresso.c src/js.c src/metaep.c src/metaprov.c src/colecoes.c src/redeurl.c src/catordem.c src/cotacat.c \
  -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -o ${TMPDIR:-/tmp}/nuvio-home-precomputada-tests -O1 -g -pthread \
  -Wall -Wno-deprecated-declarations -Wno-macro-redefined -Wno-unused-function
${TMPDIR:-/tmp}/nuvio-home-precomputada-tests
