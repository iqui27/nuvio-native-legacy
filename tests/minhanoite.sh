#!/bin/bash
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
# Minha noite: grade deterministica do canal pessoal, zapping e a regra do
# gesto. Regra pura, sem SDL: roda igual no macOS e no Linux.
# Ver tests/minhanoite.c.
#
#   bash tests/minhanoite.sh
set -eu
cd "$(dirname "$0")/.."
flags=()
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
cc ${flags[@]+"${flags[@]}"} ${NUVIO_CFLAGS:-} src/minhanoite.c tests/minhanoite.c \
  -Isrc -o "$TMPDIR/nuvio-minhanoite-tests" -O1 -g -Wall -Wextra
"$TMPDIR/nuvio-minhanoite-tests"
