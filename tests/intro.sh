#!/bin/bash
# Marcadores do TheIntroDB: leitura da resposta.
#
#   bash tests/intro.sh
#
# So src/intro.c e src/js.c — o leitor nao depende de rede (as cargas do teste
# sao respostas reais capturadas com curl) nem de SDL.
set -eu
cd "$(dirname "$0")/.."
flags=()
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
cc "${flags[@]}" src/intro.c src/js.c tests/intro.c \
  -Isrc -o /tmp/nuvio-intro-tests -O1 -g \
  -Wall -Wno-deprecated-declarations -Wno-macro-redefined
/tmp/nuvio-intro-tests
