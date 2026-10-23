#!/bin/bash
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
set -eu
cd "$(dirname "$0")/.."
if ! command -v pkg-config >/dev/null 2>&1 || ! pkg-config --exists libass; then
  echo "ass_libass: libass ausente (teste ignorado)"
  exit 0
fi
cc tests/ass_libass.c -o ${TMPDIR:-/tmp}/nuvio-ass-libass-test \
  $(pkg-config --cflags --libs libass)
${TMPDIR:-/tmp}/nuvio-ass-libass-test tests/fixtures/ass/completo.ass
