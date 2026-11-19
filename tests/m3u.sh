#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-m3u.XXXXXX")
trap 'rm -rf "$work"' EXIT
flags=(-std=c11 -D_POSIX_C_SOURCE=200809L -O1 -g -Wall -Wextra -Werror -Isrc)
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
cc "${flags[@]}" tests/m3u_parse.c src/m3u_parse.c -o "$work/parser"
"$work/parser"
cc "${flags[@]}" tests/m3u_backend.c src/m3u.c src/m3u_parse.c -lpthread -o "$work/backend"
"$work/backend" "$work/data"
