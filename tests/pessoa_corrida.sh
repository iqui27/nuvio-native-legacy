#!/bin/bash
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
# #205: latest actor wins; failed resolution/fetch can be retried.
set -euo pipefail
cd "$(dirname "$0")/.."
flags=()
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
cc ${flags[@]+"${flags[@]}"} tests/pessoa_corrida.c src/js.c src/filmomodelo.c src/conquistasregra.c \
  -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -o "${TMPDIR}/nuvio-pessoa-corrida-tests" -O1 -g \
  -Wall -Wno-deprecated-declarations -Wno-macro-redefined
"${TMPDIR}/nuvio-pessoa-corrida-tests" | grep -F '#205'
