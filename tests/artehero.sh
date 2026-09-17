#!/bin/bash
# Politica de arte de TELA CHEIA. Sem SDL, sem rede e sem disco: artehero.c so
# olha a url que o catalogo guarda e devolve outra string.
#
#   bash tests/artehero.sh
set -eu
cd "$(dirname "$0")/.."
flags=()
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
cc ${flags[@]+"${flags[@]}"} src/artehero.c tests/artehero.c \
  -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -o /tmp/nuvio-artehero-tests -O1 -g \
  -Wall -Wextra -Wno-deprecated-declarations -Wno-macro-redefined
/tmp/nuvio-artehero-tests
