#!/bin/bash
# Diagnostics QR card lifecycle, headless (see tests/celbotao_log.c).
set -euo pipefail
cd "$(dirname "$0")/.."
t=$(mktemp -d "${TMPDIR:-/tmp}/celbotao-log.XXXXXX"); trap 'rm -rf "$t"' EXIT
cc -std=gnu11 -w -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 -DNV_REC_URL='""' \
  tests/celbotao_log.c tests/celbotao_log_stubs.c src/celular.c src/logexport.c src/logcloud.c src/qr.c \
  -o "$t/test" -lpthread -lz
"$t/test"
