#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
prefix=${NUVIO_DTS_ROOT:-/tmp/nuvio-dts-host}
tmp=$(mktemp -d)
server=""
trap 'if [ -n "$server" ]; then kill "$server" 2>/dev/null || true; fi; rm -rf "$tmp"' EXIT
ffmpeg -hide_banner -loglevel error \
  -f lavfi -i 'color=size=64x64:rate=25:duration=4' \
  -f lavfi -i 'sine=frequency=440:sample_rate=48000:duration=4' \
  -c:v libx264 -bf 0 -g 25 -c:a dca -ac 2 -strict -2 -b:a 768k "$tmp/sdr-dts.mkv"
python3 - "$tmp/sdr-dts.mkv" <<'PY'
import sys
with open(sys.argv[1],'ab') as f: f.truncate(300*1048576)
PY
python3 tests/dts_engine_server.py "$tmp" "$tmp/port" "$tmp/requests" &
server=$!
for i in $(seq 50); do [ -s "$tmp/port" ] && break; sleep .1; done
cc -std=gnu11 -Wall -Wextra -Werror -Wno-misleading-indentation \
  -DNV_DTS_FFMPEG -DNV_DTS_WEBOS_RECOVERY -Isrc -I"$prefix/include" \
  src/dts/dts_engine.c tests/dts_buffer_reporting.c src/rede.c src/redeurl.c \
  -o "$tmp/test" -L"$prefix/lib" -Wl,--start-group \
  -lavformat -lavcodec -lswresample -lavutil -Wl,--end-group -lm -lpthread -ldl
"$tmp/test" "http://127.0.0.1:$(cat "$tmp/port")/telemetry/sdr-dts.mkv"
