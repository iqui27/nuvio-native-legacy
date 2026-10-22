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
cc -std=gnu11 -O1 -g -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -ffunction-sections -fdata-sections tests/juntos_cliente.c src/juntosfonte.c \
  -Wl,-dead_strip -o "$work/client"
"$work/client"
bash tests/juntossync.sh
bash tests/juntos_sim.sh
bash tests/juntosfonte.sh
bash tests/video_android_cacheboost.sh
bash tests/tpk-escolha.sh
NV_JUNTOS_TEST=1 bash tests/video_pausa_lg.sh
node tests/tizen-avplay-contract.cjs
bash tests/juntos_e2e.sh
