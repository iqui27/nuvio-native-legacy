#!/bin/bash
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
# Saude da rede para a ilha (sem internet / de volta). Sem rede; ver o .c.
set -eu
cd "$(dirname "$0")/.."
cc tests/redesaude.c src/redesaude.c -Isrc -o ${TMPDIR:-/tmp}/nuvio-redesaude -lpthread
${TMPDIR:-/tmp}/nuvio-redesaude
