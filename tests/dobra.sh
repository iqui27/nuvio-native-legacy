#!/bin/bash
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
# #144: letra estilizada -> letra comum. Ver tests/dobra.c.
#   bash tests/dobra.sh
set -eu
cd "$(dirname "$0")/.."
cc src/dobra.c tests/dobra.c -Isrc -o ${TMPDIR:-/tmp}/nuvio-dobra-tests -O1 -g -std=c11 -Wall -Wextra
${TMPDIR:-/tmp}/nuvio-dobra-tests
