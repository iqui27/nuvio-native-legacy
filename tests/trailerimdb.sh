#!/bin/bash
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
# Offline: o transporte e um duble de link com resposta sintetica fixa.
set -eu
cd "$(dirname "$0")/.."
cc -O1 -g -Wall -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 src/trailerimdb.c src/perfiltv.c src/js.c tests/trailerimdb.c -L/opt/homebrew/lib -lSDL2 -lpthread -o ${TMPDIR:-/tmp}/nuvio-trailerimdb-tests
${TMPDIR:-/tmp}/nuvio-trailerimdb-tests "${1:-tt2012616}"
