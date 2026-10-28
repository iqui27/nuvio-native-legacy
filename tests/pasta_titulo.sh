#!/bin/bash
set -eu
cd "$(dirname "$0")/.."
cc tests/pasta_titulo.c src/fontefalha.c -Isrc -o "${TMPDIR:-/tmp}/nuvio-pasta-titulo" \
  -O1 -g -ffunction-sections -fdata-sections -Wl,-dead_strip \
  -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -Wno-deprecated-declarations -Wno-macro-redefined
"${TMPDIR:-/tmp}/nuvio-pasta-titulo"
