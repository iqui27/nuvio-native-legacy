#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
task_dir="$(mktemp -d "${TMPDIR:-/tmp}/nuvio-backup-connections.XXXXXX")"
trap 'rm -rf "$task_dir"' EXIT
flags=(-std=gnu11 -O1 -g -Isrc -Wall -Wextra -Werror)
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
cc "${flags[@]}" src/backupcategories.c src/backupconnections.c src/backupstores.c src/backupcrypto.c tests/backupconnections.c -o "$task_dir/test"
"$task_dir/test"
