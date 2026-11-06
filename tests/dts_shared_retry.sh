#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
task_tmp=$(mktemp -d); server=''
trap 'if [[ -n $server ]]; then kill "$server" 2>/dev/null || true; fi; rm -rf "$task_tmp"' EXIT
python3 - "$task_tmp/range.bin" <<'PY'
import sys
open(sys.argv[1], 'wb').write(bytes(range(251))*100)
PY
python3 tests/dts_engine_server.py "$task_tmp" "$task_tmp/port" "$task_tmp/requests" &
server=$!
for i in $(seq 50); do [[ -s $task_tmp/port ]] && break; sleep .1; done
port=$(<"$task_tmp/port")
cc -std=gnu11 -O1 -g -DRANGE_WORKERS=2 -Wno-deprecated-declarations $(pkg-config --cflags libavformat libavcodec libavutil libswresample) tests/dts_range_retry.c src/rede.c src/redeurl.c -o "$task_tmp/test" $(pkg-config --libs libavformat libavcodec libavutil libswresample) -lm -lpthread
"$task_tmp/test" "http://127.0.0.1:$port/shared-retry/range.bin"
"$task_tmp/test" "http://127.0.0.1:$port/shared-cancel/range.bin"
"$task_tmp/test" "http://127.0.0.1:$port/shared-isolation/range.bin"
cc -std=gnu11 -O1 -g -DRANGE_WORKERS=2 -DNV_TEST_LEGACY_RECOVERY -Wno-deprecated-declarations $(pkg-config --cflags libavformat libavcodec libavutil libswresample) tests/dts_range_retry.c src/rede.c src/redeurl.c -o "$task_tmp/legacy" $(pkg-config --libs libavformat libavcodec libavutil libswresample) -lm -lpthread
"$task_tmp/legacy" "http://127.0.0.1:$port/shared-legacy/range.bin"
python3 - "$task_tmp/requests" <<'PY'
import json, sys
rows=[json.loads(x) for x in open(sys.argv[1])]
assert sum(r['lo']==0 for r in rows if r['path'].startswith('/shared-legacy/')) == 1
rows=[r for r in rows if r['path'].startswith('/shared-retry/')]
first=next(r for r in rows if r['lo']==0 and r['attempt']==1)
after=[r for r in rows if r['lo']>=2048 or (r['lo']==0 and r['attempt']>1)]
assert after and all(r['time']-first['time']>=.9 for r in after), rows
print('PASS: workers honor shared Retry-After')
PY
