#!/bin/bash
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
# Issue #208: o "Retomar" vale em toda copia do titulo, nao so na do CW.
#
#   SANITIZE=1 bash tests/retomar_copias.sh
set -eu
cd "$(dirname "$0")/.."
flags=()
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
cc ${flags[@]+"${flags[@]}"} src/catalogo.c tests/retomar_copias.c \
  -Isrc -o ${TMPDIR:-/tmp}/nuvio-retomar-copias-tests -O1 -g \
  -Wall -Wno-deprecated-declarations -Wno-macro-redefined
${TMPDIR:-/tmp}/nuvio-retomar-copias-tests
