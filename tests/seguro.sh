#!/bin/bash
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
# Modo seguro: diario, reversao apos queda, confirmacao e laco de quedas.
#
#   bash tests/seguro.sh
set -eu
cd "$(dirname "$0")/.."
flags=()
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
cc ${flags[@]+"${flags[@]}"} src/seguro.c tests/seguro.c -Isrc \
  -o ${TMPDIR:-/tmp}/nuvio-seguro-tests -O1 -g -Wall -Wno-deprecated-declarations
${TMPDIR:-/tmp}/nuvio-seguro-tests
