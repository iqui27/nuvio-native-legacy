#!/bin/bash
set -eu
cd "$(dirname "$0")/.."
flags=(-O1 -g -Isrc -pthread -I/opt/homebrew/include -I/opt/homebrew/include/SDL2
       -Wall -Wno-deprecated-declarations -Wno-macro-redefined)
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
dir="$(mktemp -d)"
trap 'rm -rf "$dir"' EXIT
cc "${flags[@]}" tests/contaoffline_limites.c tests/stub_contapend.c src/catordem.c src/catordemcache.c \
  src/contacache.c src/js.c src/jsw.c -o "$dir/contaoffline_limites"
NV_T_DIR="$dir" "$dir/contaoffline_limites"
