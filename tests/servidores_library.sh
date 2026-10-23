#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-server-library.XXXXXX")
trap 'rm -rf "$work"' EXIT
cc tests/biblioteca_servidores.c src/salvos.c src/focus.c -Isrc \
  -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 -O1 -g \
  -ffunction-sections -fdata-sections -Wl,-dead_strip -o "$work/test"
"$work/test"
