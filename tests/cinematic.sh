#!/bin/bash
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
set -eu
cd "$(dirname "$0")/.."
cc -Isrc tests/cinematic_selection.c src/colecoes.c src/js.c -o ${TMPDIR:-/tmp}/nuvio-cinematic-selection-tests
${TMPDIR:-/tmp}/nuvio-cinematic-selection-tests
node tests/cinematic-images.mjs "$@"
