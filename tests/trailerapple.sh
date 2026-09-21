#!/bin/bash
# Precisa de rede: fala com tv.apple.com.
set -eu
cd "$(dirname "$0")/.."
cc -O1 -g -Wall -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 src/trailerapple.c src/rede.c src/redeurl.c src/js.c tests/trailerapple.c -L/opt/homebrew/lib -lSDL2 -lcurl -lpthread -o /tmp/nuvio-trailerapple-tests
/tmp/nuvio-trailerapple-tests
