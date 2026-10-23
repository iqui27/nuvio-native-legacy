#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-epg-memory.XXXXXX")
trap 'rm -rf "$work"' EXIT
flags=(-std=c11 -D_POSIX_C_SOURCE=200809L -ffunction-sections -fdata-sections -O1 -g)
if [ "$(uname -s)" = Darwin ]; then flags+=(-Wl,-dead_strip); else flags+=(-Wl,--gc-sections); fi
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
cc tests/epg_memory.c src/xmltv.c -Isrc "${flags[@]}" -lpthread -lz -o "$work/test"
"$work/test" "$work/data"
