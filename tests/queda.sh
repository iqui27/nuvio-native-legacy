#!/bin/bash
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
set -e
cd "$(dirname "$0")/.."
cc tests/queda.c src/queda.c -o ${TMPDIR:-/tmp}/nuvio-queda-teste -Wall -Wextra
${TMPDIR:-/tmp}/nuvio-queda-teste
