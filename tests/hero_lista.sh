#!/bin/bash
set -eu
cd "$(dirname "$0")/.."
export TMPDIR="${TMPDIR:-/tmp}"
PATH="/home/ubuntu/nuvio-qa-21/global/harness/bin:${PATH}"
sources=()
for source in src/*.c src/dts/*.c; do
  case "$source" in src/main.c|src/ajustes.c) continue;; esac
  sources+=("$source")
done
cc "${sources[@]}" tests/hero_lista.c -Isrc -o "${TMPDIR%/}/nuvio-hero-lista" \
  -O1 -g -lSDL2 -lm -lpthread -ldl -Wno-macro-redefined 2>/dev/null || exit 1
"${TMPDIR%/}/nuvio-hero-lista"
