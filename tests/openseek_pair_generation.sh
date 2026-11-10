#!/bin/bash
set -eu
export TMPDIR="${TMPDIR:-/tmp}"
cd "$(dirname "$0")/.."
case "$(uname -s)" in Darwin) strip=-Wl,-dead_strip ;; *) strip=-Wl,--gc-sections ;; esac
cc -D_DEFAULT_SOURCE -ffunction-sections -fdata-sections -Isrc $(pkg-config --cflags sdl2 SDL2_image) \
  tests/openseek_pair_generation.c src/openseekproto.c src/osp1.c src/seekrvtt.c src/js.c src/redeurl.c \
  "$strip" $(pkg-config --libs sdl2 SDL2_image) -o "$TMPDIR/nuvio-openseek-pair-generation"
for mode in normal forget aba; do "$TMPDIR/nuvio-openseek-pair-generation" "$mode"; done
