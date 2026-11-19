#!/bin/bash
set -eu
cd "$(dirname "$0")/.."
flags=()
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
out=$(mktemp "${TMPDIR:-/tmp}/nuvio-esportes.XXXXXX")
trap 'rm -f "$out"' EXIT
cc ${flags[@]+"${flags[@]}"} -std=gnu11 -Isrc -O1 -g -Wall -Wextra -Werror \
  src/esportes_parse.c src/js.c src/buscanorm.c tests/esportes.c -lm -o "$out"
"$out"
