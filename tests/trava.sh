#!/bin/bash
# Trava de serie (trava.c) contra o Diario de verdade. Sem SDL, sem rede.
set -eu
cd "$(dirname "$0")/.."
flags=()
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
out="${TMPDIR:-/tmp}/nuvio-trava-tests"
cc ${flags[@]+"${flags[@]}"} src/trava.c src/diario.c src/diarionomes.c tests/trava.c \
  -Isrc -o "$out" -O1 -g -Wall -Wextra -Wno-unused-parameter -Wno-deprecated-declarations
"$out"
