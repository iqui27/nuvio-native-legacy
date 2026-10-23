#!/bin/bash
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
# Politica do LSRegister (src/lsregistro.c). Sem webOS: o video.c chama a lib
# por dlopen, e o que decide quando tentar e o que o codigo quer dizer mora
# num modulo puro justamente para rodar aqui.
set -eu
cd "$(dirname "$0")/.."
cc -O1 -g -Wall -Wextra -Isrc src/lsregistro.c tests/lsregistro.c -o ${TMPDIR:-/tmp}/nuvio-lsregistro
${TMPDIR:-/tmp}/nuvio-lsregistro
