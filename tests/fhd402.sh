#!/bin/bash
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
# #402: fontes "FHD" / "Full HD" sem os digitos 1080 no grupo 1080p.
set -euo pipefail
cd "$(dirname "$0")/.."
DIR=$(mktemp -d ${TMPDIR:-/tmp}/nuvio-fhd402.XXXXXX)
trap 'rm -rf "$DIR"' EXIT
flags=(-ffunction-sections -fdata-sections)
if [ "$(uname -s)" = Darwin ]; then flags+=(-Wl,-dead_strip); else flags+=(-Wl,--gc-sections); fi
cc src/stream_parse.c src/fontefiltro.c src/linguas.c src/js.c src/badges.c tests/fhd402.c -Isrc \
  -I/opt/homebrew/include -O1 -g -Wall -Wextra -Wno-macro-redefined \
  "${flags[@]}" -o "$DIR/test"
"$DIR/test"
