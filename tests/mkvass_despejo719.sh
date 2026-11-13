#!/bin/bash
set -eu
cd "$(dirname "$0")/.."
DIR=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-mkvass-719.XXXXXX")
trap 'rm -rf "$DIR"' EXIT
flags=(-ffunction-sections -fdata-sections)
if [ "$(uname -s)" = Darwin ]; then flags+=(-Wl,-dead_strip); else flags+=(-Wl,--gc-sections -pthread); fi
cc tests/mkvass_despejo719.c -Isrc -O1 -g -Wall -Wextra -Wno-deprecated-declarations "${flags[@]}" -lm -o "$DIR/test"
"$DIR/test"
