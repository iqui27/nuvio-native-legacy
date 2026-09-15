#!/bin/bash
# Guia de TV: casamento nome-do-addon -> canal da grade XMLTV, e as consultas
# "agora"/"a seguir". Ver tests/guiaepg.c.
#
#   bash tests/guiaepg.sh
set -eu
cd "$(dirname "$0")/.."
flags=()
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
cc ${flags[@]+"${flags[@]}"} ${NUVIO_CFLAGS:-} src/epg.c tests/guiaepg.c \
  -Isrc -o /tmp/nuvio-guiaepg-tests -O1 -g -lz \
  -Wall -Wno-deprecated-declarations -Wno-macro-redefined
/tmp/nuvio-guiaepg-tests
