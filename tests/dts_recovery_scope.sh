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
task_tmp=$(mktemp -d); server=''
trap 'if [[ -n $server ]]; then kill "$server" 2>/dev/null || true; fi; rm -rf "$task_tmp"' EXIT
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
cc "${flags[@]}" -I"$prefix/include" tests/dts_recovery_scope.c "$gc_flag" \
 -L"$prefix/lib" $AV_LIBS \
 -lm -lpthread -ldl -o "$task_tmp/timestamp"
"$task_tmp/timestamp"
python3 - "$task_tmp" <<'PY'
from pathlib import Path
import sys
(Path(sys.argv[1])/'range.bin').write_bytes(bytes(range(251))*100)
PY
python3 tests/dts_engine_server.py "$task_tmp" "$task_tmp/port" "$task_tmp/requests" &
server=$!
for i in $(seq 50); do [[ -s $task_tmp/port ]] && break; sleep .1; done
port=$(cat "$task_tmp/port")
cc "${flags[@]}" -DNV_TEST_LEGACY_RECOVERY -I"$prefix/include" \
 tests/dts_range_retry.c src/rede.c src/redeurl.c -o "$task_tmp/range" \
 -L"$prefix/lib" $AV_LIBS -lm -lpthread -ldl
"$task_tmp/range" "http://127.0.0.1:$port/retry-exhaust/range.bin"
python3 - "$task_tmp/requests" <<'PY'
import json,sys
rows=[json.loads(x) for x in open(sys.argv[1])]
wanted=[r for r in rows if r['path']=='/retry-exhaust/range.bin' and r['lo']==0]
assert len(wanted)==1, wanted
print('PASS: without webOS recovery, failed ranges receive no added retry')
PY
