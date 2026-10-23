#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
work=$(mktemp -d /tmp/nuvio-pr536.XXXXXX)
trap 'rm -rf "$work"' EXIT
flags=(-Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2
  -ffunction-sections -fdata-sections -Wl,-dead_strip -Wno-deprecated-declarations)
cc "${flags[@]}" -DTEST_SHEET tests/pr536_fixes.c -o "$work/sheet"
cc "${flags[@]}" -DNV_ASS_LIBASS tests/pr536_fixes.c \
  $(pkg-config --cflags --libs libass) -o "$work/shadow"
rc=0
"$work/sheet" || rc=1
"$work/shadow" || rc=1
exit "$rc"
