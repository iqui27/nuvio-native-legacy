#!/bin/bash
# Ordem das fileiras da home vinda da conta. NAO precisa de rede: o teste
# alimenta o leitor com as respostas que o servidor manda, transcritas do
# contrato do Anexo C1 (PLANO-PORT-WEBAPP.md).
#
#   bash tests/catordem.sh
set -eu
cd "$(dirname "$0")/.."
flags=()
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
# So catordem.c e js.c: o leitor nao depende de SDL, de rede nem da descoberta,
# e linkar o app inteiro aqui so tornaria o teste lento e fragil.
cc ${flags[@]+"${flags[@]}"} src/catordem.c src/js.c tests/catordem.c \
  -Isrc -o /tmp/nuvio-catordem-tests -O1 -g \
  -Wall -Wno-deprecated-declarations -Wno-macro-redefined
/tmp/nuvio-catordem-tests
