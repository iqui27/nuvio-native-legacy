#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
task_tmp=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-simkl-anime-XXXXXX")
trap 'rm -rf "$task_tmp"' EXIT
cc -O1 -g -Wall -Wextra -ffunction-sections -fdata-sections -Wl,-dead_strip -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -Wno-deprecated-declarations -Wno-macro-redefined \
  src/simkl.c src/trakt.c src/js.c src/traktscrobble.c src/metaep.c tests/simkl_anime_cw.c \
  -o "$task_tmp/test" -lpthread
"$task_tmp/test" | tee "$task_tmp/log"
rg -q 'activities unchanged, cached playback/shows/anime bodies' "$task_tmp/log"
rg -q 'id=tt0388629 next=1123: candidate' "$task_tmp/log"
rg -q 'anime watching: 1 entries, 1 candidates, 0 dropped' "$task_tmp/log"
rg -q 'dropped \(missing next_to_watch\)' "$task_tmp/log"
