#!/bin/bash
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
set -euo pipefail
cd "$(dirname "$0")/.."
work=$(mktemp -d ${TMPDIR:-/tmp}/nuvio-cncverse-live.XXXXXX)
trap 'rm -rf "$work"' EXIT
flags=(-ffunction-sections -fdata-sections)
if [ "$(uname -s)" = Darwin ]; then flags+=(-Wl,-dead_strip); else flags+=(-Wl,--gc-sections); fi
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
cc tests/cncverse_live.c src/ltv.c src/js.c src/idcanal.c src/guiafiltro.c src/stream_parse.c src/fontefiltro.c src/linguas.c src/badges.c -Isrc \
  -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -O1 -g -Wno-macro-redefined "${flags[@]}" -o "$work/test"
"$work/test"
