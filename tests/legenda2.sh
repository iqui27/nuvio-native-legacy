#!/bin/bash
# Second subtitle session (src/legenda2.c). See tests/legenda2.c.
#   SANITIZE=1 (ASan/UBSan) or SANITIZE=thread (TSan) for the loader.
set -euo pipefail
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-legenda2.XXXXXX")
trap 'rm -rf "$work"' EXIT
flags=()
if [ "${SANITIZE:-0}" = 1 ]; then
  flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=all)
elif [ "${SANITIZE:-0}" = thread ]; then
  flags+=(-fsanitize=thread -fno-omit-frame-pointer)
fi
cc ${flags[@]+"${flags[@]}"} -Isrc -O1 -g -Wall -Wextra src/legenda2.c src/legenda.c src/asssinais.c src/assrender.c \
  tests/legenda2.c -pthread -lm -o "$work/test"
"$work/test"
