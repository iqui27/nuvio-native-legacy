#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
task_dir=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-drive-credential.XXXXXX")
trap 'rm -rf "$task_dir"' EXIT
flags=(-std=gnu11 -O1 -g -Wall -Wextra -Werror -Isrc)
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
cc "${flags[@]}" src/drivevinculo.c src/backupcrypto.c src/js.c src/jsw.c tests/drivevinculo.c -lm -o "$task_dir/test"
"$task_dir/test" "$task_dir"
