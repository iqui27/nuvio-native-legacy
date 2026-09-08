#!/bin/bash
set -eu
cd "$(dirname "$0")/.."
flags=()
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
# ARQUIVO PROPRIO E NAO UM CASO EM tests/home.sh: aquele teste ABORTA na
# assercao `nFileiras == 17`, que falha de proposito enquanto o issue #18 nao
# for decidido. Tudo o que fosse acrescentado depois dela nunca rodaria.
#
# Mesma lista de tradutores do home.sh: home.c fala com catalogo, progresso,
# focus, ajustes, colecoes, catordem e fileiras.
cc "${flags[@]}" src/catalogo.c src/progresso.c src/focus.c src/ajustes.c src/colecoes.c src/js.c src/catordem.c src/fileiras.c tests/homepos.c \
  -Isrc -o /tmp/nuvio-homepos-tests -O1 -g -ffunction-sections -fdata-sections \
  -Wl,-dead_strip -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -Wno-deprecated-declarations -Wno-macro-redefined
/tmp/nuvio-homepos-tests
