#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
task_dir="$(mktemp -d "${TMPDIR:-/tmp}/nuvio-drive-fonts.XXXXXX")"
trap 'rm -rf "$task_dir"' EXIT
flags=(-std=gnu11 -O1 -g -Isrc -DNV_BACKUP_MBEDTLS -Wall -Wextra -Werror -pthread)
libs=(-lmbedcrypto)
if [ -n "${MBEDTLS_INCLUDE:-}" ]; then flags+=("-I$MBEDTLS_INCLUDE"); fi
if [ -n "${MBEDTLS_LIBRARY:-}" ]; then libs=("$MBEDTLS_LIBRARY"); fi
if [[ "$(uname -s)" == Darwin ]]; then
  prefix="${MBEDTLS_PREFIX:-$(brew --prefix mbedtls@3)}";flags+=("-I$prefix/include" "-L$prefix/lib")
else libs+=(-ldl);fi
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
cc "${flags[@]}" -Wno-error -c src/rede.c -o "$task_dir/rede.o"
cc "${flags[@]}" -Wno-error -c src/redeurl.c -o "$task_dir/redeurl.o"
cc "${flags[@]}" src/drivefontes.c src/drivelocal.c src/backupstores.c src/backupfonts.c src/backuplocal.c src/backupcrypto.c src/js.c src/jsw.c tests/drivefontes.c "$task_dir/rede.o" "$task_dir/redeurl.o" "${libs[@]}" -lz -lm -o "$task_dir/test"
mkdir "$task_dir/data"
"$task_dir/test" "$task_dir/data" "${NV_TEST_FONT:-deploy/app/fonts/InterDisplay-Regular.ttf}"
