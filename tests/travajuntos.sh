#!/bin/bash
# Trava Juntos (travajuntos.c) contra o Diario e a Trava de verdade. Sem SDL, sem rede.
set -eu
cd "$(dirname "$0")/.."
flags=()
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
out="${TMPDIR:-/tmp}/nuvio-travajuntos-tests"
cc ${flags[@]+"${flags[@]}"} src/travajuntos.c src/trava.c src/diario.c src/diarionomes.c src/js.c tests/travajuntos.c \
  -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 -o "$out" -O1 -g -Wall -Wextra -Wno-unused-parameter -Wno-deprecated-declarations
"$out"
