#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
prefix=${NUVIO_DTS_ROOT:-/tmp/nuvio-dts-host}
task_tmp=$(mktemp -d)
server=''
trap 'if [ -n "$server" ]; then kill "$server" 2>/dev/null || true; fi; rm -rf "$task_tmp"' EXIT
python3 tests/dts_resolver_reuse_server.py "$task_tmp/port" "$task_tmp/requests" &
server=$!
for _ in $(seq 50); do [ -s "$task_tmp/port" ] && break; sleep .1; done
flags=(-std=gnu11 -O1 -g -Wall -Wextra -Werror -Wno-misleading-indentation -Isrc -I"$prefix/include")
if [[ ${SANITIZE:-0} == 1 ]]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
cc "${flags[@]}" tests/dts_resolver_reuse.c src/rede.c src/redeurl.c -o "$task_tmp/test" \
  -L"$prefix/lib" -Wl,--start-group -lavformat -lavcodec -lswresample -lavutil -Wl,--end-group -lm -lpthread -ldl
"$task_tmp/test" "http://127.0.0.1:$(cat "$task_tmp/port")"
python3 - "$task_tmp/requests" <<'PY'
import json
import sys
rows = [json.loads(line) for line in open(sys.argv[1])]
assert sum(r['path'] == '/resolve' for r in rows) == 2, 'Only startup and expiry resolve the URL'
assert sum(r['path'] == '/resolve-limit' for r in rows) == 2
assert all(r['auth'] == r['resolver'] for r in rows)
limited = next(r for r in rows if not r['resolver'] and r['lo'] == 512 and r['status'] == 429)
after = [r for r in rows if r['mono'] > limited['mono'] and not r['resolver'] and r['lo'] in (512, 544, 576, 608)]
assert len(after) == 4 and all(r['status'] == 206 and r['mono'] - limited['mono'] >= .95 for r in after)
assert sum(r['lo'] == 1024 for r in rows) == 1, 'Cancelled seek does not send an HTTP request'
assert sum(r['lo'] == 2048 for r in rows) == 1, 'Closing interrupts long Retry-After'
print('PASS: resolver request counts, shared Retry-After timing and cross-origin authentication verified on real HTTP')
PY
