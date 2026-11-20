#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-idle.XXXXXX")
trap 'rm -rf "$work"' EXIT
flags=()
if [ "${SANITIZE:-0}" = 1 ]; then
  flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=all)
fi
cc ${flags[@]+"${flags[@]}"} -DESMAECER_SEM_GFX -Isrc -O1 -g -Wall -Wextra \
  tests/legenda_idle.c src/legenda.c src/assrender.c src/esmaecer.c \
  -pthread -lm -o "$work/test"
"$work/test"
