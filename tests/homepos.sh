#!/bin/bash
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
set -eu
cd "$(dirname "$0")/.."
flags=()
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
# Teste separado para preservar foco na sessao e reiniciar na primeira coluna.
#
# Mesma lista de tradutores do home.sh: home.c fala com catalogo, progresso,
# focus, ajustes, colecoes, catordem e fileiras.
cc ${flags[@]+"${flags[@]}"} src/catalogo.c src/progresso.c src/focus.c src/ajustes.c src/posterprov.c src/redeurl.c src/colecoes.c src/js.c src/catordem.c src/fileiras.c src/homefil.c src/artehero.c src/cwordem.c src/cwretido.c src/curadoria.c tests/homepos.c tests/amigosfil_stub.c \
  -Isrc -o ${TMPDIR:-/tmp}/nuvio-homepos-tests -O1 -g -ffunction-sections -fdata-sections \
  -DCURADORIA_TESTE -Wl,-dead_strip -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -Wno-deprecated-declarations -Wno-macro-redefined
${TMPDIR:-/tmp}/nuvio-homepos-tests
