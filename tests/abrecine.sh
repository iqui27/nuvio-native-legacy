#!/bin/bash
# Abertura Cinema (#205): a regra da tela de abertura premium. Ver o .c.
set -eu
cd "$(dirname "$0")/.."
cc src/abrecine.c tests/abrecine.c -Isrc -o "${TMPDIR:-/tmp}/nuvio-abrecine" \
  -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 -L/opt/homebrew/lib -lSDL2 -lm
"${TMPDIR:-/tmp}/nuvio-abrecine"
