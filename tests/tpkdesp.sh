#!/bin/bash
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
# Saida pela TV (Exit, desligar, janela escondida) nao conta como queda no .tpk.
set -eu
cd "$(dirname "$0")/.."
cc -fsanitize=address,undefined src/tpkdesp.c tests/tpkdesp.c -Isrc -o ${TMPDIR:-/tmp}/nuvio-tpkdesp-tests -O1 -g -Wall
${TMPDIR:-/tmp}/nuvio-tpkdesp-tests
