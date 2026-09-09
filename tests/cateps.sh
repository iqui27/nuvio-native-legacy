#!/bin/bash
# Faixas de episodio contra o catalogo que cresce.
#
#   bash tests/cateps.sh
#
# So src/catalogo.c, como tests/catfileira.sh: a regra e sobre indices e nao
# depende de SDL, de rede nem da descoberta.
set -eu
cd "$(dirname "$0")/.."
flags=()
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
cc "${flags[@]}" src/catalogo.c tests/cateps.c \
  -Isrc -o /tmp/nuvio-cateps-tests -O1 -g \
  -Wall -Wno-deprecated-declarations -Wno-macro-redefined
/tmp/nuvio-cateps-tests
