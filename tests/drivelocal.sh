#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-drive-local-test.XXXXXX")
trap 'rm -rf "$work"' EXIT
flags=(-DNV_BACKUP_MBEDTLS -std=gnu11 -O1 -g -Wall -Wextra -Werror -Isrc -pthread)
libs=(-lmbedcrypto)
if [ -n "${MBEDTLS_INCLUDE:-}" ]; then flags+=("-I$MBEDTLS_INCLUDE"); fi
if [ -n "${MBEDTLS_LIBRARY:-}" ]; then libs=("$MBEDTLS_LIBRARY"); fi
if [ "$(uname -s)" = Darwin ]; then
  prefix="${MBEDTLS_PREFIX:-$(brew --prefix mbedtls@3)}";flags+=("-I$prefix/include" "-L$prefix/lib")
fi
if [ "$(uname -s)" != Darwin ]; then flags+=(-ldl);fi
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer);fi
# Keep warnings fatal for the new protocol code. Existing transport sources
# have GCC 14 format-truncation/indentation warnings; retain those diagnostics
# without changing unrelated network code in a backup feature.
"${CC:-cc}" "${flags[@]}" -Wno-error -c src/rede.c -o "$work/rede.o"
"${CC:-cc}" "${flags[@]}" -Wno-error -c src/redeurl.c -o "$work/redeurl.o"
"${CC:-cc}" "${flags[@]}" src/backupcrypto.c src/backupfonts.c src/backuplocal.c src/drivelocal.c src/js.c src/jsw.c tests/drivelocal.c "$work/rede.o" "$work/redeurl.o" "${libs[@]}" -lz -lm -o "$work/test"
"$work/test"
