#!/bin/bash
set -eu
cd "$(dirname "$0")/.."
bin=$(mktemp "${TMPDIR:-/tmp}/nuvio-addons-sync-ui.XXXXXXXX")
trap 'rm -f "$bin"' EXIT
cc -O1 -g -ffunction-sections -fdata-sections -Wl,-dead_strip \
  -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -Wno-deprecated-declarations -Wno-macro-redefined \
  src/addonsui.c tests/addons_sync_ui.c -o "$bin"
"$bin"
