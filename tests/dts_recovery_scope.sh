#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
task_tmp=$(mktemp -d); server=''
trap 'if [[ -n $server ]]; then kill "$server" 2>/dev/null || true; fi; rm -rf "$task_tmp"' EXIT
prefix=${NUVIO_DTS_ROOT:-/tmp/nuvio-dts-host}
flags=(-std=gnu11 -O1 -g -Wall -Wextra -Werror -Wno-misleading-indentation -ffunction-sections -fdata-sections)
cc "${flags[@]}" -I"$prefix/include" tests/dts_recovery_scope.c -Wl,--gc-sections \
 -L"$prefix/lib" -Wl,--start-group -lavformat -lavcodec -lswresample -lavutil -Wl,--end-group \
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
 -L"$prefix/lib" -Wl,--start-group -lavformat -lavcodec -lswresample -lavutil -Wl,--end-group -lm -lpthread -ldl
"$task_tmp/range" "http://127.0.0.1:$port/retry-exhaust/range.bin"
python3 - "$task_tmp/requests" <<'PY'
import json,sys
rows=[json.loads(x) for x in open(sys.argv[1])]
wanted=[r for r in rows if r['path']=='/retry-exhaust/range.bin' and r['lo']==0]
assert len(wanted)==1, wanted
print('PASS: without webOS recovery, failed ranges receive no added retry')
PY
