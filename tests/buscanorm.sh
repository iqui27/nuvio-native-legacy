#!/bin/bash
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
# Normalizacao da busca: latino, romeno, cirilico, UTF-8 (tests/buscanorm.c).
#
#   bash tests/buscanorm.sh
set -eu
cd "$(dirname "$0")/.."
cc tests/buscanorm.c src/buscanorm.c -Isrc -o ${TMPDIR:-/tmp}/nuvio-buscanorm-tests -O1 -g -Wall -Wextra
${TMPDIR:-/tmp}/nuvio-buscanorm-tests
