#!/bin/bash
# Uses installed host FFmpeg; no network or TV required.
set -eu
cd "$(dirname "$0")/.."
DTS_TAIL_DIR=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-tail.XXXXXX")
trap 'rm -rf "$DTS_TAIL_DIR"' EXIT
cc -std=gnu11 -Wall -Wextra -Werror -Wno-misleading-indentation -Wno-implicit-const-int-float-conversion -Isrc \
  $(pkg-config --cflags libavformat libavcodec libavutil libswresample) \
  tests/dts_p2p_tail.c -o "$DTS_TAIL_DIR/test" \
  $(pkg-config --libs libavformat libavcodec libavutil libswresample) -lpthread -lm
"$DTS_TAIL_DIR/test"
