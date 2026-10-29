#!/bin/bash
# Offline Kaptain URL parity and split-upcoming regression.
set -eu
cd "$(dirname "$0")/.."
export TMPDIR=/Volumes/ExternalSSD/tmp
out=$(mktemp -d "$TMPDIR/kaptain.XXXXXX")
trap 'rm -rf "$out"' EXIT
cc tests/kaptain.c src/colecoes.c src/js.c -Isrc \
  -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -O1 -ffunction-sections -fdata-sections -Wl,-dead_strip \
  -Wno-deprecated-declarations -o "$out/urls"
"$out/urls"
python3 tests/kaptain_split.py "$out"
