#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
dir=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-legtemp.XXXXXX")
trap 'rm -rf "$dir"' EXIT
flags=(-std=gnu11 -O1 -g -Wall -Wextra -Werror -Isrc)
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
"${CC:-cc}" "${flags[@]}" tests/legtemp.c src/legtemp.c -o "$dir/teste" -lm
"$dir/teste"
