#!/bin/bash
# No TV: the deferred track selection of the .tpk, step by step.
#
# Pins the split the fix relies on: choices made before playback stay in the
# app, audio leaves on the first tick, the subtitle waits out the player's
# settle window, and a change during playback goes out immediately. Runs
# video_tpk.c against a fake host (same path as tpk-roi.sh).
#
# Costs ~3 s of real clock (the settle window is awaited with SDL_Delay), and
# needs no network, no GL and no device.
set -eu
cd "$(dirname "$0")/.."
SDL="-I/opt/homebrew/include $(sdl2-config --cflags) $(sdl2-config --libs)"
cc -O1 -g -Wall -DNV_TPK -DLEG_SEM_CUE_MS=1000u -Isrc tests/tpk-escolha.c src/video_tpk.c src/video_modo.c src/velocidade.c src/faixasmkv.c src/mkv.c $SDL -o "${TMPDIR:-/tmp}/nuvio-tpk-escolha"
"${TMPDIR:-/tmp}/nuvio-tpk-escolha"
