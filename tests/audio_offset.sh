#!/bin/bash
# Offline converted-packet presentation regression; uses installed FFmpeg only.
set -eu
cd "$(dirname "$0")/.."
cc -fsyntax-only -DNV_TPK -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 src/video_tpk.c src/ajustes.c
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
cc -O1 -g -Wall -Wextra -Wno-unused-parameter -ffunction-sections -fdata-sections \
  -Isrc $(pkg-config --cflags libavformat libavcodec libavutil libswresample) \
  tests/audio_offset.c src/dts/dts_engine.c -Wl,-dead_strip \
  $(pkg-config --libs libavformat libavcodec libavutil libswresample) -lpthread -lm \
  -o "$work/audio_offset"
"$work/audio_offset"
