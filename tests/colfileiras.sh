#!/bin/bash
set -eu
cd "$(dirname "$0")/.."
flags=(-O1 -g -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 -Wno-macro-redefined -Wno-deprecated-declarations)
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
cc "${flags[@]}" tests/colfileiras.c src/js.c src/colecoes.c src/redeurl.c src/catordem.c -o /tmp/nuvio-colfileiras-tests
/tmp/nuvio-colfileiras-tests
cc "${flags[@]}" tests/colcusto.c src/js.c src/colecoes.c src/redeurl.c -o /tmp/nuvio-colcusto
/tmp/nuvio-colcusto
