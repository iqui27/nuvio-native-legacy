#!/bin/bash
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
# Sem rede: a escolha da variante de midia que a Samsung entrega ao <video>.
set -eu
cd "$(dirname "$0")/.."
cc -O1 -g -Wall -Wno-unused-function -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 tests/trailerapple-midia.c src/js.c -L/opt/homebrew/lib -lSDL2 -lpthread -o ${TMPDIR:-/tmp}/nuvio-trailerapple-midia
${TMPDIR:-/tmp}/nuvio-trailerapple-midia
