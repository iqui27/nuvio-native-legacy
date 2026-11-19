#!/bin/bash
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
# Regra da cota de catalogos por addon (src/cotacat.c), sem rede nem SDL.
#
#   bash tests/cotacat.sh
set -eu
cd "$(dirname "$0")/.."
flags=()
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
cc ${flags[@]+"${flags[@]}"} src/cotacat.c tests/cotacat.c \
  -Isrc -o ${TMPDIR:-/tmp}/nuvio-cotacat-tests -O1 -g \
  -Wall -Wno-deprecated-declarations -Wno-macro-redefined
${TMPDIR:-/tmp}/nuvio-cotacat-tests
