#!/bin/bash
# Issue #130: quantas URLs a escolha automatica toca. Ver tests/fonteauto.c.
#
#   bash tests/fonteauto.sh
set -eu
cd "$(dirname "$0")/.."
cc -O1 -g -Wall -Wextra -Isrc src/fonteauto.c tests/fonteauto.c -o /tmp/nuvio-fonteauto-tests
/tmp/nuvio-fonteauto-tests

# CONTRATO: a verificacao de filme nao volta a abrir fios. O lote paralelo de
# 4 fios era a causa dos 6 arquivos no TorBox; quem trouxer o paralelismo de
# volta por velocidade tem de passar por este teste e pelo comentario de
# streams.c antes.
corpo=$(awk '/^int stream_primeira_boa\(/,/^}/' src/streams.c)
if [ -z "$corpo" ]; then echo "fonteauto: stream_primeira_boa sumiu de streams.c"; exit 1; fi
if printf '%s' "$corpo" | grep -q 'pthread_create'; then
  echo "fonteauto: stream_primeira_boa voltou a criar fios (issue #130)"; exit 1
fi
if ! printf '%s' "$corpo" | grep -q 'fonteauto_primeira'; then
  echo "fonteauto: stream_primeira_boa nao passa por fonteauto_primeira"; exit 1
fi
echo "fonteauto: contrato de streams.c ok"
