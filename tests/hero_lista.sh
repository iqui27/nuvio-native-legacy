#!/bin/bash
set -eu
cd "$(dirname "$0")/.."
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
PATH="/home/ubuntu/nuvio-qa-21/global/harness/bin:${PATH}"
cc tests/hero_lista.c -Isrc -o "${TMPDIR}/nuvio-hero-lista" -O1 -g \
  -lSDL2 -lm -lpthread -ldl -Wno-macro-redefined 2>/dev/null || \
  cc tests/hero_lista.c -Isrc -o "${TMPDIR}/nuvio-hero-lista" -O1 -g \
  -I/usr/include/SDL2 -lSDL2 -lm -lpthread -ldl
"${TMPDIR}/nuvio-hero-lista"
