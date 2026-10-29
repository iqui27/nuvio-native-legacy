#!/bin/bash
# Exercise production feed refresh with titles absent from the discovery batch.
set -euo pipefail
cd "$(dirname "$0")/.."
tmp=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-amigosarte.XXXXXX")
trap 'rm -rf "$tmp"' EXIT
python3 - "$tmp/feed.inc" <<'PY'
from pathlib import Path
import sys
s = Path('src/socialvis.c').read_text()
a = s.find('static void completarArteFeed(')
if a < 0: a = s.index('static void refazer(void) {')
b = s.index('// Consume account changes', a)
Path(sys.argv[1]).write_text(s[a:b])
PY
cc tests/amigosarte.c -I"$tmp" -Isrc -I/opt/homebrew/include/SDL2 -O1 -Wall -Wextra -o "$tmp/test"
"$tmp/test"
