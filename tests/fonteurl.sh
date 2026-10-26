#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
task_dir=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-font-url.XXXXXX")
trap 'rm -rf "$task_dir"' EXIT
flags=(-std=gnu11 -O1 -g -Wall -Wextra -Werror -Isrc -pthread)
if [ "$(uname -s)" != Darwin ]; then flags+=(-ldl);fi
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer);fi
cc "${flags[@]}" -Wno-error -c src/rede.c -o "$task_dir/rede.o"
cc "${flags[@]}" -Wno-error -c src/redeurl.c -o "$task_dir/redeurl.o"
cc "${flags[@]}" $(pkg-config --cflags sdl2) tests/fonteurl.c "$task_dir/rede.o" "$task_dir/redeurl.o" -lz -lm $(pkg-config --libs sdl2) -o "$task_dir/test"
"$task_dir/test" "$task_dir"
