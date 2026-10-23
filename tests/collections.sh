#!/bin/bash
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
set -eu
cd "$(dirname "$0")/.."
flags=(-O1 -g -ffunction-sections -fdata-sections -Wl,-dead_strip -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 -Wno-macro-redefined -Wno-deprecated-declarations)
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
cc ${flags[@]+"${flags[@]}"} src/cwordem.c tests/collections_data.c src/js.c src/metaprov.c src/colecoes.c src/redeurl.c -o ${TMPDIR:-/tmp}/nuvio-collections-data-tests
${TMPDIR:-/tmp}/nuvio-collections-data-tests
cc ${flags[@]+"${flags[@]}"} tests/badges.c -o ${TMPDIR:-/tmp}/nuvio-badges-tests
${TMPDIR:-/tmp}/nuvio-badges-tests
