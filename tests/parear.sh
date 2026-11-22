#!/bin/bash
# Regras do codigo de 6 digitos do login. Ver tests/parear.c.
#   bash tests/parear.sh
set -eu
cd "$(dirname "$0")/.."
tmp="$(mktemp -d "${TMPDIR:-/tmp}/nuvio-parear-XXXXXX")"
trap 'rm -rf "$tmp"' EXIT
flags=(-O1 -g -Isrc -Wall -Wextra -Werror)
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
cc "${flags[@]}" src/parear.c src/js.c tests/parear.c -o "$tmp/parear" -lm
"$tmp/parear"
