#!/bin/bash
set -eu
cd "$(dirname "$0")/.."
EMSDK="/Users/hrocha/emsdk/upstream/emscripten"
PATH="$EMSDK:$PATH" emcc -O0 -std=c99 -D__EMSCRIPTEN__ \
  -I/opt/homebrew/include -Isrc \
  -ffunction-sections -fdata-sections \
  tests/home-trailer-timer.c src/trailerfonte.c -Wl,--gc-sections \
  -o /tmp/nuvio-home-trailer-timer.js
node /tmp/nuvio-home-trailer-timer.js
