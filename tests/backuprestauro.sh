#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-restore.XXXXXX")
trap 'rm -rf "$work"' EXIT
flags=(-std=c11 -O1 -g -Wall -Wextra -Werror -Isrc -ffunction-sections -fdata-sections)
if [ "$(uname -s)" = Darwin ]; then flags+=(-I/opt/homebrew/include -I/opt/homebrew/include/SDL2 -Wl,-dead_strip);else read -r -a sdk_flags <<< "$(pkg-config --cflags sdl2)";flags+=("${sdk_flags[@]}" -Wl,--gc-sections);fi
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer);fi
"${CC:-cc}" "${flags[@]}" src/backupcrypto.c src/backupstores.c src/backupcategories.c src/backupconnections.c src/backupfonts.c src/backuplocal.c src/backupplano.c src/backuprestauro.c tests/backuprestauro.c -lz -o "$work/test"
"$work/test"
