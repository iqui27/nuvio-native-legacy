#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
task_tmp=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-simkl-anime-XXXXXX")
trap 'rm -rf "$task_tmp"' EXIT
trakt_extra=src/trakt.c
if [ "$(uname -s)" != Darwin ]; then trakt_extra=tests/simkl_trakt_link_stub.c; fi
ld_extra=(-Wl,-dead_strip)
if [ "$(uname -s)" != Darwin ]; then ld_extra=(); fi
inc="-Isrc"
if [ -d /opt/homebrew/include ]; then inc="$inc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2"; fi
cc -DSMK_BIB_FRESCA=0 -O1 -g -Wall -Wextra -ffunction-sections -fdata-sections "${ld_extra[@]}" $inc \
  -Wno-deprecated-declarations -Wno-macro-redefined \
  src/simkl.c src/simklbib.c src/js.c src/traktscrobble.c src/metaep.c "$trakt_extra" tests/simkl_anime_cw.c \
  -o "$task_tmp/test" -lpthread
"$task_tmp/test" | tee "$task_tmp/log"
rg -q 'library: activities unchanged, saved copy is current' "$task_tmp/log"
rg -q 'resume: playback unchanged, cached playback' "$task_tmp/log"
rg -q 'id=tt0388629 next=1123: candidate' "$task_tmp/log"
rg -q 'anime watching: 1 entries, 1 candidates, 0 dropped' "$task_tmp/log"
rg -q 'dropped \(missing next_to_watch\)' "$task_tmp/log"
