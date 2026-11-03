#!/bin/bash
# Offline parser/UI regression, including dashboard material and identity draws.
set -euo pipefail
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-aio-visual.XXXXXX")
trap 'rm -rf "$work"' EXIT
cc tests/aiostreams.c src/aiostreams.c src/js.c src/stream_parse.c src/fontefiltro.c src/linguas.c src/badges.c -Isrc -o "$work/parser" -O1 -g \
  -ffunction-sections -fdata-sections -Wl,-dead_strip \
  -I/opt/homebrew/include -I/opt/homebrew/include/SDL2
"$work/parser"
cc tests/aiostreams_ui.c src/aiostreams.c src/js.c src/jsw.c src/qr.c src/focus.c -Isrc -o "$work/ui" -O1 -g \
  -ffunction-sections -fdata-sections -Wl,-dead_strip \
  -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 -L/opt/homebrew/lib -lSDL2
"$work/ui"
