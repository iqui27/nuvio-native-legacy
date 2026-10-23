#!/bin/bash
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
# Cor de destaque: contraste da tinta e da marca de cada acento fixo. Ver tests/acentos.c.
set -eu
cd "$(dirname "$0")/.."
cc -Wall -Wextra -O2 tests/acentos.c src/corviva.c -Isrc -o ${TMPDIR:-/tmp}/nuvio-acentos-test -lm
${TMPDIR:-/tmp}/nuvio-acentos-test
rm -f ${TMPDIR:-/tmp}/nuvio-acentos-test
