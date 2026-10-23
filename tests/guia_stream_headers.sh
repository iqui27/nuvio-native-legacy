#!/bin/bash
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
set -euo pipefail
cd "$(dirname "$0")/.."
work=$(mktemp -d ${TMPDIR:-/tmp}/nuvio-guia-stream-headers.XXXXXX)
trap 'rm -rf "$work"' EXIT
flags=(-ffunction-sections -fdata-sections)
if [ "$(uname -s)" = Darwin ]; then flags+=(-Wl,-dead_strip); else flags+=(-Wl,--gc-sections); fi
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
cc tests/guia_stream_headers.c src/stream_parse.c src/fontefiltro.c src/linguas.c src/js.c src/badges.c src/proxyts.c \
  -DNV_ANDROID -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -O1 -g -Wno-macro-redefined "${flags[@]}" -lm -o "$work/test"
"$work/test"
