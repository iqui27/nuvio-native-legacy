#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
task_dir=$(mktemp -d);trap 'rm -rf "$task_dir"' EXIT
flags=(-std=c11 -D_GNU_SOURCE -DNV_DTS_WEBOS_RECOVERY -O1 -g -Wall -Wextra -Werror -Isrc)
if [[ ${NV_TEST_VIDEO_DIAG:-0} == 1 ]];then flags+=(-DNV_HTTP_BUFFER_DIAG);fi
if [[ ${SANITIZE:-0} == 1 ]];then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer);fi
cc "${flags[@]}" tests/dts_rebuffer.c src/dts/dts_playback.c src/js.c src/linguas.c src/audioinfo.c \
 -pthread -lm -Wl,--wrap=clock_gettime -o "$task_dir/test"
for mode in dv sdr paused seek audio eof cancel timeout native-dv native-sdr native-seek native-paused native-cancel;do "$task_dir/test" "$mode";done
