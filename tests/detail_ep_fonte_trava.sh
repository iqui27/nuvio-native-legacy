#!/bin/bash
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
# Issue #513 (P1): the sheet's latched episode is not cwTocar. A later
# Continue Watching / agenda write must not replace it, and a title-level
# sheet must not inherit one. See tests/detail_ep_fonte_trava.c.
#
#   bash tests/detail_ep_fonte_trava.sh
set -eu
cd "$(dirname "$0")/.."
# Catch reads of the episode outputs before the request getter initializes them.
cc -fsyntax-only -O1 -Werror=uninitialized -Isrc \
  -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -Wno-deprecated-declarations -Wno-macro-redefined src/app.c
sources=()
for source in src/*.c src/dts/*.c; do
  case "$source" in src/main.c|src/app.c) continue;; esac  # app.c comes in via the test
  sources+=("$source")
done
cc "${sources[@]}" tests/detail_ep_fonte_trava.c -Isrc -o ${TMPDIR:-/tmp}/nuvio-detail-eptrava-tests \
  -O1 -g -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wno-deprecated-declarations -Wno-macro-redefined
${TMPDIR:-/tmp}/nuvio-detail-eptrava-tests
