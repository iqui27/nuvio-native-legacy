#!/bin/bash
# No TV: the converted stream reopening on seek and reconnect (see the top of
# tests/tpk-dts-reabrir.c). Fake host, fake converter, hand-built MKV
# header; no network, no GL, no device.
set -eu
cd "$(dirname "$0")/.."
SDL="-I/opt/homebrew/include $(sdl2-config --cflags) $(sdl2-config --libs)"
cc -O1 -g -Wall -DNV_TPK -Isrc tests/tpk-dts-reabrir.c src/video_tpk.c src/mkvfix.c -DNV_MKVFIX_SEM_REDE src/vazao.c src/velocidade.c src/faixasmkv.c src/audioinfo.c src/audioescolha.c src/mkv.c src/capmkv.c -lpthread $SDL -o "${TMPDIR:-/tmp}/nuvio-tpk-dts-reabrir"
"${TMPDIR:-/tmp}/nuvio-tpk-dts-reabrir"
