#!/bin/bash
# Sem TV: ROI do .tpk sempre dentro da tela no padrao; o canario
# (NV_TPK_ZOOM_ROI=1) ainda deixa o zoom de antes passar.
set -eu
cd "$(dirname "$0")/.."
SDL="-I/opt/homebrew/include $(sdl2-config --cflags) $(sdl2-config --libs)"
cc -O1 -g -Wall -DNV_TPK -Isrc tests/tpk-roi.c src/video_tpk.c src/video_modo.c src/velocidade.c $SDL -o /tmp/nuvio-tpk-roi
/tmp/nuvio-tpk-roi
cc -O1 -g -Wall -DNV_TPK -DNV_TPK_ZOOM_ROI=1 -Isrc tests/tpk-roi.c src/video_tpk.c src/video_modo.c src/velocidade.c $SDL -o /tmp/nuvio-tpk-roi-canario
/tmp/nuvio-tpk-roi-canario
