#!/bin/bash
# Publicacao ASS concorrente ao quadro (#412), sem rede ou TV.
set -eu
cd "$(dirname "$0")/.."
DIR=$(mktemp -d /tmp/nuvio-mkvass-quadro.XXXXXX)
trap 'rm -rf "$DIR"' EXIT
flags=(-ffunction-sections -fdata-sections)
if [ "$(uname -s)" = Darwin ]; then flags+=(-Wl,-dead_strip); else flags+=(-Wl,--gc-sections -pthread -lm); fi
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=all); fi
cc tests/mkvass_quadro.c -Isrc -O1 -g -Wall -Wextra -Wno-deprecated-declarations \
  "${flags[@]}" -o "$DIR/test"
"$DIR/test"
