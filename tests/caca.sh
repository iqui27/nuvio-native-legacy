#!/bin/bash
# Caca a filmografia, parte pura: classe, escopo, datas, leitores de JSON e o
# progresso sobre uma filmografia lida. Sem SDL, sem rede.
set -eu
cd "$(dirname "$0")/.."
flags=()
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
out="${TMPDIR:-/tmp}/nuvio-caca-tests"
cc ${flags[@]+"${flags[@]}"} src/cacadados.c src/conquista.c src/js.c tests/caca.c \
  -Isrc -o "$out" -O1 -g -Wall -Wextra
"$out"
