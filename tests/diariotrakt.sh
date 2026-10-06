#!/bin/bash
# Trakt -> Diario (diariotrakt.c), so a leitura. Sem rede.
set -eu
cd "$(dirname "$0")/.."
flags=()
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
out="${TMPDIR:-/tmp}/nuvio-diariotrakt-tests"
cc ${flags[@]+"${flags[@]}"} src/diariotrakt.c src/diario.c src/diarionomes.c src/js.c tests/diariotrakt.c \
  -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 -o "$out" -O1 -g -Wall -Wextra \
  -Wno-unused-parameter -Wno-deprecated-declarations
"$out"
