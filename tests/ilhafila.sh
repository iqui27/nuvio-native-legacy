#!/bin/bash
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
# Fila da ilha do relogio (prioridade, +N, central). Sem janela; ver o .c.
set -eu
cd "$(dirname "$0")/.."
cc tests/ilhafila.c src/ilha.c -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -Wl,-undefined,dynamic_lookup -o ${TMPDIR:-/tmp}/nuvio-ilhafila \
  -Wno-macro-redefined -Wno-deprecated-declarations
${TMPDIR:-/tmp}/nuvio-ilhafila
