#!/bin/bash
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
# Attached fonts kept per file and the bounded wait before a big first load (log 12). See tests/mkvass_fontes_memoria.c.
set -eu
cd "$(dirname "$0")/.."
DIR=$(mktemp -d ${TMPDIR:-/tmp}/nuvio-mkvass-fontesmem.XXXXXX)
trap 'rm -rf "$DIR"' EXIT
flags=(-ffunction-sections -fdata-sections)
if [ "$(uname -s)" = Darwin ]; then flags+=(-Wl,-dead_strip); else flags+=(-Wl,--gc-sections -pthread -lm); fi
. tests/mkvcore_flag.sh
cc tests/mkvass_fontes_memoria.c src/fontefamilias.c -Isrc -O1 -g -Wall -Wextra -Wno-deprecated-declarations -Wno-unused-function \
  "${flags[@]}" ${MKVCORE_CC[@]+"${MKVCORE_CC[@]}"} -o "$DIR/test"
"$DIR/test"
