#!/bin/bash
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
# Retorno de fim de fileira: a seta que bate na borda desloca e volta com
# mola, sem mexer no foco. Ver tests/bordafileira.c tests/amigosfil_stub.c.
#
# Sem sanitize por padrao pelo mesmo motivo de tests/fimfileira.sh (ASan
# trava no dyld antes do main). SANITIZE=1 continua aceito.
set -eu
cd "$(dirname "$0")/.."
flags=()
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
cc ${flags[@]+"${flags[@]}"} src/catalogo.c src/progresso.c src/focus.c src/ajustes.c src/posterprov.c src/redeurl.c src/colecoes.c src/js.c src/catordem.c src/fileiras.c src/artehero.c src/cwordem.c src/cwretido.c src/curadoria.c tests/bordafileira.c tests/amigosfil_stub.c tests/home_boundary_stub.c \
  -Isrc -o ${TMPDIR:-/tmp}/nuvio-bordafileira-tests -O1 -g -ffunction-sections -fdata-sections \
  -DCURADORIA_TESTE -Wl,-dead_strip -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -Wno-deprecated-declarations -Wno-macro-redefined
${TMPDIR:-/tmp}/nuvio-bordafileira-tests "$@"
