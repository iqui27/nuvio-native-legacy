#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-playlist.XXXXXX")
trap 'rm -rf "$work"' EXIT
cc tests/servidores_playlist.c src/jellyfin.c src/plex.c src/js.c src/jsw.c src/redeurl.c src/srvidx.c \
  -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -O1 -g -ffunction-sections -fdata-sections -Wl,-dead_strip -pthread \
  -Wno-deprecated-declarations -Wno-macro-redefined -o "$work/test"
rc=0
for phase in 0 1 2 3; do
  if "$work/test" "$phase"; then
    echo "playlist phase $phase: PASS"
  else
    echo "playlist phase $phase: FAIL"
    rc=1
  fi
done
exit "$rc"
