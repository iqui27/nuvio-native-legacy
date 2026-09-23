#!/bin/bash
# Reserva de arte pelo TMDB. Sem rede: o teste substitui rede_baixar e a chave.
#
#   bash tests/artereserva.sh
set -eu
cd "$(dirname "$0")/.."
flags=()
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
cc ${flags[@]+"${flags[@]}"} src/artereserva.c src/js.c tests/artereserva.c \
  -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -o /tmp/nuvio-artereserva-tests -O1 -g -Wno-macro-redefined \
  -Wall -Wextra -Wno-deprecated-declarations
/tmp/nuvio-artereserva-tests
