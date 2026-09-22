#!/bin/bash
# Persistencia local da ordem remota entre update/boot, perfil e logout.
set -eu
cd "$(dirname "$0")/.."
flags=()
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
cc ${flags[@]+"${flags[@]}"} src/catordem.c src/catordemcache.c src/js.c tests/catordemcache.c \
  -Isrc -o /tmp/nuvio-catordemcache-tests -O1 -g \
  -Wall -Wno-deprecated-declarations -Wno-macro-redefined
/tmp/nuvio-catordemcache-tests
