#!/bin/bash
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
# A escolha do titulo do cartao unico da fileira Herói (#205) e a leitura do
# JSON da curadoria (formato do servidor/recomendacoes). Sem SDL e sem rede:
# o fio de rede fica fora com -DCURADORIA_TESTE e o teste alimenta o parser.
#   bash tests/curadoria.sh
set -eu
cd "$(dirname "$0")/.."
flags=()
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
cc ${flags[@]+"${flags[@]}"} -DCURADORIA_TESTE src/curadoria.c src/js.c tests/curadoria.c \
  -Isrc -o ${TMPDIR:-/tmp}/nuvio-curadoria-tests -O1 -g \
  -Wall -Wno-deprecated-declarations -Wno-macro-redefined
${TMPDIR:-/tmp}/nuvio-curadoria-tests
