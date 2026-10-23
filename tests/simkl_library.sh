#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
python3 tests/fixtures/simkl_library.py "$tmp/fixture.json"
cc tests/simkl_library.c tests/simkl_library_grid.c src/js.c -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 -O1 -ffunction-sections -fdata-sections -Wl,-dead_strip -lpthread -o "$tmp/test"
"$tmp/test" "$tmp/fixture.json"
