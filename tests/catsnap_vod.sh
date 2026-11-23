#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-catsnap-vod.XXXXXX")
trap 'rm -rf "$work"' EXIT
flags=(-ffunction-sections -fdata-sections)
if [ "$(uname -s)" = Darwin ]; then
  flags+=(-Wl,-dead_strip -I/opt/homebrew/include/SDL2)
else
  flags+=(-Wl,--gc-sections -I/usr/include/SDL2)
fi
cc -std=c11 -D_POSIX_C_SOURCE=200809L -D_DEFAULT_SOURCE -O0 -g -Isrc "${flags[@]}" \
  tests/catsnap_vod.c src/guiav5.c src/js.c src/jsw.c -lm -o "$work/test"
"$work/test" "$work"
