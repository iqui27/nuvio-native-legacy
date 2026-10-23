#!/bin/bash
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
# Precisa de rede: fala com tv.apple.com.
set -eu
cd "$(dirname "$0")/.."
cc -O1 -g -Wall -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 src/trailerapple.c src/rede.c src/redeurl.c src/js.c tests/trailerapple.c -L/opt/homebrew/lib -lSDL2 -lcurl -lpthread -o ${TMPDIR:-/tmp}/nuvio-trailerapple-tests
${TMPDIR:-/tmp}/nuvio-trailerapple-tests
