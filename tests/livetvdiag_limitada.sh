#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
cc tests/livetvdiag_limitada.c -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -O1 -ffunction-sections -fdata-sections -Wl,-dead_strip -lm -lpthread -o "$work/test"
"$work/test"
