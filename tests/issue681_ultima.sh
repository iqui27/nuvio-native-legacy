#!/bin/bash
# Issue #681: the last subtitle row must be reachable/selectable. tests/issue681_ultima.c.
set -euo pipefail
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-issue681.XXXXXX")
trap 'rm -rf "$work"' EXIT
flags=()
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=all); fi
cc ${flags[@]+"${flags[@]}"} tests/issue681_ultima.c src/linguas.c src/bidi.c src/legenda.c src/assrender.c src/assperf.c src/assfios.c -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -std=gnu11 -g -ffunction-sections -fdata-sections -Wl,-dead_strip \
  -Wall -Wextra -Wno-unused-function -Wno-macro-redefined -o "$work/t"
"$work/t"
