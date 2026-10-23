#!/bin/bash
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
# Intervalo real de swap medido (em vez de 16,7 ms assumido). Ver tests/swapint.c.
set -eu
cd "$(dirname "$0")/.."
cc -Wall -Wextra -O2 tests/swapint.c -Isrc -o ${TMPDIR:-/tmp}/nuvio-swapint-test -lm
${TMPDIR:-/tmp}/nuvio-swapint-test
rm -f ${TMPDIR:-/tmp}/nuvio-swapint-test
grep -q 'swapint_marca' src/main.c || { echo "FALHA: main.c nao mede o swap"; exit 1; }
