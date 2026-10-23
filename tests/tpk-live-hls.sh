#!/bin/bash
# Offline regression against the real TPK backend, no provider requests.
set -euo pipefail
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-tpk-live.XXXXXX")
trap 'rm -rf "$work"' EXIT
flags=(-ffunction-sections -fdata-sections)
if [ "$(uname -s)" = Darwin ]; then flags+=(-Wl,-dead_strip); else flags+=(-Wl,--gc-sections); fi
cc -O1 -g -Wall -DNV_TPK -I/opt/homebrew/include -Isrc tests/tpk-live-hls.c tests/dts_tpk_stub.c src/mkvfix.c -DNV_MKVFIX_SEM_REDE src/velocidade.c src/faixasmkv.c src/audioinfo.c src/audioescolha.c \
  $(sdl2-config --cflags --libs) -lpthread "${flags[@]}" -o "$work/test"
"$work/test"
