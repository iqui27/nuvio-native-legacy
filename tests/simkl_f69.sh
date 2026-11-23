#!/bin/bash
set -eu
cd "$(dirname "$0")/.."
flags=()
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
out="${TMPDIR:-/tmp}/nuvio-simkl-f69"
cc ${flags[@]+"${flags[@]}"} tests/simkl_f69.c src/simklbib.c src/js.c src/traktscrobble.c src/metaep.c \
  -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 -O1 -g -lpthread -o "$out"
for scenario in backoff disconnect memory; do "$out" "$scenario"; done
