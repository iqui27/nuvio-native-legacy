#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
tmp=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-mkv-dts-pref779.XXXXXX")
trap 'rm -rf "$tmp"' EXIT
flags=(-std=c11 -O1 -Wall -Wextra -Werror)
if [ "$(uname -s)" = Darwin ]; then flags+=(-Wl,-dead_strip); else flags+=(-Wl,--gc-sections); fi
cc "${flags[@]}" tests/mkv_dts_pref779.c -o "$tmp/test"
"$tmp/test"
