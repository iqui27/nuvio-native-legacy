#!/bin/bash
# Trakt (watchlist/colecao) na tela sem esperar os addons; volta condenada para
# no meio em vez de ir ate o fim; addons novos antes da leitura da lista nao
# condenam; catalogo pendurado nao segura a rodada. Ver tests/montagem_cedo.c.
#
#   bash tests/montagem_cedo.sh        (SANITIZE=1 para ASan/UBSan)
set -eu
cd "$(dirname "$0")/.."
flags=()
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
# Tetos de espera do catalogo lento encurtados: o caso 6 nao precisa de 4 s.
cc ${flags[@]+"${flags[@]}"} -DCAT_ESPERA_SILENCIO_MS=300 -DCAT_ESPERA_MIN_MS=600 \
  src/cwordem.c tests/montagem_cedo.c src/homeestado.c src/catalogo.c \
  src/progresso.c src/js.c src/colecoes.c src/redeurl.c src/catordem.c src/cotacat.c \
  -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -o /tmp/nuvio-montagem-cedo-tests -O1 -g -pthread \
  -Wall -Wno-deprecated-declarations -Wno-macro-redefined -Wno-unused-function
/tmp/nuvio-montagem-cedo-tests
