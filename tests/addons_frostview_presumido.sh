#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-frostview-presumido.XXXXXX")
trap 'rm -rf "$work"' EXIT
cc -std=gnu11 -O1 -g -pthread -Isrc -ffunction-sections -fdata-sections \
  -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 -Wl,-dead_strip \
  tests/addons_frostview_presumido.c src/js.c src/jsw.c src/watchstate.c \
  src/addonstats.c src/fontefalha.c src/idcanal.c src/linguas.c src/legextras.c -o "$work/test"
"$work/test"
