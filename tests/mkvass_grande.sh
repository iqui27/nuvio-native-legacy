#!/bin/bash
# #269: legenda de texto embutida num MKV de mais de 2 GiB. Ver tests/mkvass_grande.c.
#   bash tests/mkvass_grande.sh                 # Mac (64 bits)
#   MKVASS_GRANDE_ARM=1 bash tests/mkvass_grande.sh   # tambem no ARMv7 de 32 bits (nuvio-tpk-sdk)
set -eu
cd "$(dirname "$0")/.."
DIR=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-mkvass-grande.XXXXXX")
trap 'rm -rf "$DIR"' EXIT
FONTES="tests/mkvass_grande.c src/mkvass.c src/legenda.c src/assrender.c src/dados.c"
cc -Isrc -O1 -g -Wall -Wextra -Wno-deprecated-declarations $FONTES -o "$DIR/test"
mkdir "$DIR/dados"
(cd "$DIR" && NUVIO_DADOS="$DIR/dados" ./test)
if [ "${MKVASS_GRANDE_ARM:-0}" = 1 ]; then
  docker run --rm -v "$PWD:/src:ro" -w /src nuvio-tpk-sdk sh -c \
    "gcc -Isrc -O1 -Wall -Wextra -Wno-deprecated-declarations $FONTES -o /tmp/t -pthread -lm && \
     mkdir -p /tmp/d && cd /tmp && NUVIO_DADOS=/tmp/d ./t"
fi
