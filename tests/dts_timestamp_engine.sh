#!/usr/bin/env bash
set -euo pipefail
# FFmpeg group link: GNU ld needs --start-group for circular static refs;
# Apple ld only lists archives once (all dead-strippable symbols resolved)
# and uses -Wl,-dead_strip instead of -Wl,--gc-sections.
os_name=$(uname -s)
if [[ $os_name == Darwin ]]; then
  AV_LIBS="-lavformat -lavcodec -lswresample -lavutil -lz"
  gc_flag="-Wl,-dead_strip"
else
  AV_LIBS="-Wl,--start-group -lavformat -lavcodec -lswresample -lavutil -Wl,--end-group -lz"
  gc_flag="-Wl,--gc-sections"
fi
cd "$(dirname "$0")/.."
task_tmp=$(mktemp -d)
trap 'rm -rf "$task_tmp"' EXIT
if [ -n "${NUVIO_DTS_ROOT:-}" ]; then
  prefix="$NUVIO_DTS_ROOT"
elif [[ $os_name == Darwin ]]; then
  if [ -d /tmp/nuvio-dts-host/include ]; then
    prefix=/tmp/nuvio-dts-host
  elif [ -f /opt/homebrew/opt/ffmpeg/include/libavformat/avformat.h ]; then
    prefix=/opt/homebrew/opt/ffmpeg
  else
    pkg_cfg=$(command -v pkg-config 2>/dev/null || true)
    if [ -z "$pkg_cfg" ] && [ -x /opt/homebrew/bin/pkg-config ]; then
      pkg_cfg=/opt/homebrew/bin/pkg-config
    fi
    prefix=/tmp/nuvio-dts-host
    if [ -n "$pkg_cfg" ]; then
      pc_prefix=$("$pkg_cfg" --variable=prefix libavformat 2>/dev/null || true)
      if [ -n "$pc_prefix" ]; then
        prefix="$pc_prefix"
      fi
    fi
  fi
else
  prefix=/tmp/nuvio-dts-host
fi
flags=(-std=gnu11 -O1 -g -Wall -Wextra -Werror -Wno-misleading-indentation -ffunction-sections -fdata-sections)
if [[ ${SANITIZE:-0} == 1 ]]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
cc "${flags[@]}" -I"$prefix/include" tests/dts_timestamp_engine.c -o "$task_tmp/test" \
 "$gc_flag" -L"$prefix/lib" $AV_LIBS -lm -lpthread -ldl
"$task_tmp/test"
