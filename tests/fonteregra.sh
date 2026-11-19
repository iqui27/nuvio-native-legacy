#!/bin/bash
# #202: regras do auto-play (permitidos, escopo, regex, os outros) e as chaves
# do oficial na conta. Ver tests/fonteregra.c.
#
#   bash tests/fonteregra.sh
set -eu
cd "$(dirname "$0")/.."
NUVIO_DADOS=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-fonteregra.XXXXXX")
export NUVIO_DADOS
trap 'rm -rf "$NUVIO_DADOS"' EXIT
cc -O1 -g -Wall -Wextra -Isrc src/fonteregra.c src/regexjs.c src/dados.c src/js.c tests/fonteregra.c \
  -o "$NUVIO_DADOS/fonteregra-tests" -lpthread
"$NUVIO_DADOS/fonteregra-tests"
