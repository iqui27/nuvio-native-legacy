#!/bin/bash
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
# Parser sem inicializar SDL; os desenhos de badges sao descartados no link.
set -eu
cd "$(dirname "$0")/.."
DIR=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-stream-parser.XXXXXX")
trap 'rm -rf "$DIR"' EXIT
flags=(-ffunction-sections -fdata-sections)
if [ "$(uname -s)" = Darwin ]; then flags+=(-Wl,-dead_strip); else flags+=(-Wl,--gc-sections); fi
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined,float-cast-overflow -fno-omit-frame-pointer -fno-sanitize-recover=all); fi
cc src/stream_parse.c src/fontefiltro.c src/linguas.c src/js.c src/badges.c tests/stream_parser.c -Isrc \
  -I/opt/homebrew/include -O1 -g -Wall -Wextra -Wno-macro-redefined \
  "${flags[@]}" -o "$DIR/test"
"$DIR/test"
