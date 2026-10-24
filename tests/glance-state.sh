#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
export TMPDIR=/Volumes/ExternalSSD/tmp/jobtmp
mkdir -p "$TMPDIR"
tmp="$(mktemp -d "$TMPDIR/glance-state-XXXXXX")"
trap 'rm -rf "$tmp"' EXIT
cc -O1 -g -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -Wno-deprecated-declarations -ffunction-sections -fdata-sections \
  tests/glance-retarget.c src/trailerfonte.c -Wl,-dead_strip -o "$tmp/retarget"
"$tmp/retarget"
python3 - "$tmp/router.h" <<'PY'
from pathlib import Path
import sys
source = Path('src/app.c').read_text()
start = source.index('static void trocaDeTituloSeSolicitada(void) {')
end = source.index('\n}\n', start) + 3
Path(sys.argv[1]).write_text(source[start:end])
PY
cc -O1 -g -I"$tmp" -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -Wno-deprecated-declarations -ffunction-sections -fdata-sections \
  tests/glance-state.c src/trailerfonte.c -Wl,-dead_strip -o "$tmp/test"
"$tmp/test"
cc -O1 -g -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -Wno-deprecated-declarations -ffunction-sections -fdata-sections \
  tests/glance-collision.c -Wl,-dead_strip -o "$tmp/collision"
"$tmp/collision"
cc -O1 -g -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -Wno-deprecated-declarations -ffunction-sections -fdata-sections \
  tests/glance-search.c -Wl,-dead_strip -o "$tmp/search"
"$tmp/search"
cc -O1 -g -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -Wno-deprecated-declarations -ffunction-sections -fdata-sections \
  tests/glance-home.c -Wl,-dead_strip -o "$tmp/home"
"$tmp/home"
