#!/bin/bash
# Ordem das fileiras da home. Sem SDL, sem rede e sem disco: fileiras.c so fala
# com dados_gravar/dados_ler, e o teste os substitui por dubles.
set -eu
cd "$(dirname "$0")/.."
flags=()
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
cc "${flags[@]}" src/fileiras.c tests/fileiras.c \
  -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -o /tmp/nuvio-fileiras-tests -O1 -g \
  -Wall -Wno-deprecated-declarations -Wno-macro-redefined
/tmp/nuvio-fileiras-tests
