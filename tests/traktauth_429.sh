#!/bin/bash
# Reuse the deterministic auth harness; no network or TV required.
set -euo pipefail
cd "$(dirname "$0")/.."
dir=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-traktauth429.XXXXXX")
trap 'rm -rf "$dir"' EXIT
cc tests/trakt610_auth.c src/credfio.c src/js.c src/jsw.c -Isrc \
  -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -o "$dir/auth" -O1 -g -Wno-deprecated-declarations -Wno-macro-redefined -lpthread
"$dir/auth" > "$dir/log"
grep -q '\[trakt\].*429' "$dir/log"
grep -q '\[trakt\] autorizado' "$dir/log"
printf 'traktauth_429: PASS\n'
