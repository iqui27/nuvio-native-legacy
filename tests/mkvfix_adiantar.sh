#!/bin/bash
# Prefetch on leaving the app (mkvfix.h), end to end: the real local server
# (mkvfix.c + rede.c) in front of a local fake CDN. No external network.
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
set -eu
cd "$(dirname "$0")/.."
D=$(mktemp -d "$TMPDIR/nuvio-adiantar.XXXXXX")
PID=''
trap 'if [ -n "$PID" ]; then kill "$PID" 2>/dev/null || true; fi; rm -rf "$D"' EXIT
: > "$D/head.bin"; : > "$D/tail.bin"
python3 tests/mkvfix_adiantar_server.py "$D" &
PID=$!
for _ in $(seq 50); do [ -s "$D/porta" ] && break; sleep 0.1; done
${CC:-gcc} -O1 -g -Wall -Wno-misleading-indentation -Isrc tests/mkvfix_adiantar.c src/mkvfix.c src/rede.c src/redeurl.c \
  -ldl -lpthread -o "$D/test"
"$D/test" "$D" "$(cat "$D/porta")"
