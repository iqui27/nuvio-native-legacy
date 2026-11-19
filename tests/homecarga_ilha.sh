#!/bin/bash
# Quem liga o alerta "Carregamento da Home" da ilha. Ver tests/homecarga_ilha.c.
#   bash tests/homecarga_ilha.sh        (ANTES=1: so os casos que o codigo antigo ja tinha)
set -eu
cd "$(dirname "$0")/.."
extra=()
if [ "${ANTES:-0}" = 1 ]; then extra+=(-DANTES); fi
cc ${extra[@]+"${extra[@]}"} -DCAT_ESPERA_SILENCIO_MS=300 -DCAT_ESPERA_MIN_MS=600 \
  src/cwordem.c tests/homecarga_ilha.c src/homeestado.c src/catalogo.c \
  src/progresso.c src/js.c src/metaprov.c src/colecoes.c src/redeurl.c src/catordem.c src/cotacat.c \
  -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -o /Volumes/ExternalSSD/nv-ui-w1-homecarga -O1 -g -pthread \
  -Wall -Wno-deprecated-declarations -Wno-macro-redefined -Wno-unused-function
/Volumes/ExternalSSD/nv-ui-w1-homecarga
