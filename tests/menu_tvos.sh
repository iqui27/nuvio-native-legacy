#!/bin/bash
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
set -eu
cd "$(dirname "$0")/.."
for mode in desktop touch; do
  flags=()
  if [ "$mode" = touch ]; then flags+=(-DNV_TOUCH_UI); fi
  cc tests/menu_tvos.c "${flags[@]}" -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
    -Wno-deprecated-declarations -Wno-macro-redefined \
    -Wl,-undefined,dynamic_lookup -o ${TMPDIR:-/tmp}/nuvio-menu-tvos
  ${TMPDIR:-/tmp}/nuvio-menu-tvos
done
