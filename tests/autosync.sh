#!/bin/bash
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
set -euo pipefail
cd "$(dirname "$0")/.."
AS_DIR=$(mktemp -d ${TMPDIR:-/tmp}/nuvio-autosync.XXXXXX)
trap 'rm -rf "$AS_DIR"' EXIT
flags=()
if [ "${SANITIZE:-0}" = 1 ]; then
  flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=all)
elif [ "${SANITIZE:-0}" = thread ]; then
  flags+=(-fsanitize=thread -fno-omit-frame-pointer)
fi
cc "${flags[@]}" -Isrc -O2 -g -Wall -Wextra src/autosync.c src/legenda.c \
  src/assrender.c tests/autosync.c -pthread -lm -o "$AS_DIR/test"
"$AS_DIR/test"
