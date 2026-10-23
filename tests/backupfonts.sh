#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
task_dir="$(mktemp -d "${TMPDIR:-/tmp}/nuvio-backup-fonts.XXXXXX")"
trap 'rm -rf "$task_dir"' EXIT
flags=(-Dlinkat=backup_test_linkat -std=gnu11 -O1 -g -Isrc -Wall -Wextra -Werror)
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
cc "${flags[@]}" src/backupfonts.c tests/backupfonts.c -o "$task_dir/test"
mkdir "$task_dir/data"
"$task_dir/test" "$task_dir/data"
