#!/bin/bash
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
# Relogio interpolado da legenda ASS (#92). Ver tests/relogio.c.
set -eu
cd "$(dirname "$0")/.."
cc -Wall -O1 tests/relogio.c src/relogio.c -o ${TMPDIR:-/tmp}/nuvio-relogio-test -lm
${TMPDIR:-/tmp}/nuvio-relogio-test
