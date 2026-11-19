#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
cc -c src/livetvdiag.c -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -O1 -Wall -Wextra -o "$work/livetvdiag.o"
echo 'PASS: host compilation of src/livetvdiag.c'
