#!/bin/bash
# Focused Watch Together contract plus existing protocol, engine and source checks.
set -euo pipefail
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-video-juntos.XXXXXX")
trap 'rm -rf "$work"' EXIT
cc -std=gnu11 -O1 -g -Wall -Wextra -Wno-unused-function -Wno-unused-variable -Wno-deprecated-declarations -Isrc \
  -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -ffunction-sections -fdata-sections src/video.c src/video_modo.c tests/video_juntos.c \
  -L/opt/homebrew/lib -lSDL2 -Wl,-dead_strip -o "$work/test"
"$work/test"
bash tests/juntossync.sh
bash tests/juntos_sim.sh
bash tests/juntosfonte.sh
