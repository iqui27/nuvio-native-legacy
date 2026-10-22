#!/bin/bash
# Inicio de volta da descoberta atomico (dois desc_iniciar juntos = um montar)
# e o fio adiado com a marca copiada no agendamento. Ver o .c.
#
#   bash tests/desc_iniciar_corrida.sh        (SANITIZE=1 para ASan/UBSan, TSAN=1 para TSan)
set -euo pipefail
cd "$(dirname "$0")/.."
tmp="${TMPDIR:-/tmp}"
flags=()
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
if [ "${TSAN:-0}" = 1 ]; then flags+=(-fsanitize=thread); fi
cc ${flags[@]+"${flags[@]}"} -DNV_DESC_MIN_MS=1000ull -DCAT_ESPERA_SILENCIO_MS=300 -DCAT_ESPERA_MIN_MS=600 \
  src/cwordem.c tests/desc_iniciar_corrida.c src/homeestado.c src/catalogo.c \
  src/progresso.c src/js.c src/metaprov.c src/colecoes.c src/redeurl.c src/catordem.c src/cotacat.c \
  -Isrc -Itests -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -o "$tmp/nuvio-desc-iniciar-corrida" -O1 -g -pthread \
  -Wall -Wno-deprecated-declarations -Wno-macro-redefined -Wno-unused-function
"$tmp/nuvio-desc-iniciar-corrida"
