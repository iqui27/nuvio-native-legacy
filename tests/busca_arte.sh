#!/bin/bash
set -eu
cd "$(dirname "$0")/.."
cc src/artehero.c src/artereserva.c src/artefontes.c src/artetamanho.c src/js.c src/redeurl.c tests/busca_arte.c \
  -Isrc -Itests -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -o "${TMPDIR:-/tmp}/nuvio-busca-arte-tests" -O1 -g -Wall -Wextra -Wno-deprecated-declarations
"${TMPDIR:-/tmp}/nuvio-busca-arte-tests"
