#!/bin/bash
# Offline regression for #610. No account, device or HTTP server required.
set -euo pipefail
cd "$(dirname "$0")/.."
dir=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-trakt610.XXXXXX")
trap 'rm -rf "$dir"' EXIT
cc tests/trakt610_lista.c src/js.c -Isrc \
  -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -o "$dir/lista" -O1 -g -Wall -Wno-deprecated-declarations \
  -ffunction-sections -fdata-sections -Wl,-dead_strip -lpthread
"$dir/lista"
cc tests/trakt610_auth.c src/credfio.c src/js.c src/jsw.c -Isrc \
  -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -o "$dir/auth" -O1 -g -Wno-deprecated-declarations -Wno-macro-redefined -lpthread
"$dir/auth"
python3 tests/trakt610_android.py
