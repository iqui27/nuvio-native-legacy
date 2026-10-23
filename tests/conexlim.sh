#!/bin/bash
# Offline-only account policy and real HTTP connection-count regression.
set -euo pipefail
cd "$(dirname "$0")/.."
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
cc src/xtream.c src/js.c tests/xtream_conexlim.c -Isrc -Itests \
  -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 -O1 -lpthread -o "$work/account"
"$work/account"
bash tests/xtream.sh
python3 tests/conexlim.py
