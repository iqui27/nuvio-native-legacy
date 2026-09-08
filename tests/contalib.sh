#!/bin/bash
# Biblioteca e vistos da conta. NAO precisa de rede: o teste alimenta o leitor
# com as respostas que o servidor manda, transcritas do contrato da secao 1.5 do
# PLANO-CONTA-SYNC.md e conferidas contra o app web.
#
#   bash tests/contalib.sh
set -eu
cd "$(dirname "$0")/.."
flags=()
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
# So contalib.c e js.c: o leitor nao depende de SDL, de rede nem da descoberta.
# O catalogo entra como dubl dentro de tests/contalib.c — linkar catalogo.c
# arrastaria descoberta, trakt e progresso junto, e um teste que precisa do app
# inteiro para provar um parser deixa de ser rodado.
cc "${flags[@]}" src/contalib.c src/js.c tests/contalib.c \
  -Isrc -o /tmp/nuvio-contalib-tests -O1 -g \
  -Wall -Wno-deprecated-declarations -Wno-macro-redefined
/tmp/nuvio-contalib-tests
