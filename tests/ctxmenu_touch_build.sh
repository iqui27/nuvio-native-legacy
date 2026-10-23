#!/bin/bash
# Session reset must compile after removing the touch style picker state.
set -eu
cd "$(dirname "$0")/.."
cc -fsyntax-only -DNV_TOUCH_UI -Isrc \
  -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -Wno-deprecated-declarations src/ctxmenu.c src/home.c
