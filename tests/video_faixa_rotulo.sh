#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
tmp=$(mktemp -d);trap 'rm -rf "$tmp"' EXIT
flags=(-std=c11 -O1 -g -Wall -Wextra -Werror -Isrc)
if [[ ${SANITIZE:-0} == 1 ]];then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer);fi
cc "${flags[@]}" tests/video_faixa_rotulo.c -o "$tmp/test"
"$tmp/test"
