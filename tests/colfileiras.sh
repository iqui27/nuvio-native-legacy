#!/bin/bash
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
# -DNV_DESC_MIN_MS=0ull: este teste e anterior ao minimo de 10 s entre voltas
# (descdebounce.h, #319); confere a montagem, nao a regra de espera.
set -eu
cd "$(dirname "$0")/.."
flags=(-O1 -g -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 -Wno-macro-redefined -Wno-deprecated-declarations)
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
cc tests/discovery_boundary_stub.c ${flags[@]+"${flags[@]}"} -DNV_DESC_MIN_MS=0ull src/cwordem.c tests/colfileiras.c src/cotacat.c src/js.c src/metaprov.c src/colecoes.c src/redeurl.c src/catordem.c -o ${TMPDIR:-/tmp}/nuvio-colfileiras-tests
${TMPDIR:-/tmp}/nuvio-colfileiras-tests
cc ${flags[@]+"${flags[@]}"} tests/colcusto.c src/js.c src/colecoes.c src/redeurl.c -o ${TMPDIR:-/tmp}/nuvio-colcusto
${TMPDIR:-/tmp}/nuvio-colcusto
