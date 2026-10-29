#!/bin/bash
# #483: Tracks fora da janela da sonda (Void 400 KB antes dele). Ver tests/mkv_tracks_relayout.c.
#   bash tests/mkv_tracks_relayout.sh
set -eu
cd "$(dirname "$0")/.."
. tests/mkvcore_flag.sh
cc -Isrc tests/mkv_tracks_relayout.c src/mkv.c \
  -o "${TMPDIR:-/tmp}/nuvio-mkv-tracks-relayout" -O1 -g -Wall \
  -I/opt/homebrew/include -Wno-deprecated-declarations ${MKVCORE_CC[@]+"${MKVCORE_CC[@]}"}
"${TMPDIR:-/tmp}/nuvio-mkv-tracks-relayout"
