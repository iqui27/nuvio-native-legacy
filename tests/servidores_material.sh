#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
cc tests/servidores_material.c src/plrui.c -Isrc \
  -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -ffunction-sections -fdata-sections -Wl,-dead_strip -Wl,-undefined,dynamic_lookup \
  -Wno-deprecated-declarations -Wno-macro-redefined -o "$work/test"
"$work/test"
