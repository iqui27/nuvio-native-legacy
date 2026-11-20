#!/bin/bash
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
mkdir -p "$TMPDIR"
# GPU adaptive policy offline tests (src/gpunivel.h). See docs/tv-auto-performance.md.
set -eu
cd "$(dirname "$0")/.."
cc src/gpunivel.c src/perfiltv.c tests/gpunivel.c -Isrc -o "${TMPDIR}/nuvio-gpunivel-teste" \
  -DNV_GPUN_TESTE -DNV_WEBOS -O1 -g -Wall -Wextra -framework OpenGL -Wno-deprecated-declarations \
  -I/opt/homebrew/include -I/opt/homebrew/include/SDL2
for modo in gpu1080 combinations mali-light; do
  "${TMPDIR}/nuvio-gpunivel-teste" "$modo"
done
"${TMPDIR}/nuvio-gpunivel-teste"
