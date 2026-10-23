#!/bin/bash
# Personal-server notification accounting; fake server, no network.
set -euo pipefail
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-servidores-origem.XXXXXX")
trap 'rm -rf "$work"' EXIT
flags=(-std=gnu11 -O1 -g -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 -pthread -ffunction-sections -fdata-sections -Wno-macro-redefined)
if [ "$(uname -s)" = Darwin ]; then flags+=(-Wl,-dead_strip); else flags+=(-Wl,--gc-sections); fi
cc "${flags[@]}" tests/servidores_origem.c src/js.c -o "$work/test"
"$work/test"
