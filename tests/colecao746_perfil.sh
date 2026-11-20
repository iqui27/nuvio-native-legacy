#!/bin/bash
# #746: a colecao da conta sobrevive a troca de perfil com forma e capa.
set -eu
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
cd "$(dirname "$0")/.."
cc -O1 -g -Wall -Wextra -Isrc -Wno-misleading-indentation \
   src/colecoes.c src/redeurl.c src/js.c tests/colecao746_perfil.c \
   -o "${TMPDIR}/nuvio-colecao746-perfil" -lpthread
"${TMPDIR}/nuvio-colecao746-perfil"
