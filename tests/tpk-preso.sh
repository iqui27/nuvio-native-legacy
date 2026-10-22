#!/bin/bash
set -eu
cd "$(dirname "$0")/.."
work=$(mktemp -d /tmp/nuvio-tpk-preso.XXXXXX)
trap 'rm -rf "$work"' EXIT
flags=(-ffunction-sections -fdata-sections)
if [ "$(uname -s)" = Darwin ]; then flags+=(-Wl,-dead_strip); else flags+=(-Wl,--gc-sections); fi
cc -O1 -g -Wall -DNV_TPK -I/opt/homebrew/include -Isrc tests/tpk-preso.c src/velocidade.c src/faixasmkv.c src/audioinfo.c \
  $(sdl2-config --cflags --libs) -lpthread "${flags[@]}" -o "$work/test"
"$work/test"
