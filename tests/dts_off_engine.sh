#!/usr/bin/env bash
# Settings > DTS "Off" (LG): the real engine with an HEVC film carrying DTS and AC-3.
set -euo pipefail
cd "$(dirname "$0")/.."
prefix=${NUVIO_DTS_ROOT:-/tmp/nuvio-dts-host}
tmp=$(mktemp -d);trap 'rm -rf "$tmp"' EXIT
flags=(-std=gnu11 -O1 -g -Wall -Wextra -Werror -DNV_DTS_FFMPEG -DNV_DTS_WEBOS_RECOVERY -Isrc -I"$prefix/include")
if [[ ${SANITIZE:-0} == 1 ]];then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer);fi
ffmpeg -hide_banner -loglevel error -f lavfi -i 'testsrc2=size=128x72:rate=24' -f lavfi -i 'sine=frequency=500:sample_rate=48000' \
  -t 4 -map 0:v -map 1:a -map 1:a -c:v libx265 -x265-params 'pools=1:frame-threads=1:log-level=error:keyint=24' \
  -c:a:0 dca -strict -2 -b:a:0 768k -c:a:1 ac3 -b:a:1 384k "$tmp/input.mkv"
cc "${flags[@]}" src/dts/dts_engine.c tests/dts_off_engine.c \
  -L"$prefix/lib" -Wl,--start-group -lavformat -lavcodec -lswresample -lavutil -Wl,--end-group -lm -pthread -o "$tmp/test"
"$tmp/test" "$tmp/input.mkv"
# The same with the Dolby Vision TrueHD conversion built in (the test builds' flag).
cc "${flags[@]}" -DNV_DV_TRUEHD_TEST src/dts/dts_engine.c tests/dts_off_engine.c \
  -L"$prefix/lib" -Wl,--start-group -lavformat -lavcodec -lswresample -lavutil -Wl,--end-group -lm -pthread -o "$tmp/test-truehd"
"$tmp/test-truehd" "$tmp/input.mkv"
