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
# Lossless noise keeps the file above 8 MiB; mkvmerge puts Cues and Tags at
# EOF, both in the front SeekHead, as on most real releases.
ffmpeg -hide_banner -loglevel error \
  -f lavfi -i 'nullsrc=size=320x240:rate=25:duration=4,geq=lum=random(1)*255:cb=128:cr=128' \
  -f lavfi -i 'sine=frequency=300:sample_rate=48000:duration=4' \
  -f lavfi -i 'sine=frequency=900:sample_rate=48000:duration=4' \
  -map 0:v -map 1:a -map 2:a -c:v libx264 -qp 0 -g 25 -bf 0 \
  -c:a dca -strict -2 -b:a 768000 "$DTS_TAIL_DIR/ff.mkv"
mkvmerge -q -o "$DTS_TAIL_DIR/p2p.mkv" "$DTS_TAIL_DIR/ff.mkv"
"$DTS_TAIL_DIR/test" "$DTS_TAIL_DIR/p2p.mkv"
