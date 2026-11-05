#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
d=$(mktemp -d);trap 'rm -rf "$d"' EXIT
flags=(-std=c11 -O1 -g -Wall -Wextra -Werror -Isrc)
if [[ ${SANITIZE:-0} == 1 ]];then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer);fi
cc "${flags[@]}" tests/video_packet_diag.c -o "$d/test"
"$d/test"
