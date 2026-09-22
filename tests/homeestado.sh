#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT
${CC:-cc} -std=c11 -D_DEFAULT_SOURCE -D_XOPEN_SOURCE=700 -pthread -Isrc -I/opt/homebrew/include \
  src/homeestado.c tests/homeestado.c -o "$tmp/homeestado"
"$tmp/homeestado"
