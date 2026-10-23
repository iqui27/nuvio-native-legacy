#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-drive-tv-test.XXXXXX")
trap 'rm -rf "$work"' EXIT
flags=(-std=gnu11 -O1 -g -Wall -Wextra -Werror -Isrc -pthread)
if [ "$(uname -s)" != Darwin ]; then flags+=(-ldl);fi
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer);fi
"${CC:-cc}" "${flags[@]}" -Wno-error -c src/rede.c -o "$work/rede.o"
"${CC:-cc}" "${flags[@]}" -Wno-error -c src/redeurl.c -o "$work/redeurl.o"
"${CC:-cc}" "${flags[@]}" src/drivetv.c src/js.c tests/drivetv.c "$work/rede.o" "$work/redeurl.o" -lz -lm -o "$work/test"
"$work/test"
