#!/bin/bash
# A 4.4 MB Tracks (log 12): the probe reads it in pieces and hides the ASS
# tracks. Real local server (mkvfix.c + rede.c) in front of the local fake CDN
# of tests/mkvfix_adiantar_server.py. No external network.
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
set -eu
cd "$(dirname "$0")/.."
D=$(mktemp -d "$TMPDIR/nuvio-tracksgrande.XXXXXX")
PID=''
trap 'if [ -n "$PID" ]; then kill "$PID" 2>/dev/null || true; fi; rm -rf "$D"' EXIT
: > "$D/head.bin"; : > "$D/tail.bin"
python3 tests/mkvfix_adiantar_server.py "$D" &
PID=$!
for _ in $(seq 50); do [ -s "$D/porta" ] && break; sleep 0.1; done
${CC:-gcc} -O1 -g -Wall -Wno-misleading-indentation -Isrc tests/mkvfix_tracks_grande.c src/mkvfix.c src/rede.c src/redeurl.c \
  -ldl -lpthread -o "$D/test"
"$D/test" "$D" "$(cat "$D/porta")"
