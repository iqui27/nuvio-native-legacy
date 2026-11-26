#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
prefix=${NUVIO_DTS_ROOT:-/tmp/nuvio-dts-host}
tmp=$(mktemp -d)
server=''
trap 'if [ -n "$server" ]; then kill "$server" 2>/dev/null || true; fi; rm -rf "$tmp"' EXIT
python3 - "$tmp/cache.bin" <<'PY'
from pathlib import Path
import sys
block=bytes(range(251))*4178
with Path(sys.argv[1]).open('wb') as f:
    # Enough real patterned payload for every speculative range at 256 MiB.
    for _ in range(350):f.write(block)
PY
python3 tests/dts_engine_server.py "$tmp" "$tmp/port" "$tmp/requests" &
server=$!
for i in $(seq 50); do [ -s "$tmp/port" ] && break; sleep .1; done
extra=()
if [ "${NV_TEST_CACHE_REUSE:-0}" = 1 ]; then extra+=(-DNV_NATIVE_SEEK_TEST); fi
if [ "${SANITIZE:-0}" = 1 ]; then extra+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
cc "${extra[@]}" -std=gnu11 -Wall -Wextra -Werror -Wno-misleading-indentation -Isrc -I"$prefix/include" \
  tests/dts_cached_seek.c src/rede.c src/redeurl.c -o "$tmp/test" -L"$prefix/lib" \
  -Wl,--start-group -lavformat -lavcodec -lswresample -lavutil -Wl,--end-group -lm -lpthread -ldl
"$tmp/test" "http://127.0.0.1:$(cat "$tmp/port")/cache.bin"
