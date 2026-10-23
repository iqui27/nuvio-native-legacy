#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-sonda-cache.XXXXXX")
trap 'rm -rf "$work"' EXIT
cc -O1 -g -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -ffunction-sections -fdata-sections -Wl,-dead_strip \
  -Wno-deprecated-declarations -Wno-macro-redefined \
  tests/addons_sonda_cache.c src/js.c src/watchstate.c -lpthread -o "$work/test"
"$work/test"
