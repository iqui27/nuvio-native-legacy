#!/bin/bash
# #488: a volta pedida na troca de perfil espera o ciclo da conta. Ver o .c.
#
#   bash tests/troca_perfil_espera_conta.sh        (SANITIZE=1 para ASan/UBSan, SANITIZE=thread para TSan)
set -euo pipefail
cd "$(dirname "$0")/.."
tmp="${TMPDIR:-/tmp}"
flags=()
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
if [ "${SANITIZE:-0}" = thread ]; then flags+=(-fsanitize=thread); fi
cc ${flags[@]+"${flags[@]}"} -DNV_DESC_MIN_MS=1000ull -DCAT_ESPERA_SILENCIO_MS=300 -DCAT_ESPERA_MIN_MS=600 \
  src/cwordem.c tests/troca_perfil_espera_conta.c src/homeestado.c src/catalogo.c \
  src/progresso.c src/js.c src/metaprov.c src/colecoes.c src/redeurl.c src/catordem.c src/cotacat.c \
  -Isrc -Itests -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -o "$tmp/nuvio-troca-perfil-espera-conta" -O1 -g -pthread \
  -Wall -Wno-deprecated-declarations -Wno-macro-redefined -Wno-unused-function
"$tmp/nuvio-troca-perfil-espera-conta"
