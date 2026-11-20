#!/bin/bash
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
# js_cadeia (src/js.c): elemento de array JSON com \uXXXX vira UTF-8 — o
# "genre" dos catalogos de canal no guia. Ver tests/jscadeia.c.
set -eu
cd "$(dirname "$0")/.."
flags=()
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
cc ${flags[@]+"${flags[@]}"} ${NUVIO_CFLAGS:-} src/js.c tests/jscadeia.c \
  -Isrc -o ${TMPDIR:-/tmp}/nuvio-jscadeia-tests -O1 -g -Wall -Wextra
${TMPDIR:-/tmp}/nuvio-jscadeia-tests
