#!/bin/bash
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
# Canal ao vivo: ordem do zapping, debounce e escolha de agora/a seguir.
# Ver tests/aovivo.c.
#
#   bash tests/aovivo.sh
set -eu
cd "$(dirname "$0")/.."
flags=()
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
cc ${flags[@]+"${flags[@]}"} ${NUVIO_CFLAGS:-} src/aovivo.c src/epg.c src/xmltv.c tests/aovivo.c \
  -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 -o ${TMPDIR:-/tmp}/nuvio-aovivo-tests -O1 -g -lz \
  -L/opt/homebrew/lib -lSDL2 \
  -Wall -Wno-deprecated-declarations -Wno-macro-redefined
${TMPDIR:-/tmp}/nuvio-aovivo-tests
