#!/usr/bin/env bash
# HEVC Annex-B conversion that keeps the stream's own parameter sets.
# Always: pure unit checks. With NUVIO_DTS_ROOT (pinned FFmpeg 7.1.5 build):
# byte parity with FFmpeg's hevc_mp4toannexb for packets without own sets.
set -euo pipefail
cd "$(dirname "$0")/.."
tmp=$(mktemp -d);trap 'rm -rf "$tmp"' EXIT
flags=(-std=c11 -O1 -g -Wall -Wextra -Werror -Isrc)
if [[ ${SANITIZE:-0} == 1 ]];then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer);fi
cc "${flags[@]}" tests/hevc_annexb.c -o "$tmp/unit"
"$tmp/unit"
if [[ -n ${NUVIO_DTS_ROOT:-} ]];then
  prefix=$NUVIO_DTS_ROOT
  cc "${flags[@]}" -I"$prefix/include" tests/hevc_annexb_ffmpeg.c \
    -L"$prefix/lib" -Wl,--start-group -lavformat -lavcodec -lswresample -lavutil -Wl,--end-group \
    -lm -pthread -o "$tmp/parity"
  "$tmp/parity"
fi
