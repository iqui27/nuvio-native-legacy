#!/bin/bash
set -eu
cd "$(dirname "$0")/.."
flags=(-ffunction-sections -fdata-sections)
if [ "$(uname -s)" = Darwin ]; then flags+=(-Wl,-dead_strip); else flags+=(-Wl,--gc-sections); fi
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
out=$(mktemp "${TMPDIR:-/tmp}/nuvio-esportes-ordem.XXXXXX")
trap 'rm -f "$out"' EXIT
cc "${flags[@]}" -std=gnu11 -Isrc -O1 -g -Wall -Wextra -Wno-unused-function tests/esportes_ordem.c -o "$out"
"$out"
