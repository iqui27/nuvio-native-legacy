#!/bin/bash
# Seek recusado (webOS 700): tentativas com espera, sem derrubar a fonte.
# E o seek segurado pela sonda do mkvfix no TPK (tests/tpk_seek_segurado.c):
# sonda tardia/timeout, seeks sucessivos, retomar em alvo diferente de zero
# com ocultacao de ASS e o rotulo separando queda de rede de preparo do
# arquivo na reabertura local.
set -eu
cd "$(dirname "$0")/.."
cc -O1 -g -Wall -Wextra -Werror tests/seekretry.c -o "${TMPDIR:-/tmp}/nuvio-seekretry-tests"
"${TMPDIR:-/tmp}/nuvio-seekretry-tests"
SDL="-I/opt/homebrew/include $(sdl2-config --cflags) $(sdl2-config --libs)"
cc -O1 -g -Wall -DNV_TPK -Isrc tests/tpk_seek_segurado.c tests/mkvfix_mp4_stub.c \
  src/video_tpk.c src/video_modo.c src/vazao.c src/velocidade.c src/faixasmkv.c src/audioinfo.c \
  src/audioescolha.c src/mkv.c src/capmkv.c tests/dts_tpk_stub.c -lpthread $SDL \
  -o "${TMPDIR:-/tmp}/nuvio-tpk-seek-segurado"
"${TMPDIR:-/tmp}/nuvio-tpk-seek-segurado"
