#!/bin/bash
set -eu
cd "$(dirname "$0")/.."
export TMPDIR=/Volumes/ExternalSSD/tmp/jobtmp
mkdir -p "$TMPDIR"
bin=$(mktemp "$TMPDIR/homevazio.XXXXXX")
trap 'rm -f "$bin"' EXIT
cc src/homevazio.c src/qr.c tests/homevazio.c -Isrc -o "$bin" -O1 \
  -ffunction-sections -fdata-sections -Wl,-dead_strip \
  -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -Wno-deprecated-declarations -Wno-macro-redefined
"$bin"
