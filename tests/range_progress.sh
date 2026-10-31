#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
task_dir=$(mktemp -d);server=''
trap 'if [[ -n $server ]];then kill "$server" 2>/dev/null || true;fi;rm -rf "$task_dir"' EXIT
python3 - "$task_dir" <<'PY'
from pathlib import Path
import sys
p=Path(sys.argv[1]);n=12*1048576;pattern=bytes(range(251))
(p/'dv.bin').write_bytes((pattern*((n+250)//251))[:n])
PY
python3 tests/dts_engine_server.py "$task_dir" "$task_dir/port" "$task_dir/requests" &
server=$!
for i in $(seq 50);do [[ -s $task_dir/port ]] && break;sleep .1;done
port=$(cat "$task_dir/port")
flags=(-std=gnu11 -O1 -g -Wall -Wextra -Werror -Isrc)
if [[ ${SANITIZE:-0} == 1 ]];then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer);fi
cc "${flags[@]}" tests/range_progress.c src/rede.c src/redeurl.c -pthread -ldl -o "$task_dir/test"
"$task_dir/test" "http://127.0.0.1:$port/retry-live-dripall/dv.bin" progress
"$task_dir/test" "http://127.0.0.1:$port/retry-live-bodyhang/dv.bin" idle
"$task_dir/test" "http://127.0.0.1:$port/retry-live-dvtrickle/dv.bin" trickle
"$task_dir/test" "http://127.0.0.1:$port/retry-live-dripall/dv.bin" cancel
python3 - "$task_dir/requests" <<'PY'
import collections,json,sys
rows=[json.loads(l) for l in open(sys.argv[1])]
counts=collections.Counter(r['path'] for r in rows)
assert counts['/retry-live-dripall/dv.bin']==2,counts
assert counts['/retry-live-bodyhang/dv.bin']==1 and counts['/retry-live-dvtrickle/dv.bin']==1,counts
assert all(r['auth'] for r in rows)
print('PASS: progressing gap completes in one request; idle/trickle retry guards and cancellation retained')
PY
