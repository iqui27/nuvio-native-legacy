#!/bin/bash
# Exercita o scheduler real de src/app.c com resolver Stalker bloqueavel.
set -eu
cd "$(dirname "$0")/.."
cc src/cwordem.c tests/stalker_scheduler.c -Isrc \
  -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -o /tmp/nuvio-stalker-scheduler -O0 -g -Wall -Wextra \
  -Wno-deprecated-declarations -ffunction-sections -fdata-sections \
  -Wl,-dead_strip \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz \
  -framework OpenGL -pthread
timeout 5 /tmp/nuvio-stalker-scheduler
