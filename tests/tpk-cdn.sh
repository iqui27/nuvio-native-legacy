#!/bin/bash
# No TV: the .tpk player gets the TorBox CDN link (see the top of
# tests/tpk-cdn.c). Fake host, fake resolver; no network, no GL, no device.
set -eu
cd "$(dirname "$0")/.."
SDL="-I/opt/homebrew/include $(sdl2-config --cflags) $(sdl2-config --libs)"
cc -O1 -g -Wall -DNV_TPK -DNV_TEST_DIRECT_CDN -Isrc tests/tpk-cdn.c src/video_tpk.c src/mkvfix.c src/vazao.c src/velocidade.c src/faixasmkv.c src/audioinfo.c src/audioescolha.c src/mkv.c src/capmkv.c -lpthread $SDL -o "${TMPDIR:-/tmp}/nuvio-tpk-cdn"
"${TMPDIR:-/tmp}/nuvio-tpk-cdn"
