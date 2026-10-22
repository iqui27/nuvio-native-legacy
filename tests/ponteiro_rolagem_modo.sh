#!/bin/bash
# Registered touch scroll captures a drag only in Mobile mode (TV keeps arrows).
#   bash tests/ponteiro_rolagem_modo.sh
set -eu
cd "$(dirname "$0")/.."
bin=$(mktemp "${TMPDIR:-/tmp}/nuvio-ponteiro-rolagem.XXXXXX")
trap 'rm -f "$bin"' EXIT
cc tests/ponteiro_rolagem_modo.c src/ponteiro.c src/layout.c -Isrc -o "$bin" -O1 -lm \
  -DNV_TOUCH_UI -DNV_LINUX_DESKTOP \
  -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -Wno-deprecated-declarations -Wno-macro-redefined
"$bin"
