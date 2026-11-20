#!/bin/bash
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
set -eu
cd "$(dirname "$0")/.."
cc tests/sidebaratv_geometry.c -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -Wno-deprecated-declarations -Wno-macro-redefined \
  -Wl,-undefined,dynamic_lookup -o ${TMPDIR:-/tmp}/nuvio-sidebaratv-geometry
${TMPDIR:-/tmp}/nuvio-sidebaratv-geometry
