#!/bin/bash
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
# Stalker (src/stalker.c): normalizacao do portal e varredura de rotas (#237),
# com rede e disco dublados. Ver tests/stalker_portal.c.
set -eu
cd "$(dirname "$0")/.."
flags=()
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
cc ${flags[@]+"${flags[@]}"} ${NUVIO_CFLAGS:-} src/stalker.c src/js.c tests/stalker_portal.c \
  -Isrc -o ${TMPDIR:-/tmp}/nuvio-stalker-portal -O1 -g -Wall -Wextra -lpthread \
  -Wno-deprecated-declarations -Wno-macro-redefined
${TMPDIR:-/tmp}/nuvio-stalker-portal
