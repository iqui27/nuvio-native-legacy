#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
flags=(-ffunction-sections -fdata-sections)
if [ "$(uname -s)" = Darwin ]; then flags+=(-Wl,-dead_strip); else flags+=(-Wl,--gc-sections); fi
cc -Wall -Wextra "${flags[@]}" tests/livetv_auto_res.c -Isrc -I/opt/homebrew/include -o "$work/test" -lpthread
"$work/test"
