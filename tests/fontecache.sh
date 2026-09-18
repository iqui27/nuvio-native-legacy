#!/bin/bash
# Cache de fontes por canal e prefetch dos vizinhos do guia (src/fontecache.c).
#
#   bash tests/fontecache.sh
#
# So tests/fontecache.c, que INCLUI src/fontecache.c (o molde de texfila.sh): o
# cache nao depende de SDL alem do tipo Uint32, e a rede, a busca principal e o
# player entram por dubles — sao ENTRADAS do prefetch, e o teste as controla. O
# relogio tambem e do teste (FC_AGORA), para a expiracao ser medida sem dormir.
# Linkar o app inteiro aqui faria addons_consultar ir a rede de verdade.
set -eu
cd "$(dirname "$0")/.."
flags=()
# SANITIZE=1 e address+undefined; SANITIZE=thread e o TSan (o clang nao aceita os
# dois juntos), e e o que vale a pena aqui: o cache e lido de tres fios.
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
if [ "${SANITIZE:-0}" = thread ]; then flags+=(-fsanitize=thread -fno-omit-frame-pointer); fi
cc ${flags[@]+"${flags[@]}"} ${NUVIO_CFLAGS:-} tests/fontecache.c \
  -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -o /tmp/nuvio-fontecache-tests -O1 -g -Wall -Wextra -lpthread \
  -Wno-deprecated-declarations -Wno-macro-redefined
/tmp/nuvio-fontecache-tests
