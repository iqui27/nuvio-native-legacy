#!/bin/bash
# Cache em disco do catalogo: onde ele e gravado e de quem ele e.
#
#   bash tests/catcache.sh
#
# So src/catalogo.c: o cache nao depende de SDL, de rede nem da descoberta, e
# linkar o app inteiro aqui so tornaria o teste lento e fragil (mesma razao de
# tests/catordem.sh). A pasta gravavel, o usuario logado e o perfil ativo entram
# por dubles em tests/catcache.c — sao ENTRADAS do teste, nao do ambiente.
#
# PARA RODAR CONTRA A VERSAO ANTERIOR AO CONSERTO (git stash), acrescente
# -DNV_CACHE_SEM_APAGAR: cat_apagar_cache ainda nao existia la e sem isso o
# teste falharia no link em vez de falhar na assercao, que e o que interessa.
set -eu
cd "$(dirname "$0")/.."
flags=()
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
cc "${flags[@]}" ${NUVIO_CFLAGS:-} src/catalogo.c tests/catcache.c \
  -Isrc -o /tmp/nuvio-catcache-tests -O1 -g \
  -Wall -Wno-deprecated-declarations -Wno-macro-redefined
/tmp/nuvio-catcache-tests
