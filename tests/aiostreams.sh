#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-aio.XXXXXX")
trap 'rm -rf "$work"' EXIT
if [ ! -f src/aiostreams.h ]; then echo 'FAIL: AIOStreams contract missing'; exit 1; fi
cc tests/aiostreams.c src/aiostreams.c src/js.c -Isrc -o "$work/test" -O1 -g \
  -ffunction-sections -fdata-sections -Wl,-dead_strip \
  -I/opt/homebrew/include -I/opt/homebrew/include/SDL2
"$work/test"
