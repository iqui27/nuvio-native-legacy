#!/bin/bash
# Speed test through a cross-origin redirect (debrid -> CDN) keeps Range.
set -euo pipefail
cd "$(dirname "$0")/.."
tmp=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-sfredir.XXXXXX")
srv=""
trap '[ -z "$srv" ] || kill "$srv" 2>/dev/null || true; rm -rf "$tmp"' EXIT
python3 tests/streamfit_diagnostico_server.py "$tmp/port" &
srv=$!
for i in $(seq 50); do [ -s "$tmp/port" ] && break; sleep .1; done
for c in 1 4; do
  cc -std=gnu11 -O1 -g -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 -pthread -Wno-deprecated-declarations -Wno-macro-redefined \
    -DSTREAMFITDIAG_CONEXOES=$c tests/streamfit_redirect_range.c src/streamfitdiag.c src/redemarca.c src/streamfit.c src/vazao.c src/rede.c src/redeurl.c -o "$tmp/t$c"
  "$tmp/t$c" "$(cat "$tmp/port")"
done
echo "streamfit_redirect_range: OK"
