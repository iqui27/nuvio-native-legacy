#!/usr/bin/env bash
set -euo pipefail
# FFmpeg group link: GNU ld needs --start-group for circular static refs;
# Apple ld only lists archives once (all dead-strippable symbols resolved).
if [[ $(uname -s) == Darwin ]]; then
  AV_LIBS="-lavformat -lavcodec -lswresample -lavutil"
else
  AV_LIBS="-Wl,--start-group -lavformat -lavcodec -lswresample -lavutil -Wl,--end-group"
fi
cd "$(dirname "$0")/.."
task_tmp=$(mktemp -d)
trap 'rm -rf "$task_tmp"' EXIT
prefix=${NUVIO_DTS_ROOT:-/tmp/nuvio-dts-host}
flags=(-std=gnu11 -O1 -g -Wall -Wextra -Werror -Wno-misleading-indentation -ffunction-sections -fdata-sections)
if [[ ${SANITIZE:-0} == 1 ]]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
cc "${flags[@]}" -I"$prefix/include" tests/dts_timestamp_engine.c -o "$task_tmp/test" \
 -Wl,--gc-sections -L"$prefix/lib" $AV_LIBS -lm -lpthread -ldl
"$task_tmp/test"
