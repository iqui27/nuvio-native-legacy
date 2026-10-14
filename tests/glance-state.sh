#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
export TMPDIR=/Volumes/ExternalSSD/tmp/jobtmp
mkdir -p "$TMPDIR"
tmp="$(mktemp -d "$TMPDIR/glance-state-XXXXXX")"
trap 'rm -rf "$tmp"' EXIT
cc -O1 -g -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -Wno-deprecated-declarations -ffunction-sections -fdata-sections \
  tests/glance-state.c src/trailerfonte.c -Wl,-dead_strip -o "$tmp/test"
"$tmp/test"
