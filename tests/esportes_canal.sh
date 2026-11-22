#!/bin/bash
# Esporte casado com o canal: a regra pura (src/esportes_canal.c).
set -eu
cd "$(dirname "$0")/.."
flags=()
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
out=$(mktemp "${TMPDIR:-/tmp}/nuvio-esportes-canal.XXXXXX")
trap 'rm -f "$out"' EXIT
cc ${flags[@]+"${flags[@]}"} -std=gnu11 -Isrc -O1 -g -Wall -Wextra -Werror \
  src/esportes_canal.c src/esportes_parse.c src/js.c src/buscanorm.c tests/esportes_canal.c -lm -o "$out"
"$out"
