#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
tmp=$(mktemp -d);trap 'rm -rf "$tmp"' EXIT
flags=(-std=gnu11 -Wall -Wextra -Werror -O1 -g -Isrc)
if [[ ${SANITIZE:-0} == 1 ]];then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer);fi
cc "${flags[@]}" tests/mkv_dts.c -o "$tmp/test"
"$tmp/test"
