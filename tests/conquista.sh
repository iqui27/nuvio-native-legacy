#!/bin/bash
# Motor das conquistas (degraus, "Em ordem", desbloqueio, aviso, privacidade).
# Sem SDL, sem rede: so conquista.c.
set -eu
cd "$(dirname "$0")/.."
flags=()
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
out="${TMPDIR:-/tmp}/nuvio-conquista-tests"
cc ${flags[@]+"${flags[@]}"} src/conquista.c tests/conquista.c \
  -Isrc -o "$out" -O1 -g -Wall -Wextra
"$out"
