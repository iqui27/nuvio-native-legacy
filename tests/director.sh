#!/bin/bash
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
set -eu
cd "$(dirname "$0")/.."
cc tests/director_cache.c src/js.c -Isrc -o ${TMPDIR:-/tmp}/nuvio-director-tests -O1 -g -pthread
${TMPDIR:-/tmp}/nuvio-director-tests
