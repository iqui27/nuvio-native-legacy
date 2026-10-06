#!/bin/bash
# Cofre do diario (diarionuvem.c), parte pura. Rede/sessao/ilha sao dubles.
set -eu
cd "$(dirname "$0")/.."
flags=()
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
out="${TMPDIR:-/tmp}/nuvio-diarionuvem-tests"
cc ${flags[@]+"${flags[@]}"} src/diarionuvem.c src/diario.c src/diarionomes.c src/js.c src/jsw.c tests/diarionuvem.c \
  -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 -o "$out" -O1 -g -Wall -Wextra \
  -Wno-unused-parameter -Wno-deprecated-declarations -DNV_REC_URL='""'
"$out"
