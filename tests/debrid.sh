#!/bin/bash
# src/redeurl.c entra aqui e src/rede.c NAO: tests/debrid.c finge o transporte
# (rede_postar_st / rede_baixar_st), entao linkar rede.c inteiro daria simbolo
# duplicado. redeurl.c e so a redacao de credencial do log, sem rede nenhuma —
# e o teste exercita a de verdade em vez de um stub que nao esconde nada.
set -eu
cd "$(dirname "$0")/.."
cc -O1 -g -Wall -Wextra -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 -Wno-deprecated-declarations \
  src/debrid.c src/stream_parse.c src/js.c src/redeurl.c tests/debrid.c -o /tmp/nuvio-debrid-tests
/tmp/nuvio-debrid-tests
