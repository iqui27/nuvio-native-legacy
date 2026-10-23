#!/bin/bash
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
# O hero desenha a arte do titulo em foco, nao a do anterior (issue #118).
# Ver tests/divididosplit.c tests/amigosfil_stub.c.
set -eu
cd "$(dirname "$0")/.."
cc src/catalogo.c src/progresso.c src/focus.c src/ajustes.c src/ajlog.c src/fonteregra.c src/regexjs.c src/posterprov.c src/redeurl.c src/colecoes.c src/js.c src/catordem.c src/fileiras.c src/artehero.c src/cwordem.c src/cwretido.c tests/divididosplit.c tests/amigosfil_stub.c \
  -Isrc -o ${TMPDIR:-/tmp}/nuvio-divididosplit -O1 -g -ffunction-sections -fdata-sections \
  -Wl,-dead_strip -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -Wno-deprecated-declarations -Wno-macro-redefined
${TMPDIR:-/tmp}/nuvio-divididosplit
