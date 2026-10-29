#!/bin/bash
# Compile the touch collection implementation without linking or network access.
set -euo pipefail
cd "$(dirname "$0")/.."
cc -fsyntax-only -DNV_TOUCH_UI src/vertudo.c \
  -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 -Wno-deprecated-declarations
