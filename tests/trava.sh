#!/bin/bash
# Trava de serie: regra do limite, gate de spoiler, frases, arquivo, servidor.
# Sem SDL, sem rede: trava.c + dados.c + js.c.
set -eu
cd "$(dirname "$0")/.."
flags=()
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
out="${TMPDIR:-/tmp}/nuvio-trava-tests"
cc ${flags[@]+"${flags[@]}"} src/trava.c src/dados.c src/js.c tests/trava.c \
  -Isrc -o "$out" -O1 -g -Wall -Wextra -Wno-unused-parameter \
  -Wno-deprecated-declarations -Wno-macro-redefined
"$out"
