#!/bin/bash
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
# Rotacao automatica do destaque: o que a segura e o que a desliga (voltar um
# titulo, trailer tocando). Ver tests/herorotacao.c tests/amigosfil_stub.c.
set -eu
cd "$(dirname "$0")/.."
cc src/catalogo.c src/progresso.c src/focus.c src/ajustes.c src/posterprov.c src/redeurl.c src/colecoes.c src/js.c src/catordem.c src/fileiras.c src/homefil.c src/artehero.c src/cwordem.c src/cwretido.c src/ajlog.c src/fonteregra.c src/regexjs.c src/curadoria.c tests/herorotacao.c tests/amigosfil_stub.c tests/home_boundary_stub.c \
  -Isrc -DNV_SHOT_HOOKS -DCURADORIA_TESTE -o ${TMPDIR:-/tmp}/nuvio-herorotacao-tests -O1 -g -ffunction-sections -fdata-sections \
  -DNV_SHOT_HOOKS -DCURADORIA_TESTE -Wl,-dead_strip -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -Wno-deprecated-declarations -Wno-macro-redefined
${TMPDIR:-/tmp}/nuvio-herorotacao-tests "$@"
