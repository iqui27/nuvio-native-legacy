#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
prefix=${NUVIO_DTS_ROOT:-/tmp/nuvio-dts-host}
tmp=$(mktemp -d)
server=""
trap 'if [ -n "$server" ]; then kill "$server" 2>/dev/null || true; fi; rm -rf "$tmp"' EXIT
python3 - "$tmp" <<'PY'
import pathlib,sys
root=pathlib.Path(sys.argv[1])
pattern=bytes(range(251))
for name,size in [('range.bin',16*1024*1024),('prefetch.bin',96*1024*1024)]:
    with (root/name).open('wb') as f:
        f.write((pattern*((size+250)//251))[:size])
(root/'dv-prefetch.bin').symlink_to(root/'prefetch.bin')
PY
python3 tests/dts_engine_server.py "$tmp" "$tmp/port" "$tmp/requests" &
server=$!
for i in $(seq 50); do [ -s "$tmp/port" ] && break; sleep .1; done
port=$(cat "$tmp/port")
cc -std=gnu11 -Wall -Wextra -Werror -Wno-misleading-indentation \
  -I"$prefix/include" tests/dts_engine_avio.c src/rede.c src/redeurl.c \
  -o "$tmp/ranges" -L"$prefix/lib" -Wl,--start-group \
  -lavformat -lavcodec -lswresample -lavutil -Wl,--end-group -lm -lpthread -ldl
"$tmp/ranges" "http://127.0.0.1:$port/parallel/range.bin"
"$tmp/ranges" "http://127.0.0.1:$port/slow/range.bin"
"$tmp/ranges" "http://127.0.0.1:$port/prefetch/prefetch.bin"
"$tmp/ranges" "http://127.0.0.1:$port/prefetch/dv-prefetch.bin"
