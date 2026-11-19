#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
cc tests/importar.c src/importar.c src/js.c -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 -O1 -Wall -ffunction-sections -fdata-sections -Wl,-dead_strip -lpthread -o "$tmp/test"
"$tmp/test"
