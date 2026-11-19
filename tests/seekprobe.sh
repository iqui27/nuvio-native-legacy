#!/bin/bash
# #557: Range probe tells a host that ignores Range (200) from one that honors it (206).
set -euo pipefail
cd "$(dirname "$0")/.."
D=$(mktemp -d "${TMPDIR:-/tmp}/nv-seekprobe.XXXXXX")
trap 'kill ${S1:-} ${S2:-} 2>/dev/null || true; rm -rf "$D"' EXIT
head -c 4096 /dev/zero > "$D/f.mkv"
cc -Isrc -I/opt/homebrew/include -O1 -g -Wall -Wextra src/seekprobe.c src/rede.c src/redeurl.c src/dados.c \
  tests/seekprobe.c -pthread -lm -lz -Wno-deprecated-declarations -o "$D/t"
P=$((20000 + RANDOM % 20000))
python3 tests/legref_rangesrv.py "$D" $P & S1=$!
python3 tests/legref_rangesrv.py "$D" $((P + 1)) semrange & S2=$!
for i in $(seq 50); do nc -z 127.0.0.1 $P 2>/dev/null && nc -z 127.0.0.1 $((P + 1)) 2>/dev/null && break; sleep 0.1; done
"$D/t" "http://127.0.0.1:$P/f/f.mkv" "http://127.0.0.1:$((P + 1))/f/f.mkv" 2>/dev/null
