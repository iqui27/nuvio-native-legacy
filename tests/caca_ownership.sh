#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
tmp=$(mktemp -d /tmp/nuvio-caca-ownership.XXXXXX)
trap 'rm -rf "$tmp"' EXIT
flags=()
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
cc "${flags[@]}" -O1 -g -Wall -Wextra -ffunction-sections -fdata-sections \
  -DNV_REC_URL='"https://worker.test"' tests/caca_ownership.c src/cacadados.c src/conquista.c src/js.c \
  -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 -Wl,-dead_strip -o "$tmp/test"
cd "$tmp"
./test
