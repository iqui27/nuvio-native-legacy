#!/bin/bash
set -eu
cd "$(dirname "$0")/.."
flags=(-O1 -g -Isrc -ffunction-sections -fdata-sections -Wl,-dead_strip \
       -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
       -Wno-deprecated-declarations -Wno-macro-redefined)
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
# SO descoberta.c: as duas funcoes publicas testadas (desc_manifesto_cache_*)
# nao chamam rede, addons.c, SDL, ajustes, fileiras, colecoes, trakt nem
# nuvem — so a trava e a tabela estaticas do proprio arquivo. -dead_strip com
# secoes por funcao descarta o resto que este teste nao alcanca, entao nenhum
# desses modulos precisa de stub (mesma receita de tests/homepos.sh).
cc "${flags[@]}" src/descoberta.c tests/manifesto_cache.c \
  -o /tmp/nuvio-manifesto-cache-tests
/tmp/nuvio-manifesto-cache-tests
