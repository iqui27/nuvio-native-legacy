#!/usr/bin/env bash
# Offline real PGS decoder + controller. Uses an existing FFmpeg, never installs.
set -euo pipefail
cd "$(dirname "$0")/.."
prefix=${NUVIO_DTS_ROOT:-/opt/homebrew/opt/ffmpeg}
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
flags=(-std=c11 -D_GNU_SOURCE -O1 -g -Wall -Wextra -Werror -Isrc -I"$prefix/include")
if [[ ${SANITIZE:-0} == 1 ]];then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer);fi
libs=(-L"$prefix/lib" -lavcodec -lavutil -pthread -lm)
cc "${flags[@]}" -DNV_DTS_FFMPEG tests/dts_pgs.c "${libs[@]}" -o "$tmp/decoder"
cc "${flags[@]}" -DNV_DTS_REAL_SUBTITLES -D__real_clock_gettime=clock_gettime \
  -c tests/dts_playback.c -o "$tmp/fixture.o"
cc "${flags[@]}" -DNV_DTS_FFMPEG -DNV_DTS_WEBOS_RECOVERY -DDTS_PLAYBACK_TEST_HOOKS \
  -Dclock_gettime=__wrap_clock_gettime -c src/dts/dts_playback.c -o "$tmp/playback.o"
cc "${flags[@]}" -DNV_DTS_FFMPEG "$tmp/fixture.o" "$tmp/playback.o" \
  src/dts/dts_subtitles.c src/js.c src/linguas.c src/audioinfo.c "${libs[@]}" -o "$tmp/playback"
printf 'Real PGS fixture libavcodec major: '
awk '/^#define LIBAVCODEC_VERSION_MAJOR / { print $3 }' "$prefix/include/libavcodec/version_major.h"
# Run both even when one fails, so decoder and playback red evidence is visible.
rc=0
"$tmp/decoder" || rc=1
"$tmp/playback" realpgs || rc=1
exit "$rc"
