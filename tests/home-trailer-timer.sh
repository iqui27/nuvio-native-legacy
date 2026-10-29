#!/bin/bash
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
set -euo pipefail
cd "$(dirname "$0")/.."
EMSDK="/Users/hrocha/emsdk/upstream/emscripten"
PATH="$EMSDK:$PATH" emcc -O0 -std=gnu99 -D__EMSCRIPTEN__ \
  -I/opt/homebrew/include -Isrc \
  -ffunction-sections -fdata-sections \
  src/cwordem.c src/cwretido.c tests/home-trailer-timer.c tests/amigosfil_stub.c src/trailerfonte.c -Wl,--gc-sections \
  -o ${TMPDIR:-/tmp}/nuvio-home-trailer-timer.js
node ${TMPDIR:-/tmp}/nuvio-home-trailer-timer.js
# Native TPK scheduling is a different production branch: no WGT iframe,
# and the real Home source order can be IMDb -> Apple. Only these four units
# are linked; unused UI edges stay uncalled rather than compiling the core.
cc -DNV_TPK -O1 -g -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -Wno-return-stack-address -ffunction-sections -fdata-sections \
  src/cwordem.c src/cwretido.c tests/home-trailer-timer.c tests/amigosfil_stub.c src/trailerfonte.c \
  -Wl,-dead_strip,-undefined,dynamic_lookup -o ${TMPDIR:-/tmp}/nuvio-home-trailer-tpk
${TMPDIR:-/tmp}/nuvio-home-trailer-tpk
cc -DNV_ANDROID -O1 -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -ffunction-sections -fdata-sections tests/trailer-android-geometry.c \
  -Wl,-dead_strip,-undefined,dynamic_lookup -o ${TMPDIR:-/tmp}/nuvio-trailer-android-geometry
${TMPDIR:-/tmp}/nuvio-trailer-android-geometry
bash tests/video_android_janela.sh
python3 tests/android_window_ready.py
