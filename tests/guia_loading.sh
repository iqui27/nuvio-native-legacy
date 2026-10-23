#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-guia-loading.XXXXXX")
trap 'rm -rf "$work"' EXIT
flags=(-ffunction-sections -fdata-sections)
if [ "$(uname -s)" = Darwin ]; then flags+=(-Wl,-dead_strip); else flags+=(-Wl,--gc-sections); fi
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
cc tests/guia_loading.c src/js.c src/idcanal.c src/ltv.c src/guiafiltro.c -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 -O1 -g "${flags[@]}" -lpthread -lm -o "$work/test"
"$work/test"
