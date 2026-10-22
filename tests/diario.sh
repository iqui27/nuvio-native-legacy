#!/bin/bash
# O diario por perfil (diario.c). Sem SDL, sem rede: dados.c e perfis.c sao dubles.
set -eu
cd "$(dirname "$0")/.."
flags=()
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
out="${TMPDIR:-/tmp}/nuvio-diario-tests"
cc ${flags[@]+"${flags[@]}"} src/diario.c src/diarionomes.c tests/diario.c \
  -Isrc -o "$out" -O1 -g -Wall -Wextra -Wno-unused-parameter -Wno-deprecated-declarations
"$out"
