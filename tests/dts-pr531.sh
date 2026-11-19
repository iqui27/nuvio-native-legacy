#!/bin/bash
# No source downloads or TCP listeners: socketpair stop regression and real remux queues.
set -eu
cd "$(dirname "$0")/.."
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
cc -O1 -g -Wall -Wextra -Wno-unused-parameter -Isrc tests/dts_pr531_stop.c \
  src/dts/dts_remux.c src/dts/dts_engine.c -lpthread -lm -o "$work/stop"
cc -O1 -g -Wall -Wextra -Wno-unused-parameter -ffunction-sections -fdata-sections \
  $(pkg-config --cflags libavformat libavcodec libavutil libswresample) \
  tests/dts_pr531_memory.c -Wl,-dead_strip \
  $(pkg-config --libs libavformat libavcodec libavutil libswresample) -lpthread -lm -o "$work/memory"
rc=0
"$work/stop" || rc=1
"$work/memory" || rc=1
exit "$rc"
