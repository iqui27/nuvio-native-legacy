#!/bin/bash
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
# Fim de fileira (issue #65): o foco chega a ultima coluna e continua recebendo
# seta, com republicacao concorrente do catalogo. Ver tests/fimfileira.c tests/amigosfil_stub.c.
#
# Sem janela nem inicializacao SDL: o relogio e um duble deterministico do
# teste. SANITIZE=1 verifica a logica sem carregar SDL/AppKit no macOS.
set -eu
cd "$(dirname "$0")/.."
flags=()
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
cc ${flags[@]+"${flags[@]}"} src/catalogo.c src/progresso.c src/focus.c src/ajustes.c src/posterprov.c src/redeurl.c src/colecoes.c src/js.c src/catordem.c src/fileiras.c src/artehero.c src/cwordem.c src/cwretido.c src/curadoria.c tests/fimfileira.c tests/amigosfil_stub.c tests/home_boundary_stub.c \
  -Isrc -o ${TMPDIR:-/tmp}/nuvio-fimfileira-tests -O1 -g -ffunction-sections -fdata-sections \
  -DCURADORIA_TESTE -Wl,-dead_strip -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -Wno-deprecated-declarations -Wno-macro-redefined
${TMPDIR:-/tmp}/nuvio-fimfileira-tests "$@"
