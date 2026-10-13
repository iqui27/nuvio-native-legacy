#!/bin/bash
set -eu
cd "$(dirname "$0")/.."
tmp="$(mktemp -d "${TMPDIR:-/tmp}/nuvio-listas-wasm-XXXXXX")"
trap 'rm -rf "$tmp"' EXIT
cc -D__EMSCRIPTEN__ -ffunction-sections -fdata-sections src/listasescrita.c tests/listas_wasm.c \
  -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 -Wl,-dead_strip -o "$tmp/test"
"$tmp/test"
