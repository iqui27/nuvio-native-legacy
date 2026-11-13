#!/bin/bash
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
# Offline: o transporte e um duble de link com respostas sinteticas fixas.
set -eu
cd "$(dirname "$0")/.."
cc -O1 -g -Wall -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 src/trailerapple.c src/js.c tests/trailerapple.c -L/opt/homebrew/lib -lSDL2 -lpthread -o ${TMPDIR:-/tmp}/nuvio-trailerapple-tests
${TMPDIR:-/tmp}/nuvio-trailerapple-tests
