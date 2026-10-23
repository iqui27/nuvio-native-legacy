#!/bin/bash
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
# Leitores privados EBML/lacing; sem sockets, SDL ou arquivos de midia.
set -eu
cd "$(dirname "$0")/.."
DIR=$(mktemp -d ${TMPDIR:-/tmp}/nuvio-mkvass-limites.XXXXXX)
trap 'rm -rf "$DIR"' EXIT
flags=(-ffunction-sections -fdata-sections)
if [ "$(uname -s)" = Darwin ]; then flags+=(-Wl,-dead_strip); else flags+=(-Wl,--gc-sections -pthread -lm); fi
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=all); fi
. tests/mkvcore_flag.sh
cc tests/mkvass_limites.c -Isrc -O1 -g -Wall -Wextra -Wno-deprecated-declarations \
  "${flags[@]}" ${MKVCORE_CC[@]+"${MKVCORE_CC[@]}"} -o "$DIR/test"
"$DIR/test"
