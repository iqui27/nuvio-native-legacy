#!/bin/bash
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
set -eu
cd "$(dirname "$0")/.."
flags=()
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
cc ${flags[@]+"${flags[@]}"} src/catalogo.c tests/cw_local_recente.c -Isrc \
  -o ${TMPDIR:-/tmp}/nuvio-cw-local-recente -O1 -g \
  -Wall -Wno-macro-redefined
${TMPDIR:-/tmp}/nuvio-cw-local-recente
