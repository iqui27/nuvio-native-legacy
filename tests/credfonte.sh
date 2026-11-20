#!/bin/bash
set -eu
cd "$(dirname "$0")/.."
cc -Isrc tests/credfonte.c src/credfonte.c src/rede.c src/redeurl.c -o "${TMPDIR:-/tmp}/nuvio-credfonte" -I/opt/homebrew/include -Wno-deprecated-declarations -Wno-macro-redefined
"${TMPDIR:-/tmp}/nuvio-credfonte"
