#!/bin/bash
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
# Contagem "voce viu N de M". Sem SDL, rede ou disco.
set -eu
cd "$(dirname "$0")/.."
cc src/vistocont.c tests/vistocont.c -Isrc -o ${TMPDIR:-/tmp}/nuvio-vistocont-tests -O1 -g -Wall -Wextra
${TMPDIR:-/tmp}/nuvio-vistocont-tests
