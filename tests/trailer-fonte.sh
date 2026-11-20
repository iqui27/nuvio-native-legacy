#!/bin/bash
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
# Sem rede nem tela: qual fonte de trailer cada ajuste tenta, nas duas TVs.
# Duas vezes: como o .wgt/LG compilam e como o .tpk (-DNV_TPK) compila.
set -eu
cd "$(dirname "$0")/.."
cc -O1 -g -Wall -Isrc -I/opt/homebrew/include tests/trailer-fonte.c src/trailerfonte.c -o ${TMPDIR:-/tmp}/nuvio-trailer-fonte
${TMPDIR:-/tmp}/nuvio-trailer-fonte
cc -O1 -g -Wall -DNV_TPK -Isrc -I/opt/homebrew/include tests/trailer-fonte.c src/trailerfonte.c -o ${TMPDIR:-/tmp}/nuvio-trailer-fonte-tpk
${TMPDIR:-/tmp}/nuvio-trailer-fonte-tpk
