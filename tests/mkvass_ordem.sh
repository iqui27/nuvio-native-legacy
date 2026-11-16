#!/bin/bash
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
# Fetch order after a jump (src/mkvass.c proximoPendente). See tests/mkvass_ordem.c.
set -eu
cd "$(dirname "$0")/.."
DIR=$(mktemp -d ${TMPDIR:-/tmp}/nuvio-mkvass-ordem.XXXXXX)
trap 'rm -rf "$DIR"' EXIT
flags=(-ffunction-sections -fdata-sections)
if [ "$(uname -s)" = Darwin ]; then flags+=(-Wl,-dead_strip); else flags+=(-Wl,--gc-sections -pthread -lm); fi
. tests/mkvcore_flag.sh
cc tests/mkvass_ordem.c src/fontefamilias.c -Isrc -O1 -g -Wall -Wextra -Wno-deprecated-declarations -Wno-unused-function \
  "${flags[@]}" ${MKVCORE_CC[@]+"${MKVCORE_CC[@]}"} -o "$DIR/test"
"$DIR/test"
