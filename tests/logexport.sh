#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
task_tmp=$(mktemp -d)
trap 'rm -rf "$task_tmp"' EXIT
flags=(-std=c11 -O1 -g -Wall -Wextra -Werror -Isrc)
if [[ ${SANITIZE:-0} == 1 ]]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
cc "${flags[@]}" src/logexport.c tests/logexport.c -o "$task_tmp/test"
"$task_tmp/test"
