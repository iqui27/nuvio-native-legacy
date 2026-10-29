#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
tmp=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-bibprevia.XXXXXX")
trap 'rm -rf "$tmp"' EXIT
cc tests/biblioteca_previa.c -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -O1 -ffunction-sections -fdata-sections -Wl,-dead_strip -o "$tmp/test"
"$tmp/test"
