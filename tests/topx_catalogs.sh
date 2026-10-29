#!/bin/bash
set -eu
cd "$(dirname "$0")/.."
flags=(-std=gnu11 -Isrc -O1 -g -ffunction-sections -fdata-sections -Wno-unused-function)
if [ "$(uname -s)" = Darwin ]; then flags+=(-Wl,-dead_strip); else flags+=(-Wl,--gc-sections); fi
read -r -a sdk <<< "$(pkg-config --cflags sdl2)"
out=$(mktemp "${TMPDIR:-/tmp}/nuvio-topx-catalogs.XXXXXX")
trap 'rm -f "$out"' EXIT
cc "${flags[@]}" "${sdk[@]}" tests/topx_catalogs.c src/js.c src/cotacat.c -pthread -lm -o "$out"
"$out"
