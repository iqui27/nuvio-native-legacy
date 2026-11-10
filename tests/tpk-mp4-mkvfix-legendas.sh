#!/bin/bash
# No TV: wrong MP4 hint must keep the mkvfix file subtitle menu (see
# tests/tpk-mp4-mkvfix-legendas.c). Fake host and mkvfix hooks; no network.
set -eu
cd "$(dirname "$0")/.."
SDL="-I/opt/homebrew/include $(sdl2-config --cflags) $(sdl2-config --libs)"
cc -O1 -g -Wall -DNV_TPK -Isrc tests/tpk-mp4-mkvfix-legendas.c tests/mkvfix_mp4_stub.c \
  src/video_tpk.c src/video_modo.c src/vazao.c src/velocidade.c src/faixasmkv.c src/audioinfo.c \
  src/audioescolha.c src/mkv.c src/capmkv.c tests/dts_tpk_stub.c -lpthread $SDL \
  -o "${TMPDIR:-/tmp}/nuvio-tpk-mp4-mkvfix-legendas"
"${TMPDIR:-/tmp}/nuvio-tpk-mp4-mkvfix-legendas"
