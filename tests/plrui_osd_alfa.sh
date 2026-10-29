#!/bin/bash
set -eu
cd "$(dirname "$0")/.."
B=$(mktemp -d); trap 'rm -rf "$B"' EXIT
cc tests/plrui_osd_alfa.c src/plrui.c -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -Wno-deprecated-declarations -Wno-macro-redefined -Wl,-undefined,dynamic_lookup -o "$B/t"
"$B/t"
