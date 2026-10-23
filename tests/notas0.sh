#!/bin/bash
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
# "notas=0/11" (#205): extras_pedir com rede falsa — o Trakt sozinho publica a
# sua nota, e com chave do MDBList os POSTs por provedor publicam as demais.
# Sem libcurl, sem conta: NV_EXTRAS_TESTE desvia a rede para o teste.
set -eu
cd "$(dirname "$0")/.."
cc -O1 -g -Wall -Wextra -DNV_EXTRAS_TESTE -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  src/extras.c src/notasfontes.c src/js.c tests/notas0.c \
  -lpthread -o ${TMPDIR:-/tmp}/nuvio-notas0-tests
${TMPDIR:-/tmp}/nuvio-notas0-tests
