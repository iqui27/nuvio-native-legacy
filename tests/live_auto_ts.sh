#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
flags=(-O1 -g -ffunction-sections -fdata-sections -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2)
if [ "$(uname -s)" = Darwin ]; then flags+=(-Wl,-dead_strip); else flags+=(-Wl,--gc-sections); fi
cc "${flags[@]}" tests/live_auto_ts.c -pthread -o "$work/probe"
"$work/probe"
bash tests/guia_stream_headers.sh
