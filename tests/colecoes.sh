#!/bin/bash
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
set -eu
cd "$(dirname "$0")/.."
cc -O1 -g -Wall -Wextra -Isrc -Wno-misleading-indentation src/colecoes.c src/redeurl.c src/js.c tests/colecoes.c -o ${TMPDIR:-/tmp}/nuvio-colecoes-tests
${TMPDIR:-/tmp}/nuvio-colecoes-tests
