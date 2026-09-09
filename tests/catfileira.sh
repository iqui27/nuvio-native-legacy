#!/bin/bash
# Janelas de fileira: tirar um item sem quebrar as outras (#22).
#
#   bash tests/catfileira.sh
#
# So src/catalogo.c, como tests/catcache.sh: a regra e sobre indices e nao
# depende de SDL, de rede nem da descoberta.
set -eu
cd "$(dirname "$0")/.."
flags=()
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
cc "${flags[@]}" src/catalogo.c tests/catfileira.c \
  -Isrc -o /tmp/nuvio-catfileira-tests -O1 -g \
  -Wall -Wno-deprecated-declarations -Wno-macro-redefined
/tmp/nuvio-catfileira-tests
