#!/bin/bash
# Pure policy/config checks; no SDL, account, network or player dependencies.
set -eu
cd "$(dirname "$0")/.."
tmp=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-fontefalha.XXXXXX")
trap 'rm -rf "$tmp"' EXIT
mkdir "$tmp/data"
flags=(-std=c11 -O1 -g -Wall -Wextra -Werror -Isrc)
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
"${CC:-cc}" "${flags[@]}" src/fontefalha.c src/fontefiltro.c src/linguas.c tests/fontefalha.c -o "$tmp/test"
"$tmp/test" "$tmp/data"
