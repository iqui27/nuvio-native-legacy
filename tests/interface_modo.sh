#!/bin/bash
# Saved TV/Mobile policy, Auto, rotation and stale gestures. No window or network.
set -euo pipefail
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-interface-modo.XXXXXX")
trap 'rm -rf "$work"' EXIT
flags=(-std=c11 -O1 -g -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2)
if [ "${SANITIZE:-0}" = 1 ]; then
  flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer)
fi
cc "${flags[@]}" tests/interface_modo.c src/layout.c -lm -o "$work/tv"
"$work/tv"
cc "${flags[@]}" -DNV_TOUCH_UI tests/interface_modo.c src/layout.c -lm -o "$work/touch"
"$work/touch"
